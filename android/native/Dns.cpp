// Port of src/Dns.mm: Game server over IPv4.
// Photon (the game's network library) returns the FIRST IPv6 address it finds for a server; the game's
// servers don't answer on IPv6, so with DNS that offers IPv6 every connection hangs. The game's code
// (libil2cpp.so) calls getaddrinfo() through its import table (GOT). We point that slot at a wrapper that asks
// for IPv4 only for *.wannaplay.studio names, exactly like the iOS "fishhook" rebinding: only data changes.
#include "BFShared.h"
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <netdb.h>
#include <pthread.h>
#include <strings.h>
#include <sys/mman.h>
#include <unistd.h>
#include <atomic>
#include <mutex>
#include <set>
#include <thread>

typedef int (*getaddrinfo_t)(const char *, const char *, const struct addrinfo *, struct addrinfo **);
static getaddrinfo_t sRealGetaddrinfo;
static std::atomic<int> sRebound{ 0 };
static std::mutex sHostsLock;
static std::set<Str> sLoggedHosts;
static std::set<Str> sAllHosts;

static bool IsGameHost(const char *node) {
    size_t n = strlen(node);
    static const char *suffixes[] = { ".wannaplay.studio", ".spareball.com" };
    for (const char *s : suffixes) {
        size_t k = strlen(s);
        if (n > k && strcasecmp(node + n - k, s) == 0) return true;
    }
    return strcasecmp(node, "wannaplay.studio") == 0;
}

static void LogOnce(const char *node, const Str &what) {
    bool first;
    {
        std::lock_guard<std::mutex> g(sHostsLock);
        first = sLoggedHosts.insert(node).second;
    }
    if (first) BFLogEvent("dns", Fmt("game lookup %s: %s", node, what));
}

static int BF_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
    if (gBF.logHosts && node) {                       // diagnostics: which servers the game contacts, each host once
        bool first;
        { std::lock_guard<std::mutex> g(sHostsLock); first = sAllHosts.insert(node).second; }
        if (first) BFLogEvent("host", node);
    }
    if (gBF.gameIPv4 && !gBFSafeMode && node && IsGameHost(node) && (!hints || hints->ai_family == AF_UNSPEC)) {
        struct addrinfo h;
        memset(&h, 0, sizeof(h));
        if (hints) h = *hints;
        h.ai_family = AF_INET;
        int rc = sRealGetaddrinfo(node, service, &h, res);
        if (rc == 0 && *res) {
            LogOnce(node, "using IPv4 only");
            return 0;
        }
        LogOnce(node, Fmt("no IPv4 address (%d), normal lookup", rc));
    }
    return sRealGetaddrinfo(node, service, hints, res);
}

// Point every import slot for `symbol` in this loaded library at `replacement`.
static int Rebind(const struct dl_phdr_info *info, const char *symbol, void *replacement) {
    ElfW(Addr) base = info->dlpi_addr;
    const ElfW(Dyn) *dyn = nullptr;
    for (int i = 0; i < info->dlpi_phnum; i++)
        if (info->dlpi_phdr[i].p_type == PT_DYNAMIC) dyn = (const ElfW(Dyn) *)(base + info->dlpi_phdr[i].p_vaddr);
    if (!dyn) return 0;
    const ElfW(Sym) *symtab = nullptr;
    const char *strtab = nullptr;
    const ElfW(Rela) *jmprel = nullptr, *rela = nullptr;
    size_t jmpsz = 0, relasz = 0;
    for (const ElfW(Dyn) *d = dyn; d->d_tag != DT_NULL; d++) {
        switch (d->d_tag) {   // bionic leaves these unrelocated: add the load bias
            case DT_SYMTAB: symtab = (const ElfW(Sym) *)(base + d->d_un.d_ptr); break;
            case DT_STRTAB: strtab = (const char *)(base + d->d_un.d_ptr); break;
            case DT_JMPREL: jmprel = (const ElfW(Rela) *)(base + d->d_un.d_ptr); break;
            case DT_PLTRELSZ: jmpsz = d->d_un.d_val; break;
            case DT_RELA: rela = (const ElfW(Rela) *)(base + d->d_un.d_ptr); break;
            case DT_RELASZ: relasz = d->d_un.d_val; break;
        }
    }
    if (!symtab || !strtab) return 0;
    long page = sysconf(_SC_PAGESIZE);
    int count = 0;
    const ElfW(Rela) *tables[2] = { jmprel, rela };
    size_t sizes[2] = { jmpsz, relasz };
    for (int t = 0; t < 2; t++) {
        if (!tables[t]) continue;
        for (size_t k = 0; k < sizes[t] / sizeof(ElfW(Rela)); k++) {
            const ElfW(Rela) &r = tables[t][k];
            uint32_t type = ELF64_R_TYPE(r.r_info), si = ELF64_R_SYM(r.r_info);
            if (!si || (type != R_AARCH64_JUMP_SLOT && type != R_AARCH64_GLOB_DAT && type != R_AARCH64_ABS64)) continue;
            if (strcmp(strtab + symtab[si].st_name, symbol) != 0) continue;
            void **slot = (void **)(base + r.r_offset);
            uintptr_t pg = (uintptr_t)slot & ~(uintptr_t)(page - 1);
            if (mprotect((void *)pg, (size_t)page, PROT_READ | PROT_WRITE) != 0) continue;   // RELRO: read-only after load
            *slot = replacement;
            __builtin___clear_cache((char *)slot, (char *)(slot + 1));
            mprotect((void *)pg, (size_t)page, PROT_READ);
            count++;
        }
    }
    return count;
}

static int FindIl2cpp(struct dl_phdr_info *info, size_t, void *data) {
    if (!info->dlpi_name || !strstr(info->dlpi_name, "libil2cpp.so")) return 0;
    *(int *)data = Rebind(info, "getaddrinfo", (void *)BF_getaddrinfo);
    return 1;
}

// libil2cpp.so is loaded right after BowlingPlus (Unity's NativeLoader.load), so wait for it to show up and
// attach before the game's first server lookup, like 1.4.8's add-image callback on iOS.
void BFDnsHookStart(void) {
    static bool done = false;
    if (done) return;
    done = true;
    sRealGetaddrinfo = (getaddrinfo_t)dlsym(RTLD_DEFAULT, "getaddrinfo");
    if (!sRealGetaddrinfo) return;
    BFLogEvent("dns", "game DNS filter waiting for the game to load");
    std::thread([] {
        for (int i = 0; i < 6000; i++) {          // up to ~2 minutes
            int n = -1;
            dl_iterate_phdr(FindIl2cpp, &n);
            if (n >= 0) {
                sRebound += n;
                BFLogEvent("dns", Fmt("game DNS filter attached to the game (%d slot%s)", n, n == 1 ? "" : "s"));
                return;
            }
            usleep(20000);
        }
        BFLogEvent("dns", "game DNS filter: libil2cpp.so never loaded");
    }).detach();
}

int BFDnsHookSlots(void) { return sRebound; }
