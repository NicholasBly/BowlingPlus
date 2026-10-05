// Port of src/Log.mm.
//  - events: BowlingPlus's own messages, game-state changes, network checks (ring buffer, as on iOS)
//  - console: on Android the game's output (Unity Debug.Log, Photon, SDKs) goes to logcat, so "Copy log"
//    reads this app's own logcat lines (BP.java) instead of capturing stdout/stderr like iOS.
//  - the connection test: DNS + raw TCP here (POSIX, same as iOS), the HTTP checks in Java.
#include "BFShared.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>

static std::mutex sLock;
static std::deque<Str> sEvents;
static Str sLastEvent;
static int sEventRepeat = 0;
static double sT0 = 0;
static const size_t kMaxEvents = 500;

static Str Stamp() {
    if (sT0 == 0) sT0 = BFNow();
    return Fmt("%7.2f", BFNow() - sT0);
}

static void Push(const Str &line) {
    sEvents.push_back(line);
    while (sEvents.size() > kMaxEvents) sEvents.pop_front();
}

void BFLogEvent(const Str &source, const Str &msg) {
    if (msg.empty()) return;
    std::lock_guard<std::mutex> g(sLock);
    Str key = source + msg;
    if (key == sLastEvent) {
        sEventRepeat++;                        // collapse identical repeats
    } else {
        if (sEventRepeat > 0) Push(Fmt("        (previous line repeated %d more times)", sEventRepeat));
        sEventRepeat = 0;
        sLastEvent = key;
        Push(Fmt("%s [%s] %s", Stamp(), source, msg));
    }
}

Str BFLogText(void) {
    std::lock_guard<std::mutex> g(sLock);
    Str ev;
    for (const Str &l : sEvents) { ev += l; ev += "\n"; }
    if (ev.empty()) ev = "(nothing yet)\n";
    if (sEventRepeat > 0) ev += Fmt("        (previous line repeated %d more times)\n", sEventRepeat);
    return "--- events (seconds since launch) ---\n" + ev;
}

// ---- connection test: DNS, raw TCP to the game server, plain-HTTP API ----
static double MsSince(double t) { return (BFNow() - t) * 1000.0; }

static Str AddrString(const struct sockaddr *sa) {
    char buf[INET6_ADDRSTRLEN] = { 0 };
    if (sa->sa_family == AF_INET) inet_ntop(AF_INET, &((const struct sockaddr_in *)sa)->sin_addr, buf, sizeof(buf));
    else if (sa->sa_family == AF_INET6) inet_ntop(AF_INET6, &((const struct sockaddr_in6 *)sa)->sin6_addr, buf, sizeof(buf));
    return buf;
}

static Str TcpConnect(const struct sockaddr *sa, socklen_t len, int port, int timeoutMs) {
    struct sockaddr_storage ss;
    memcpy(&ss, sa, len);
    if (ss.ss_family == AF_INET) ((struct sockaddr_in *)&ss)->sin_port = htons(port);
    else ((struct sockaddr_in6 *)&ss)->sin6_port = htons(port);
    int fd = socket(ss.ss_family, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return Fmt("socket failed: %s", strerror(errno));
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    double t = BFNow();
    Str result;
    if (connect(fd, (struct sockaddr *)&ss, len) == 0) {
        result = Fmt("connected in %.0f ms", MsSince(t));
    } else if (errno != EINPROGRESS) {
        result = Fmt("failed right away: %s", strerror(errno));
    } else {
        struct pollfd p = { fd, POLLOUT, 0 };
        int r = poll(&p, 1, timeoutMs);
        int err = 0;
        socklen_t el = sizeof(err);
        if (r == 0) result = Fmt("no answer after %d ms (timed out)", timeoutMs);
        else if (r < 0 || getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &el) != 0) result = Fmt("poll error: %s", strerror(errno));
        else if (err) result = Fmt("failed after %.0f ms: %s", MsSince(t), strerror(err));
        else result = Fmt("connected in %.0f ms", MsSince(t));
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
    double t = BFNow();
    int rc = getaddrinfo(host, NULL, &hints, &res);
    double ms = MsSince(t);
    if (rc != 0) {
        BFLogEvent("net", Fmt("DNS %s: FAILED after %.0f ms (%d %s)", host, ms, rc, gai_strerror(rc)));
        return;
    }
    std::vector<Str> addrs;
    struct addrinfo *v4 = NULL, *v6 = NULL;
    for (struct addrinfo *p = res; p; p = p->ai_next) {
        addrs.push_back(AddrString(p->ai_addr));
        if (p->ai_family == AF_INET && !v4) v4 = p;
        if (p->ai_family == AF_INET6 && !v6) v6 = p;
    }
    BFLogEvent("net", Fmt("DNS %s: %.0f ms -> %s", host, ms, StrJoin(addrs, ", ")));
    if (tcpPort > 0) {
        if (v4) BFLogEvent("net", Fmt("TCP %s:%d via IPv4 %s: %s", host, tcpPort, AddrString(v4->ai_addr), TcpConnect(v4->ai_addr, v4->ai_addrlen, tcpPort, 6000)));
        if (v6) BFLogEvent("net", Fmt("TCP %s:%d via IPv6 %s: %s", host, tcpPort, AddrString(v6->ai_addr), TcpConnect(v6->ai_addr, v6->ai_addrlen, tcpPort, 6000)));
    }
    freeaddrinfo(res);
}

void BFHttpTest(const char *url);   // Jni.cpp (Java HttpURLConnection)

void BFNetTest(void) {
    static std::atomic<bool> running{ false };
    if (running.exchange(true)) return;   // one test at a time
    std::thread([] {
        BFLogEvent("net", "--- connection test start ---");
        TestHost("s1.wannaplay.studio", 4055);      // the game's master server (Photon)
        TestHost("w1.wannaplay.studio", 4056);      // game server it redirects to
        TestHost("p1.wannaplay.studio", 4056);      // relay
        TestHost("51.20.120.110", 4056);            // relay (raw IPv4)
        TestHost("api.wannaplay.studio", 80);       // the game's plain-HTTP API
        TestHost("clickhouse.wannaplay.studio", 0);
        TestHost("google.com", 443);                // control: should always work (apple.com on iOS)
        BFHttpTest("http://api.wannaplay.studio/scripts");
        BFHttpTest("https://wannaplay.studio/privacy-policy/");
        BFLogEvent("net", "--- connection test done ---");
        running = false;
    }).detach();
}
