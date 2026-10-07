#pragma once
#import <Foundation/Foundation.h>
#include <stdint.h>
#include <stdbool.h>

#define BF_ALL_PINS ((uint16_t)0x3FF)
#define BF_VERSION @"1.7.2"
#define BF_MAX_SPEED 5.0f
#define BF_MAX_SPIN 17.0f          // 17 x the game's 600 rpm cap ~ 10,000 rpm

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
    bool     privacyOK;    // you accepted the My.Games privacy page once: it's pressed for you, out of sight, from now on
    bool     unstick;      // time limits for loading waits / spinners that can hang forever
    bool     gameIPv4;     // resolve the game's servers to IPv4 only (their IPv6 doesn't answer)
    bool     oilMirrorFix; // draw the oil on the side the physics uses (fixes mirrored carrydown)
    bool     oilBreakdown; // redraw the lane oil after every shot (practice)
    bool     oilInvisible; // hide the oil + random unlocked built-in pattern every game (practice)
    bool     oilThickness; // lane oil colors show thickness (display only)
    float    oilHue;       // oil color hue 0-1 for the lane, -1 = the game's own
    bool     pinImage;     // pins use the player's own image (looks only)
    float    spinMult;     // ball spin (RPM) multiplier 1...17 (Practice only)
    bool     menuHelp;     // show every description in the shake menu (off: they stay hidden)
    bool     menuButton;   // show a draggable on-screen button to open the menu, as well as shake
    bool     laneOther;    // Practice: bowl on the game's other lane (experimental, off by default)
    // (new fields go last: the initializers in Engine.mm / Engine.cpp are positional)
    bool     bgImage;      // the alley background uses the player's own picture (looks only)
    bool     bgTitle;      // ...and the room's name stays on it
    bool     pinPhys;      // realistic pin physics (Practice): pin collider friction, ball continuous collision
    float    pinFric;      // ...the pins' friction (0 = the recommended 0.25)
    bool     pinRate2x;    // ...and physics twice as often (experimental: writes the engine's fixed step)
    int      oilShowOrig;  // the game's SHOW_OIL_PATTERN while invisible oil keeps it at 0 in memory (0: not changed)
    bool     noTap9;       // game mode: 9-pin no-tap (9 or more on a full rack is a strike; Practice)
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
NSString *BFWriteBackup(void);      // Backup.mm: zip the app sandbox, returns the file path (or nil)
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
NSDictionary *BFOilCompute(int templateIndex, NSArray *fwd, NSArray *rev, int drop, BOOL exact, int feet, BOOL precise);  // exact: use each step's own end distance
void BFOilShowColorPicker(void);                  // OilUI.mm
NSString *BFPinImagePath(void);                   // Game.mm: Documents/BowlingPlus/pin_image.png
NSString *BFPinImageStatus(void);                 // Game.mm: what the pins show right now
void BFPinImagePick(BOOL fromFiles, void (^done)(NSString *message));   // OilUI.mm
void BFPinImageShareGuide(void);                  // OilUI.mm
void BFPinShowLibrary(void);                       // OilUI.mm: the pin library (presets, your pictures, 3D preview)
NSString *BFPinActiveName(void);                   // OilUI.mm: the pin picture on the pins (nil: the game's own)
NSString *BFPinUseLast(void);
NSString *BFBgImagePath(void);                     // Game.mm: Documents/BowlingPlus/bg_image.png (fitted to the wall, 2820:850)
NSData *BFGamePinPNG(void);                        // Game.mm: the game's current pin picture as PNG (main thread), nil if it can't                      // OilUI.mm: puts the last chosen picture back on (the menu switch)
void BFBgShowSheet(void);                          // OilUI.mm: the alley background sheet
NSString *BFBgStatus(void);
NSString *BFPinPhysStatus(void);                   // Game.mm: first-ball counts with realistic pin physics on and off                        // OilUI.mm: a line for the menu
void BFOilApplyHue(void);                         // Game.mm: push gBF.oilHue to the lane now      // runs the game's Kegel engine
NSArray<UIColor *> *BFOilColors(int n, float *maxHeight);                       // the game's oil color gradient
void BFOilSetCustom(NSDictionary *pattern);
NSDictionary *BFOilBuiltinSpec(int index);        // Game.mm: a game pattern's Kegel file as steps ({fwd, rev, drop, feet, ul, name}) or nil
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
NSString *BFPrivacyDebug(void);
bool BFPrivacyPageVisible(void);

// pin layouts + the spare picker
bool BFPinTapAt(float u, float v);                  // Game.mm: a tap (0..1 of the screen, v up); true if it opened the picker
void BFApplySpareSelectionNow(uint16_t mask);       // Game.mm: re-rack these pins right now, for this shot only
void BFSpareDismissed(void);                        // Game.mm: the picker was closed with the X
void BFMenuShowPinPickerOneShot(uint16_t mask);     // Menu.mm: the picker without Auto (this shot only)
bool BFMenuPickerOneShot(void);
bool BFMenuVisible(void);
void BFPinTapInstall(void);                         // Menu.mm: listen for taps on the game's window
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

// MenuButton.mm
void BFMenuButtonStart(void);
void BFMenuButtonRefresh(void);   // call after gBF.menuButton changes, and whenever the menu opens/closes

// BPTexture.mm
NSData *BFMakeMatchUpBPTexturePNG(int size);
#ifdef __cplusplus
}
#endif
