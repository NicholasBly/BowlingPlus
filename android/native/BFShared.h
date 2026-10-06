// Android version of src/BFShared.h: the same settings, status and functions, with C++ types
// (Str for NSString, Json for NSArray / NSDictionary). Game.cpp is a port of src/Game.mm.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "Platform.h"

#define BF_ALL_PINS ((uint16_t)0x3FF)
#define BF_VERSION "1.6.4"
#define BF_PLATFORM_VERSION "android-1"
#define BF_MAX_SPEED 5.0f
#define BF_MAX_SPIN 17.0f          // 17 x the game's 600 rpm cap ~ 10,000 rpm

// Settings (saved as JSON in the app's files folder: files/BowlingPlus/config.json)
typedef struct {
    bool     textureFix;   // Match Up BP / Pearl skin fix (looks only, works everywhere)
    bool     pinFix;       // continuous collision for pins + ball (Practice only)
    float    speedMult;    // ball speed multiplier 1...5 (Practice only)
    bool     spareMode;    // pick pins at the start of each frame (Practice only)
    uint16_t lastPinMask;  // bit i set = pin (i+1) standing
    bool     spareAuto;    // rack lastPinMask every frame without asking
    bool     fps120;       // 120 FPS mode
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
    bool     logHosts;     // log every server hostname the game looks up (diagnostics; off by default)
    // Android only (the Java side reads these; native just stores them so they survive a restart and the
    // menu's refresh, which reloads every setting from here)
    bool     menuButton;   // the draggable on-screen menu button (off by default)
    bool     fbWebLogin;   // Facebook login through the browser instead of the Facebook app (on by default)
    bool     laneOther;    // Practice: bowl on the game's other lane (experimental, off by default)
} BFConfig;

// Live game status for the menu
typedef struct {
    bool engineReady;      // IL2CPP found + game classes resolved
    bool offline;          // Practice (GameParams.gameMode == FUN)
    int  gameMode;
    int  location;
} BFStatus;

extern BFConfig gBF;
extern BFStatus gBFStatus;
extern bool gBFSafeMode;   // crash guard tripped: BowlingPlus stays out of the way

void BFMarkHealthy(void);
void BFExitSafeMode(void);

void BFLogLine(const Str &s);   // BFLog's sink (logcat + the event log)
template <typename... A> inline void BFLog(const char *fmt, const A &...a) { BFLogLine(Fmt(fmt, a...)); }

void BFLoadConfig(void);
void BFSaveConfig(void);
Json BFConfigJson(void);
bool BFConfigSet(const Str &key, double value);
Str BFDataDir(void);                 // files/BowlingPlus

// Game.cpp
void BFEngineTick(void);
void BFRequestSkipTutorial(void);
Str BFDebugInfo(void);
Str BFFpsLine(void);

// Log.cpp
void BFLogEvent(const Str &source, const Str &msg);
Str BFLogText(void);
void BFNetTest(void);

// Game.cpp - oil (practice)
bool BFOilReady(void);
Json BFOilBuiltins(void);                                   // [{index, name, feet, ml}]
Json BFOilCompute(int templateIndex, const Json *fwd, const Json *rev, int drop, bool exact, int feet, bool precise);   // null when it couldn't
Str BFPinImagePath(void);
Str BFPinImageStatus(void);
void BFOilApplyHue(void);
void BFOilSetCustom(const Json *pattern);                   // nullptr = off
Json BFOilBuiltinSpec(int index);
bool BFPracticeLobbyOpen(void);
Str BFOilStatusLine(void);

// Dns.cpp
void BFDnsHookStart(void);
int BFDnsHookSlots(void);

// Privacy (Java side reports what it sees)
int BFPrivacyAcceptCount(void);
Str BFPrivacyDebug(void);
bool BFPrivacyPageVisible(void);

// pin layouts + the spare picker
bool BFPinTapAt(float u, float v);
void BFApplySpareSelectionNow(uint16_t mask);
void BFSpareDismissed(void);
void BFMenuShowPinPickerOneShot(uint16_t mask);
bool BFMenuPickerOneShot(void);
bool BFMenuVisible(void);
bool BFOverlayVisible(void);   // any BowlingPlus panel is open over the game (menu, oil library, editor, pickers)
void BFApplySpareSelection(uint16_t mask);
void BFSetArsenalQuery(const Str &query);   // "" = cleared
Str BFStatusLine(void);
Str BFBallLine(void);
Str BFArsenalLine(void);

// the menu lives in Java (Menu.java); these post to it (Jni.cpp)
void BFMenuShowPinPicker(uint16_t mask);
void BFMenuHidePinPicker(void);
bool BFMenuPickerVisible(void);
void BFMenuSetSkipTutorialVisible(bool visible);

// BPTexture.cpp + Jni.cpp
std::vector<uint8_t> BFMakeMatchUpBPTexturePNG(int size);
std::vector<uint8_t> BFEncodePNG(const uint8_t *rgba, int w, int h);   // Java's Bitmap.compress
Str BFAppAssetList(void);           // every .manifest's "- Assets/..." lines, from the APK (Java)
Str BFDeviceLine(void);             // "Android 14 (API 34) | Pixel 8"
int BFScreenMaxHz(void);
