#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import "BFShared.h"
#include <math.h>

BFConfig gBF = { true, true, 1.0f, false, BF_ALL_PINS, false, false, false, false, true, true, true, false, false, false, -1.0f, false, 1.0f, false };
BFStatus gBFStatus = { false, false, -1, -1 };
bool gBFSafeMode = false;

static NSString *const kGuardKey = @"BowlingPlus.startGuard";

void BFMarkHealthy(void) {
    [[NSUserDefaults standardUserDefaults] setInteger:0 forKey:kGuardKey];
    BFLog(@"game started fine - crash guard reset");
}

void BFExitSafeMode(void) {
    gBFSafeMode = false;
    [[NSUserDefaults standardUserDefaults] setInteger:0 forKey:kGuardKey];
    BFLog(@"safe mode turned off from the menu");
}

void BFLog(NSString *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    NSString *s = [[NSString alloc] initWithFormat:fmt arguments:ap];
    va_end(ap);
    NSLog(@"[BowlingPlus] %@", s);
    BFLogEvent(@"bf", s);
}

static NSString *const kCfgKey = @"BowlingPlus.config.v1";

void BFLoadConfig(void) {
    NSDictionary *d = [[NSUserDefaults standardUserDefaults] dictionaryForKey:kCfgKey]
        ?: [[NSUserDefaults standardUserDefaults] dictionaryForKey:@"BowlingFix.config.v1"];   // saved before the rename
    if (![d isKindOfClass:[NSDictionary class]]) return;
    if (d[@"tex"])   gBF.textureFix  = [d[@"tex"] boolValue];
    if (d[@"pin"])   gBF.pinFix      = [d[@"pin"] boolValue];
    if (d[@"speed"]) gBF.speedMult   = [d[@"speed"] floatValue];
    if (d[@"spin"])  gBF.spinMult    = [d[@"spin"] floatValue];
    if (d[@"menuHelp"]) gBF.menuHelp = [d[@"menuHelp"] boolValue];
    if (d[@"menuButton"]) gBF.menuButton = [d[@"menuButton"] boolValue];
    if (d[@"laneOther"]) gBF.laneOther = [d[@"laneOther"] boolValue];
    if (d[@"spare"]) gBF.spareMode   = [d[@"spare"] boolValue];
    if (d[@"mask"])  gBF.lastPinMask = (uint16_t)[d[@"mask"] unsignedIntValue];
    if (d[@"auto"])  gBF.spareAuto   = [d[@"auto"] boolValue];
    if (d[@"fps120"]) gBF.fps120     = [d[@"fps120"] boolValue];
    if (d[@"pinSpec2"]) gBF.pinSpec  = [d[@"pinSpec2"] boolValue];   // 1.2.0's "pinSpec" is ignored on purpose
    if (d[@"unstick"]) gBF.unstick   = [d[@"unstick"] boolValue];
    if (d[@"ipv4"])    gBF.gameIPv4  = [d[@"ipv4"] boolValue];
    if (d[@"oilMirror"]) gBF.oilMirrorFix = [d[@"oilMirror"] boolValue];
    if (d[@"oilBreak"])  gBF.oilBreakdown = [d[@"oilBreak"] boolValue];
    if (d[@"oilInvis"])  gBF.oilInvisible = [d[@"oilInvis"] boolValue];
    if (d[@"oilThick2"]) gBF.oilThickness = [d[@"oilThick2"] boolValue];   // new name: 1.4.1's default-on is dropped
    if (d[@"oilHue"])    gBF.oilHue = [d[@"oilHue"] floatValue];
    if (d[@"pinImage"])  gBF.pinImage = [d[@"pinImage"] boolValue];
    if (d[@"privacyOK"]) gBF.privacyOK = [d[@"privacyOK"] boolValue];
    else if ([d[@"privacy"] boolValue]) gBF.privacyOK = true;     // auto-accept was on: you had accepted before
    if (isnan(gBF.speedMult) || gBF.speedMult < 1.f) gBF.speedMult = 1.f;
    if (gBF.speedMult > BF_MAX_SPEED) gBF.speedMult = BF_MAX_SPEED;   // v1.0 allowed up to 500x
    if (isnan(gBF.spinMult) || gBF.spinMult < 1.f) gBF.spinMult = 1.f;
    if (gBF.spinMult > BF_MAX_SPIN) gBF.spinMult = BF_MAX_SPIN;
    gBF.lastPinMask &= BF_ALL_PINS;
    if (!gBF.lastPinMask) gBF.lastPinMask = BF_ALL_PINS;
}

