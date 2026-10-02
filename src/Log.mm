#import <Foundation/Foundation.h>
#import <Network/Network.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#import "BFShared.h"

// Diagnostics log
// ---------------
// Two ring buffers, so chatty game output can't push out the useful lines:
//  - events:  BowlingPlus's own messages, game-state changes, network checks
//  - console: whatever the game prints (Unity's Debug.Log output, Photon, SDKs), captured
//             by pointing stdout/stderr at a pipe. The pipe is non-blocking on the game's side,
//             so if we ever fall behind, lines get dropped instead of freezing the game.
// "Copy log" in the shake menu copies both.

static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static NSMutableArray<NSString *> *sEvents, *sConsole;
static NSString *sLastEvent;
static int sEventRepeat = 0;
static CFAbsoluteTime sT0 = 0;
static const NSUInteger kMaxEvents = 500, kMaxConsole = 400;

static NSString *Stamp(void) {
    if (sT0 == 0) sT0 = CFAbsoluteTimeGetCurrent();
    return [NSString stringWithFormat:@"%7.2f", CFAbsoluteTimeGetCurrent() - sT0];
}

static void Push(NSMutableArray<NSString *> *buf, NSUInteger max, NSString *line) {
    [buf addObject:line];
    if (buf.count > max) [buf removeObjectsInRange:NSMakeRange(0, buf.count - max)];
}

void BFLogEvent(NSString *source, NSString *msg) {
    if (!msg.length) return;
    pthread_mutex_lock(&sLock);
    if (!sEvents) sEvents = [NSMutableArray array];
    NSString *key = [source stringByAppendingString:msg];
    if ([key isEqualToString:sLastEvent]) {
        sEventRepeat++;                        // collapse identical repeats
    } else {
        if (sEventRepeat > 0) Push(sEvents, kMaxEvents, [NSString stringWithFormat:@"        (previous line repeated %d more times)", sEventRepeat]);
        sEventRepeat = 0;
        sLastEvent = key;
        Push(sEvents, kMaxEvents, [NSString stringWithFormat:@"%@ [%@] %@", Stamp(), source, msg]);
    }
    pthread_mutex_unlock(&sLock);
}

static void LogConsoleLine(NSString *line) {
    if (!line.length || [line containsString:@"[BowlingPlus]"]) return;   // our own lines are in the event log already
    pthread_mutex_lock(&sLock);
    if (!sConsole) sConsole = [NSMutableArray array];
    Push(sConsole, kMaxConsole, [NSString stringWithFormat:@"%@ %@", Stamp(), line]);
    pthread_mutex_unlock(&sLock);
}

NSString *BFLogText(void) {
    pthread_mutex_lock(&sLock);
    NSString *ev = sEvents.count ? [sEvents componentsJoinedByString:@"\n"] : @"(nothing yet)";
    if (sEventRepeat > 0) ev = [ev stringByAppendingFormat:@"\n        (previous line repeated %d more times)", sEventRepeat];
    NSString *co = sConsole.count ? [sConsole componentsJoinedByString:@"\n"] : @"(nothing captured)";
    pthread_mutex_unlock(&sLock);
    return [NSString stringWithFormat:@"--- events (seconds since launch) ---\n%@\n\n--- game console output (last %lu lines) ---\n%@\n", ev, (unsigned long)kMaxConsole, co];
}

// ---- console capture ----
static int sOrigErr = -1;

static void *ReaderThread(void *arg) {
    int fd = (int)(intptr_t)arg;
    char buf[8192];
    NSMutableData *pending = [NSMutableData data];
    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            break;
        }
        if (sOrigErr >= 0) (void)write(sOrigErr, buf, (size_t)n);   // still shows up in Xcode / Console
        @autoreleasepool {
            [pending appendBytes:buf length:(NSUInteger)n];
            const char *p = (const char *)pending.bytes;
            NSUInteger len = pending.length, start = 0;
            for (NSUInteger i = 0; i < len; i++) {
                if (p[i] != '\n') continue;
                NSString *line = [[NSString alloc] initWithBytes:p + start length:i - start encoding:NSUTF8StringEncoding];
                LogConsoleLine([line stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]]);
                start = i + 1;
            }
            if (start > 0) [pending replaceBytesInRange:NSMakeRange(0, start) withBytes:NULL length:0];
            if (pending.length > 16384) [pending setLength:0];   // a runaway line with no newline
        }
    }
    return NULL;
}

