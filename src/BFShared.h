#pragma once
#import <Foundation/Foundation.h>
#include <stdint.h>
#include <stdbool.h>

#define BF_ALL_PINS ((uint16_t)0x3FF)
#define BF_VERSION @"1.4.4"
#define BF_MAX_SPEED 5.0f

// Settings (saved in NSUserDefaults)
typedef struct {
    bool     textureFix;   // Match Up BP / Pearl skin fix (looks only, works everywhere)
    bool     pinFix;       // continuous collision for pins + ball (Practice only)
    float    speedMult;    // ball speed multiplier 1...5 (Practice only)
    bool     spareMode;    // pick pins at the start of each frame (Practice only)
    uint16_t lastPinMask;  // bit i set = pin (i+1) standing
    bool     spareAuto;    // rack lastPinMask every frame without asking
    bool     fps120;       // 120 FPS mode (needs CADisableMinimumFrameDurationOnPhone in Info.plist)
    bool     pinSpec;      // experimental: pins (never the ball) use speculative collisions
    bool     autoPrivacy;  // press "Sign up" on the My.Games privacy page that shows every launch
    bool     unstick;      // time limits for loading waits / spinners that can hang forever
    bool     gameIPv4;     // resolve the game's servers to IPv4 only (their IPv6 doesn't answer)
    bool     oilMirrorFix; // draw the oil on the side the physics uses (fixes mirrored carrydown)
    bool     oilBreakdown; // redraw the lane oil after every shot (practice)
    bool     oilInvisible; // hide the oil + random unlocked built-in pattern every game (practice)
    bool     oilThickness; // lane oil colors show thickness (display only)
    float    oilHue;       // oil color hue 0-1 for the lane, -1 = the game's own
} BFConfig;

// Live game status for the menu
typedef struct {
    bool engineReady;      // IL2CPP found + game classes resolved
    bool offline;          // Practice (GameParams.gameMode == FUN)
    int  gameMode;
    int  location;
} BFStatus;

#ifdef __cplusplus
extern "C" {
#endif
extern BFConfig gBF;
extern BFStatus gBFStatus;
extern bool gBFSafeMode;   // crash guard tripped: BowlingPlus stays out of the way

void BFMarkHealthy(void);   // the game reached the lane fine, reset the crash guard
void BFExitSafeMode(void);

void BFLog(NSString *fmt, ...) NS_FORMAT_FUNCTION(1, 2);
void BFStart(void);
void BFHandleShake(NSString *source);
void BFLoadConfig(void);
void BFSaveConfig(void);

// Game.mm
void BFEngineTick(void);
void BFRequestSkipTutorial(void);   // runs the game's own TutorialManager.SkipTutorial() next frame
NSString *BFDebugInfo(void);        // text for the "Copy debug info" button
NSString *BFFpsLine(void);          // status line under the 120 FPS switch

// Log.mm (diagnostics log; "Copy log" in the menu)
void BFLogEvent(NSString *source, NSString *msg);
NSString *BFLogText(void);
void BFLogCaptureStart(void);
void BFNetMonitorStart(void);
void BFNetTest(void);

// Game.mm - oil (practice)
@class UIColor;
bool BFOilReady(void);
NSArray<NSDictionary *> *BFOilBuiltins(void);                                   // [{index, name, feet, ml}]
NSDictionary *BFOilCompute(int templateIndex, NSArray *fwd, NSArray *rev, int drop, BOOL exact);  // exact: use each step's own end distance
void BFOilShowColorPicker(void);                  // OilUI.mm
void BFOilApplyHue(void);                         // Game.mm: push gBF.oilHue to the lane now      // runs the game's Kegel engine
NSArray<UIColor *> *BFOilColors(int n, float *maxHeight);                       // the game's oil color gradient
void BFOilSetCustom(NSDictionary *pattern);
NSArray<NSDictionary *> *BFOilCollection(void);   // OilCollection.mm: built-in BowlingPlus patterns
bool BFPracticeLobbyOpen(void);
NSString *BFOilStatusLine(void);

// OilUI.mm - custom oil pattern library
void BFOilUIStart(void);
void BFOilShowLibrary(void);

// Dns.mm
void BFDnsHookStart(void);
int BFDnsHookSlots(void);

// Privacy.mm
void BFPrivacyStart(void);
int BFPrivacyAcceptCount(void);
bool BFPrivacyPageVisible(void);
void BFApplySpareSelection(uint16_t mask);
void BFSetArsenalQuery(NSString *query);
NSString *BFStatusLine(void);
NSString *BFBallLine(void);
NSString *BFArsenalLine(void);

// Menu.mm
void BFMenuToggle(void);
void BFMenuShowPinPicker(uint16_t mask);
void BFMenuHidePinPicker(void);
bool BFMenuPickerVisible(void);
void BFMenuSetSkipTutorialVisible(bool visible);
NSString *BFPinsText(uint16_t mask);   // e.g. "7-10"

// Shake.mm
void BFShakeStart(void);

// BPTexture.mm
NSData *BFMakeMatchUpBPTexturePNG(int size);
#ifdef __cplusplus
}
#endif