void BFSaveConfig(void) {
    NSDictionary *d = @{ @"tex": @(gBF.textureFix), @"pin": @(gBF.pinFix), @"speed": @(gBF.speedMult), @"spin": @(gBF.spinMult), @"menuHelp": @(gBF.menuHelp), @"menuButton": @(gBF.menuButton), @"laneOther": @(gBF.laneOther),
                         @"spare": @(gBF.spareMode), @"mask": @(gBF.lastPinMask),
                         @"auto": @(gBF.spareAuto),
                         @"fps120": @(gBF.fps120), @"pinSpec2": @(gBF.pinSpec), @"privacyOK": @(gBF.privacyOK),
                         @"unstick": @(gBF.unstick), @"ipv4": @(gBF.gameIPv4),
                         @"oilMirror": @(gBF.oilMirrorFix), @"oilBreak": @(gBF.oilBreakdown), @"oilInvis": @(gBF.oilInvisible), @"oilThick2": @(gBF.oilThickness), @"oilHue": @(gBF.oilHue), @"pinImage": @(gBF.pinImage) };
    [[NSUserDefaults standardUserDefaults] setObject:d forKey:kCfgKey];
}

// Runs once per screen refresh on the main thread - the same thread Unity uses on
// iOS, so it's safe to talk to the game between its frames.
@interface BFTicker : NSObject
- (void)tick:(CADisplayLink *)link;
@end
@implementation BFTicker
- (void)tick:(CADisplayLink *)link { BFEngineTick(); }
@end

static BFTicker *sTicker;
static CADisplayLink *sLink;

static void BFEngineStart(void) {
    if (sLink) return;
    BFLoadConfig();
    // Crash guard: count launches that crashed before the game finished starting. After 2 in a row, BowlingPlus
    // stays out of the way so the game still works (the shake menu can turn it back on).
    NSUserDefaults *ud = [NSUserDefaults standardUserDefaults];
    NSInteger unfinished = [ud integerForKey:kGuardKey];
    if (unfinished >= 2) {
        gBFSafeMode = true;
        BFLog(@"SAFE MODE: the last %ld launches didn't finish starting, so BowlingPlus is paused", (long)unfinished);
    }
    [ud setInteger:unfinished + 1 forKey:kGuardKey];
    // A launch still running after 60 s didn't crash, even if the game is stuck loading (for example its
    // server can't be reached): that must not pause BowlingPlus, whose "Fix connection" helps exactly then.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(60 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        if ([[NSUserDefaults standardUserDefaults] integerForKey:kGuardKey] > 0) {
            [[NSUserDefaults standardUserDefaults] setInteger:0 forKey:kGuardKey];
            BFLog(@"still running after 60 s (no crash) - crash guard reset");
        }
    });
    sTicker = [BFTicker new];
    sLink = [CADisplayLink displayLinkWithTarget:sTicker selector:@selector(tick:)];
    sLink.preferredFramesPerSecond = 60;   // our checks never need more than 60 per second
    [sLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
    BFShakeStart();
    BFMenuButtonStart();
    if (!gBFSafeMode) {
        BFPrivacyStart();
        BFPinTapInstall();
        BFOilUIStart();
        BFNetMonitorStart();
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(3 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{ BFNetTest(); });
    }
    BFLog(@"v%@ started - shake the phone to open the menu", BF_VERSION);
}

void BFStart(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        BFLogCaptureStart();   // record the game's console output from the very start
        BFDnsHookStart();      // before the game's first server lookup
        // runs as soon as the app's main loop starts
        dispatch_async(dispatch_get_main_queue(), ^{
            [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationDidBecomeActiveNotification
                                                              object:nil
                                                               queue:[NSOperationQueue mainQueue]
                                                          usingBlock:^(NSNotification *note) { BFEngineStart(); }];
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(4 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                BFEngineStart();
            });
        });
    });
}
