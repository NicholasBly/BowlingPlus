// Backs up the game's own sandbox - NSUserDefaults (where the Facebook SDK keeps cached login state), the
// game's save files, and anything else under the app's data folders - into one .zip you can save or share
// (Files, AirDrop, email). A safety net for your own records if the game's servers or the App Store listing
// ever go away. The Android side is android/java/com/bowlingplus/Backup.java.
//
// This only reads the app's own sandbox, the normal way any app reads its own files; nothing here touches
// another app or anything on a server. It doesn't parse the save data (its format isn't reverse engineered),
// so it's a raw copy, meant to be kept, not read. The Facebook access token itself lives in the iOS keychain,
// which an app extension can't export, so the token isn't in here - but the cached session/profile state in
// NSUserDefaults is, and that's what records that you were logged in and as whom.
#import <UIKit/UIKit.h>
#import "BFShared.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

class ZipWriter {   // minimal STORED (uncompressed) zip; the backup is small and this keeps us dependency-free
public:
    explicit ZipWriter(FILE *f) : f_(f) {}

    void add(const std::string &name, const uint8_t *data, size_t len) {
        uint32_t crc = crc32(data, len), off = (uint32_t)ftell(f_);
        put32(0x04034b50); put16(20); put16(0); put16(0); put16(0); put16(0);
        put32(crc); put32((uint32_t)len); put32((uint32_t)len);
        put16((uint16_t)name.size()); put16(0);
        fwrite(name.data(), 1, name.size(), f_);
        if (len) fwrite(data, 1, len, f_);
        central_.push_back({ name, crc, (uint32_t)len, off });
    }

    void finish() {
        uint32_t cdStart = (uint32_t)ftell(f_);
        for (auto &e : central_) {
            put32(0x02014b50); put16(20); put16(20); put16(0); put16(0); put16(0); put16(0);
            put32(e.crc); put32(e.size); put32(e.size);
            put16((uint16_t)e.name.size()); put16(0); put16(0); put16(0); put16(0);
            put32(0); put32(e.offset);
            fwrite(e.name.data(), 1, e.name.size(), f_);
        }
        uint32_t cdSize = (uint32_t)ftell(f_) - cdStart;
        put32(0x06054b50); put16(0); put16(0);
        put16((uint16_t)central_.size()); put16((uint16_t)central_.size());
        put32(cdSize); put32(cdStart); put16(0);
    }

private:
    struct Entry { std::string name; uint32_t crc, size, offset; };
    FILE *f_;
    std::vector<Entry> central_;
    void put16(uint16_t v) { uint8_t b[2] = { (uint8_t)v, (uint8_t)(v >> 8) }; fwrite(b, 1, 2, f_); }
    void put32(uint32_t v) { uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) }; fwrite(b, 1, 4, f_); }
    static uint32_t crc32(const uint8_t *data, size_t len) {
        static uint32_t table[256]; static bool init = false;
        if (!init) { for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320 ^ (c >> 1) : c >> 1; table[i] = c; } init = true; }
        uint32_t c = 0xFFFFFFFF;
        for (size_t i = 0; i < len; i++) c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
        return c ^ 0xFFFFFFFF;
    }
};

const long long kMaxTotal = 60LL * 1024 * 1024;
const long long kMaxFile = 8LL * 1024 * 1024;

}  // namespace

NSString *BFWriteBackup(void) {
    NSString *home = NSHomeDirectory();
    NSFileManager *fm = [NSFileManager defaultManager];

    NSString *outDir = [NSTemporaryDirectory() stringByAppendingPathComponent:@"BowlingPlus"];
    [fm createDirectoryAtPath:outDir withIntermediateDirectories:YES attributes:nil error:nil];
    NSDateFormatter *df = [NSDateFormatter new];
    df.dateFormat = @"yyyy-MM-dd_HHmm";
    NSString *zipPath = [outDir stringByAppendingPathComponent:[NSString stringWithFormat:@"bowlingplus-backup-%@.zip", [df stringFromDate:[NSDate date]]]];

    FILE *f = fopen(zipPath.fileSystemRepresentation, "wb");
    if (!f) return nil;
    ZipWriter zip(f);

    // a small note so the archive explains itself later
    NSDateFormatter *iso = [NSDateFormatter new];
    iso.dateFormat = @"yyyy-MM-dd'T'HH:mm:ssZ";
    NSDictionary *info = @{
        @"app": [NSBundle mainBundle].bundleIdentifier ?: @"",
        @"device": [NSString stringWithFormat:@"%@ / iOS %@", [UIDevice currentDevice].model, [UIDevice currentDevice].systemVersion],
        @"backedUpAt": [iso stringFromDate:[NSDate date]],
        @"gameDebugInfo": BFDebugInfo() ?: @"",
        @"note": @"Bowling by Jason Belmonte's own on-device data: NSUserDefaults (Library/Preferences/*.plist, "
                 "including cached Facebook login state), save files and app-support data. The Facebook access "
                 "token lives in the keychain and is not included. Nothing from the game's servers is in here.",
    };
    NSData *infoData = [NSJSONSerialization dataWithJSONObject:info options:NSJSONWritingPrettyPrinted error:nil];
    zip.add("backup-info.json", (const uint8_t *)infoData.bytes, infoData.length);

    // Walk the sandbox. Skip the throwaway/huge folders and our own output. tmp/ and Caches/ hold nothing worth
    // keeping; the Unity asset cache under Library can be hundreds of MB.
    NSArray<NSString *> *skip = @[ @"tmp", @"Library/Caches", @"BowlingPlus" ];
    long long used = 0;
    NSDirectoryEnumerator<NSString *> *en = [fm enumeratorAtPath:home];
    for (NSString *rel in en) {
        BOOL skipIt = NO;
        for (NSString *s in skip) if ([rel isEqualToString:s] || [rel hasPrefix:[s stringByAppendingString:@"/"]]) { skipIt = YES; break; }
        if (skipIt) { [en skipDescendants]; continue; }

        NSString *full = [home stringByAppendingPathComponent:rel];
        NSDictionary *attrs = [en fileAttributes];
        if (![attrs.fileType isEqualToString:NSFileTypeRegular]) continue;
        long long size = (long long)attrs.fileSize;
        if (size <= 0 || size > kMaxFile) continue;
        if (used + size > kMaxTotal) break;

        NSData *d = [NSData dataWithContentsOfFile:full options:0 error:nil];
        if (!d) continue;   // a file the OS won't hand us right now: skip it, keep the rest
        zip.add(std::string(rel.UTF8String), (const uint8_t *)d.bytes, d.length);
        used += size;
    }

    zip.finish();
    fclose(f);
    return zipPath;
}
