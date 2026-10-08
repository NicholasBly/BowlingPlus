# IPv6 loading fix ("Game server over IPv4" and "Don't get stuck connecting")

Status: shipped since 1.2.3 (IPv4 filter) and 1.2.1 (watchdogs). Both on by default.
Confirmed on a phone after 1.2.3: networking with NextDNS (IPv4 filter and watchdogs), per `VERIFIED_NOTES.md`. The Android port is in the code and passes CI; no phone test is recorded for it.
Difficulty to port: **1-2**. The watchdogs are plain timers. The root fix is a server or address-selection change, which is easier for the team than for us.

## The problem

- The game's network library (Photon) resolves the game's server names, and in `IPhotonSocket.GetIpAddress` (RVA `0x32AD060` in the iOS build) it returns the **first IPv6 address** it gets. It uses IPv4 only when the DNS answer has no IPv6 at all.
- The game's servers didn't answer on IPv6 in our tests. Over IPv4 they answered.
- Any DNS that returns IPv6 records for those hosts (NextDNS does, and it's a common privacy setting) makes every connection attempt hang, time out and retry forever. The player sees endless loading, or "disconnected from the server".

Tests recorded in `VERIFIED_NOTES.md` ("Photon prefers IPv6" and "The game servers don't answer on IPv6").

## What BowlingPlus does

### 1. "Game server over IPv4" (DNS filter)

Shared logic in `src/Dns.mm` (iOS) and `android/native/Dns.cpp` (Android):

- The game calls the system `getaddrinfo()` through its import table. BowlingPlus points that one import slot at its own wrapper. This only changes data, never code, so it works on a phone that isn't jailbroken.
  - **iOS:** a "fishhook"-style rebind of the slot in `UnityFramework` (the 1.907 build has it at `0x481abf8` in `__DATA.__la_symbol_ptr`). That binary uses classic dyld binding, so the slot is rewritten directly.
  - **Android:** the same idea on the game's `libil2cpp.so` global offset table. BowlingPlus makes the page writable with `mprotect`, rewrites the slot, then puts the page's protection back exactly as it was.
- The wrapper only changes lookups for `*.wannaplay.studio`, `*.spareball.com` and `wannaplay.studio`, and only when the game asks for "any address family" (`AF_UNSPEC`). For those it asks for `AF_INET` (IPv4) only.
- If a name has no IPv4 address, the normal lookup runs unchanged, so nothing breaks.
- It must attach as soon as the game's code is loaded. In 1.4.8 it attached too late (diagnostics showed `dnsSlots=0`), so the attach happens when the game's library loads, not at BowlingPlus's startup.
- It's skipped in safe mode.

### 2. "Don't get stuck connecting" (watchdogs)

`StuckTick()` in `src/Game.mm` and `android/native/Game.cpp` (shared logic, one timer, checked every 30 frames):

- **Loading screen stuck on "connecting" for 30 s:** shows the game's own offline section (`SwitchToSection(_offlineSection)` on `LoadingWindow`). If the connection comes through later, the game's normal flow takes over again.
- **Privacy SDK silent for 20 s with no privacy page on screen:** marks the wait as done, the same as having no internet. The game has no time limit on this wait otherwise.
- **Spinner with no server answer for 30 s:** hides it (`Processing.Hide`) so the player can try again.
- Safe mode counts only real crashes, so a slow loading screen doesn't switch BowlingPlus off (1.4.8).

## Where the official fix belongs

In order of preference:

1. **Server side:** make the game's servers answer on IPv6, or stop publishing AAAA records for them. Then the client needs no change.
2. **Client, one rule:** when choosing an address in `IPhotonSocket.GetIpAddress`, prefer IPv4 for the game's hosts when one exists. That's the same policy as BowlingPlus's filter, done in the source. We haven't seen the source, so we can't say how it's written.
3. **Loading time limits:** port the three watchdogs above. These help any player whose connection hangs, whatever the cause, and they're the most useful part for players outside the NextDNS case.

If you do (1) or (2), BowlingPlus's filter becomes a no-op: the lookup returns IPv4 either way. It logs `dnsSlots=0` if the import slot is gone. Safe to run alongside, but once you ship it, hide BowlingPlus's switch.

## Testing it

Use a DNS that returns IPv6 for the game hosts (NextDNS over HTTPS works; so does any resolver that returns AAAA records for `*.wannaplay.studio`).

- **Without the fix:** loading spins for a long time or never ends; the game's "play offline" button doesn't appear.
- **With the IPv4 filter only:** the game connects normally.
- **With the watchdogs only and a DNS that breaks the connection:** after about 30 s, the offline section appears and Practice can be entered.

What to check in the log:

- `game lookup <host>: using IPv4 only` (filter working)
- `loading: stuck connecting for 30 s, showing the game's offline button` (watchdog fired)
- `loading: privacy SDK silent for 20 s, carrying on` (privacy watchdog fired)
- `spinner: no server answer for 30 s, hid it` (spinner watchdog fired)
- `dnsSlots=<n>` in the loading line: `0` means the filter didn't attach.

## Duplicate-fix note

BowlingPlus can't know whether the official client already has a fix. Until a version check exists, both can run together without harm. Once the official fix ships, hide BowlingPlus's two switches and skip the filter (see the compatibility section in `PORTING_GUIDE.md`).
