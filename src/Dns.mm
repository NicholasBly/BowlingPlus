#import <Foundation/Foundation.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach/mach.h>
#include <dlfcn.h>
#include <netdb.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#import "BFShared.h"

// Game server over IPv4
// ---------------------
// Photon (the game's network library) resolves the server name and, in
// IPhotonSocket.GetIpAddress, returns the FIRST IPv6 address it finds; IPv4 is only used when
// the DNS answer has no IPv6 at all. The game's servers don't answer on IPv6 (checked: TCP to
// s1.wannaplay.studio over IPv6 times out, over IPv4 it answers), so with DNS that returns IPv6
// addresses (NextDNS does) every connection attempt hangs, times out and retries forever:
// "disconnected from the server" / endless loading.
//
// Fix: the game's code (UnityFramework) calls the system getaddrinfo(). We point its import slot at
// our wrapper (the "fishhook" technique: only the game's data pages are changed, never code, so it
// works on a non-jailbroken phone). For *.wannaplay.studio names asked with "any address family" we
// ask for IPv4 only. If a name has no IPv4 address, the normal lookup runs unchanged.

typedef int (*getaddrinfo_t)(const char *, const char *, const struct addrinfo *, struct addrinfo **);
static getaddrinfo_t sRealGetaddrinfo;
static int sRebound = 0;
static pthread_mutex_t sHostsLock = PTHREAD_MUTEX_INITIALIZER;
static NSMutableSet<NSString *> *sLoggedHosts;

static bool IsGameHost(const char *node) {
    size_t n = strlen(node);
    static const char *suffixes[] = { ".wannaplay.studio", ".spareball.com" };
    for (const char *s : suffixes) {
        size_t k = strlen(s);
        if (n > k && strcasecmp(node + n - k, s) == 0) return true;
    }
    return strcasecmp(node, "wannaplay.studio") == 0;
}

static void LogOnce(const char *node, NSString *what) {
    NSString *host = [NSString stringWithUTF8String:node];
    pthread_mutex_lock(&sHostsLock);
    if (!sLoggedHosts) sLoggedHosts = [NSMutableSet set];
    bool first = ![sLoggedHosts containsObject:host];
    [sLoggedHosts addObject:host];
    pthread_mutex_unlock(&sHostsLock);
    if (first) BFLogEvent(@"dns", [NSString stringWithFormat:@"game lookup %@: %@", host, what]);
}

static int BF_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
    if (!sRealGetaddrinfo) sRealGetaddrinfo = (getaddrinfo_t)dlsym(RTLD_DEFAULT, "getaddrinfo");
    if (gBF.gameIPv4 && !gBFSafeMode && node && IsGameHost(node) && (!hints || hints->ai_family == AF_UNSPEC)) {
        struct addrinfo h;
        memset(&h, 0, sizeof(h));
        if (hints) h = *hints;
        h.ai_family = AF_INET;
        int rc = sRealGetaddrinfo(node, service, &h, res);
        if (rc == 0 && *res) {
            LogOnce(node, @"using IPv4 only");
            return 0;
        }
        LogOnce(node, [NSString stringWithFormat:@"no IPv4 address (%d), normal lookup", rc]);
    }
    return sRealGetaddrinfo(node, service, hints, res);
}

// Point every import slot for `symbol` in one loaded image at `replacement`.
static int Rebind(const struct mach_header_64 *mh, intptr_t slide, const char *symbol, void *replacement) {
    const struct segment_command_64 *linkedit = NULL;
    const struct symtab_command *symtab = NULL;
    const struct dysymtab_command *dysymtab = NULL;
    const uint8_t *cmd = (const uint8_t *)(mh + 1);
    for (uint32_t i = 0; i < mh->ncmds; i++) {
        const struct load_command *lc = (const struct load_command *)cmd;
        if (lc->cmd == LC_SEGMENT_64 && strcmp(((const struct segment_command_64 *)lc)->segname, SEG_LINKEDIT) == 0)
            linkedit = (const struct segment_command_64 *)lc;
        else if (lc->cmd == LC_SYMTAB) symtab = (const struct symtab_command *)lc;
        else if (lc->cmd == LC_DYSYMTAB) dysymtab = (const struct dysymtab_command *)lc;
        cmd += lc->cmdsize;
    }
    if (!linkedit || !symtab || !dysymtab || !dysymtab->nindirectsyms) return 0;
    uintptr_t base = (uintptr_t)slide + linkedit->vmaddr - linkedit->fileoff;
    const struct nlist_64 *syms = (const struct nlist_64 *)(base + symtab->symoff);
    const char *strs = (const char *)(base + symtab->stroff);
    const uint32_t *indirect = (const uint32_t *)(base + dysymtab->indirectsymoff);
    int count = 0;
    cmd = (const uint8_t *)(mh + 1);
    for (uint32_t i = 0; i < mh->ncmds; i++) {
        const struct load_command *lc = (const struct load_command *)cmd;
        cmd += lc->cmdsize;
        if (lc->cmd != LC_SEGMENT_64) continue;
        const struct segment_command_64 *seg = (const struct segment_command_64 *)lc;
        if (strcmp(seg->segname, "__DATA") != 0 && strcmp(seg->segname, "__DATA_CONST") != 0) continue;
        const struct section_64 *sect = (const struct section_64 *)(seg + 1);
        for (uint32_t s = 0; s < seg->nsects; s++, sect++) {
            uint32_t type = sect->flags & SECTION_TYPE;
            if (type != S_LAZY_SYMBOL_POINTERS && type != S_NON_LAZY_SYMBOL_POINTERS) continue;
            void **slots = (void **)((uintptr_t)slide + sect->addr);
            uint32_t n = (uint32_t)(sect->size / sizeof(void *));
            for (uint32_t k = 0; k < n; k++) {
                uint32_t idx = indirect[sect->reserved1 + k];
                if (idx & (INDIRECT_SYMBOL_ABS | INDIRECT_SYMBOL_LOCAL)) continue;
                if (idx >= symtab->nsyms) continue;
                uint32_t strx = syms[idx].n_un.n_strx;
                if (strx >= symtab->strsize || strcmp(strs + strx, symbol) != 0) continue;
                vm_address_t page = (vm_address_t)&slots[k] & ~(vm_address_t)(vm_page_size - 1);
                if (strcmp(seg->segname, "__DATA_CONST") == 0)
                    vm_protect(mach_task_self(), page, vm_page_size, false, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
                slots[k] = replacement;
                count++;
            }
        }
    }
    return count;
}

void BFDnsHookStart(void) {
    static bool done = false;
    if (done) return;
    done = true;
    sRealGetaddrinfo = (getaddrinfo_t)dlsym(RTLD_DEFAULT, "getaddrinfo");
    if (!sRealGetaddrinfo) return;
    for (uint32_t i = 0; i < _dyld_image_count(); i++) {
        const char *name = _dyld_get_image_name(i);
        if (!name || !strstr(name, "/UnityFramework.framework/UnityFramework")) continue;
        const struct mach_header *mh = _dyld_get_image_header(i);
        if (!mh || mh->magic != MH_MAGIC_64) continue;
        sRebound = Rebind((const struct mach_header_64 *)mh, _dyld_get_image_vmaddr_slide(i), "_getaddrinfo", (void *)BF_getaddrinfo);
    }
    BFLogEvent(@"dns", [NSString stringWithFormat:@"game DNS filter installed (%d slot%s)", sRebound, sRebound == 1 ? "" : "s"]);
}

int BFDnsHookSlots(void) { return sRebound; }