void BFLogCaptureStart(void) {
    static bool started = false;
    if (started) return;
    started = true;
    Stamp();
    int fds[2];
    if (pipe(fds) != 0) return;
    sOrigErr = dup(STDERR_FILENO);
    fcntl(fds[1], F_SETFL, fcntl(fds[1], F_GETFL) | O_NONBLOCK);   // never block the game
    fcntl(fds[1], F_SETNOSIGPIPE, 1);
    setvbuf(stdout, NULL, _IOLBF, 0);
    dup2(fds[1], STDOUT_FILENO);
    dup2(fds[1], STDERR_FILENO);
    close(fds[1]);
    pthread_t t;
    if (pthread_create(&t, NULL, ReaderThread, (void *)(intptr_t)fds[0]) == 0) pthread_detach(t);
}

// ---- network path (Wi-Fi/cellular, IPv4/IPv6, DNS) ----
static nw_path_monitor_t sMonitor;

void BFNetMonitorStart(void) {
    if (sMonitor) return;
    sMonitor = nw_path_monitor_create();
    nw_path_monitor_set_queue(sMonitor, dispatch_get_global_queue(QOS_CLASS_UTILITY, 0));
    nw_path_monitor_set_update_handler(sMonitor, ^(nw_path_t path) {
        nw_path_status_t st = nw_path_get_status(path);
        const char *status = st == nw_path_status_satisfied ? "online" : st == nw_path_status_unsatisfied ? "offline" :
                             st == nw_path_status_satisfiable ? "satisfiable" : "invalid";
        NSMutableArray *ifs = [NSMutableArray array];
        if (nw_path_uses_interface_type(path, nw_interface_type_wifi)) [ifs addObject:@"wifi"];
        if (nw_path_uses_interface_type(path, nw_interface_type_cellular)) [ifs addObject:@"cellular"];
        if (nw_path_uses_interface_type(path, nw_interface_type_wired)) [ifs addObject:@"wired"];
        if (nw_path_uses_interface_type(path, nw_interface_type_other)) [ifs addObject:@"other(VPN?)"];
        BFLogEvent(@"net", [NSString stringWithFormat:@"path %s via %@ | ipv4=%d ipv6=%d dns=%d expensive=%d constrained=%d",
                            status, ifs.count ? [ifs componentsJoinedByString:@"+"] : @"none",
                            nw_path_has_ipv4(path), nw_path_has_ipv6(path), nw_path_has_dns(path),
                            nw_path_is_expensive(path), nw_path_is_constrained(path)]);
    });
    nw_path_monitor_start(sMonitor);
}

// ---- connection test: DNS, raw TCP to the game server, plain-HTTP API ----
static double MsSince(CFAbsoluteTime t) { return (CFAbsoluteTimeGetCurrent() - t) * 1000.0; }

static NSString *AddrString(const struct sockaddr *sa) {
    char buf[INET6_ADDRSTRLEN] = {0};
    if (sa->sa_family == AF_INET) inet_ntop(AF_INET, &((const struct sockaddr_in *)sa)->sin_addr, buf, sizeof(buf));
    else if (sa->sa_family == AF_INET6) inet_ntop(AF_INET6, &((const struct sockaddr_in6 *)sa)->sin6_addr, buf, sizeof(buf));
    return [NSString stringWithUTF8String:buf];
}

static NSString *TcpConnect(const struct sockaddr *sa, socklen_t len, int port, int timeoutMs) {
    struct sockaddr_storage ss;
    memcpy(&ss, sa, len);
    if (ss.ss_family == AF_INET) ((struct sockaddr_in *)&ss)->sin_port = htons(port);
    else ((struct sockaddr_in6 *)&ss)->sin6_port = htons(port);
    int fd = socket(ss.ss_family, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return [NSString stringWithFormat:@"socket failed: %s", strerror(errno)];
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    CFAbsoluteTime t = CFAbsoluteTimeGetCurrent();
    NSString *result;
    if (connect(fd, (struct sockaddr *)&ss, len) == 0) {
        result = [NSString stringWithFormat:@"connected in %.0f ms", MsSince(t)];
    } else if (errno != EINPROGRESS) {
        result = [NSString stringWithFormat:@"failed right away: %s", strerror(errno)];
    } else {
        struct pollfd p = { fd, POLLOUT, 0 };
        int r = poll(&p, 1, timeoutMs);
        int err = 0;
        socklen_t el = sizeof(err);
        if (r == 0) result = [NSString stringWithFormat:@"no answer after %d ms (timed out)", timeoutMs];
        else if (r < 0 || getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &el) != 0) result = [NSString stringWithFormat:@"poll error: %s", strerror(errno)];
        else if (err) result = [NSString stringWithFormat:@"failed after %.0f ms: %s", MsSince(t), strerror(err)];
        else result = [NSString stringWithFormat:@"connected in %.0f ms", MsSince(t)];
    }
    close(fd);
    return result;
}

static void TestHost(const char *host, int tcpPort) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res = NULL;
    CFAbsoluteTime t = CFAbsoluteTimeGetCurrent();
    int rc = getaddrinfo(host, NULL, &hints, &res);
    double ms = MsSince(t);
    if (rc != 0) {
        BFLogEvent(@"net", [NSString stringWithFormat:@"DNS %s: FAILED after %.0f ms (%d %s)", host, ms, rc, gai_strerror(rc)]);
        return;
    }
    NSMutableArray *addrs = [NSMutableArray array];
    struct addrinfo *v4 = NULL, *v6 = NULL;
    for (struct addrinfo *p = res; p; p = p->ai_next) {
        [addrs addObject:AddrString(p->ai_addr)];
        if (p->ai_family == AF_INET && !v4) v4 = p;
        if (p->ai_family == AF_INET6 && !v6) v6 = p;
    }
    BFLogEvent(@"net", [NSString stringWithFormat:@"DNS %s: %.0f ms -> %@", host, ms, [addrs componentsJoinedByString:@", "]]);
    if (tcpPort > 0) {
        if (v4) BFLogEvent(@"net", [NSString stringWithFormat:@"TCP %s:%d via IPv4 %@: %@", host, tcpPort, AddrString(v4->ai_addr), TcpConnect(v4->ai_addr, v4->ai_addrlen, tcpPort, 6000)]);
        if (v6) BFLogEvent(@"net", [NSString stringWithFormat:@"TCP %s:%d via IPv6 %@: %@", host, tcpPort, AddrString(v6->ai_addr), TcpConnect(v6->ai_addr, v6->ai_addrlen, tcpPort, 6000)]);
    }
    freeaddrinfo(res);
}

static void TestHTTP(NSString *url) {
    NSURLSessionConfiguration *cfg = [NSURLSessionConfiguration ephemeralSessionConfiguration];
    cfg.timeoutIntervalForRequest = 10;
    cfg.timeoutIntervalForResource = 12;
    NSURLSession *session = [NSURLSession sessionWithConfiguration:cfg];
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    CFAbsoluteTime t = CFAbsoluteTimeGetCurrent();
    [[session dataTaskWithURL:[NSURL URLWithString:url] completionHandler:^(NSData *data, NSURLResponse *resp, NSError *error) {
        if (error) BFLogEvent(@"net", [NSString stringWithFormat:@"HTTP %@: FAILED after %.0f ms (%@ %ld: %@)", url, MsSince(t), error.domain, (long)error.code, error.localizedDescription]);
        else BFLogEvent(@"net", [NSString stringWithFormat:@"HTTP %@: status %ld, %lu bytes in %.0f ms", url, (long)((NSHTTPURLResponse *)resp).statusCode, (unsigned long)data.length, MsSince(t)]);
        dispatch_semaphore_signal(done);
    }] resume];
    dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 15 * NSEC_PER_SEC));
    [session finishTasksAndInvalidate];
}

void BFNetTest(void) {
    static bool running = false;
    if (running) return;
    running = true;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
        BFLogEvent(@"net", @"--- connection test start ---");
        TestHost("s1.wannaplay.studio", 4055);      // the game's master server (Photon)
        TestHost("w1.wannaplay.studio", 4056);      // game server it redirects to
        TestHost("p1.wannaplay.studio", 4056);      // relay
        TestHost("51.20.120.110", 4056);            // relay (raw IPv4)
        TestHost("api.wannaplay.studio", 80);       // the game's plain-HTTP API
        TestHost("clickhouse.wannaplay.studio", 0);
        TestHost("apple.com", 443);                 // control: should always work
        TestHTTP(@"http://api.wannaplay.studio/scripts");
        TestHTTP(@"https://wannaplay.studio/privacy-policy/");
        BFLogEvent(@"net", @"--- connection test done ---");
        running = false;
    });
}
