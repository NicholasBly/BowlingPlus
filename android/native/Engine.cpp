// Port of src/Engine.mm: settings, the crash guard (safe mode) and BFLog. The per-frame tick is driven from
// Java (BP.java posts a job to Unity's main thread every screen refresh; see Jni.cpp), the Android
// equivalent of the iOS CADisplayLink on the main thread Unity also runs on.
#include "BFShared.h"
#include <android/log.h>
#include <math.h>
#include <mutex>

BFConfig gBF = { true, true, 1.0f, false, BF_ALL_PINS, false, false, false, false, true, true, true, false, false, false, -1.0f, false, 1.0f, false, false, false, true };
BFStatus gBFStatus = { false, false, -1, -1 };
bool gBFSafeMode = false;

static Str sDataDir;
static std::mutex sCfgLock;

void BFSetDataDir(const Str &filesDir) {
    sDataDir = filesDir + "/BowlingPlus";
    MakeDirs(sDataDir);
}
Str BFDataDir(void) { return sDataDir; }

static Str GuardPath() { return sDataDir + "/startGuard"; }
static int ReadGuard() {
    std::vector<uint8_t> d;
    if (!ReadFile(GuardPath(), d) || d.empty()) return 0;
    return atoi(Str(d.begin(), d.end()).c_str());
}
static void WriteGuard(int v) { Str s = Fmt("%d", v); WriteFile(GuardPath(), s.data(), s.size()); }

void BFLogLine(const Str &s) {
    __android_log_print(ANDROID_LOG_INFO, "BowlingPlus", "%s", s.c_str());
    BFLogEvent("bf", s);
}

void BFMarkHealthy(void) {
    WriteGuard(0);
    BFLog("game started fine - crash guard reset");
}

void BFExitSafeMode(void) {
    gBFSafeMode = false;
    WriteGuard(0);
    BFLog("safe mode turned off from the menu");
}

// Same keys as the iOS NSUserDefaults dictionary, so the two builds read the same way.
Json BFConfigJson(void) {
    Json d = Json::Obj();
    d.set("tex", Json::Bool_(gBF.textureFix)); d.set("pin", Json::Bool_(gBF.pinFix)); d.set("speed", gBF.speedMult);
    d.set("spin", gBF.spinMult); d.set("menuHelp", Json::Bool_(gBF.menuHelp));
    d.set("spare", Json::Bool_(gBF.spareMode)); d.set("mask", gBF.lastPinMask); d.set("auto", Json::Bool_(gBF.spareAuto));
    d.set("fps120", Json::Bool_(gBF.fps120)); d.set("pinSpec2", Json::Bool_(gBF.pinSpec)); d.set("privacyOK", Json::Bool_(gBF.privacyOK));
    d.set("unstick", Json::Bool_(gBF.unstick)); d.set("ipv4", Json::Bool_(gBF.gameIPv4));
    d.set("oilMirror", Json::Bool_(gBF.oilMirrorFix)); d.set("oilBreak", Json::Bool_(gBF.oilBreakdown));
    d.set("oilInvis", Json::Bool_(gBF.oilInvisible)); d.set("oilThick2", Json::Bool_(gBF.oilThickness));
    d.set("oilHue", gBF.oilHue); d.set("pinImage", Json::Bool_(gBF.pinImage));
    d.set("bgImage", Json::Bool_(gBF.bgImage)); d.set("bgTitle", Json::Bool_(gBF.bgTitle));
    d.set("pinPhys", Json::Bool_(gBF.pinPhys)); d.set("pinFric", gBF.pinFric); d.set("pinRate2x", Json::Bool_(gBF.pinRate2x));
    d.set("oilShowOrig", gBF.oilShowOrig); d.set("noTap9", Json::Bool_(gBF.noTap9));
    d.set("logHosts", Json::Bool_(gBF.logHosts));
    d.set("menuButton", Json::Bool_(gBF.menuButton)); d.set("fbWebLogin", Json::Bool_(gBF.fbWebLogin));
    d.set("laneOther", Json::Bool_(gBF.laneOther));
    d.set("hideBallStats", Json::Bool_(gBF.hideBallStats)); d.set("loftHud", Json::Bool_(gBF.loftHud)); d.set("loftFt", gBF.loftFt);
    d.set("arsSort", gBF.arsSort); d.set("arsDesc", Json::Bool_(gBF.arsDesc)); d.set("rampsMode", Json::Bool_(gBF.rampsMode)); d.set("rampsBest", gBF.rampsBest);
    d.set("pinFricV2", 1);
    return d;
}

static void Clamp() {
    if (std::isnan(gBF.speedMult) || gBF.speedMult < 1.f) gBF.speedMult = 1.f;
    if (gBF.speedMult > BF_MAX_SPEED) gBF.speedMult = BF_MAX_SPEED;
    if (std::isnan(gBF.spinMult) || gBF.spinMult < 1.f) gBF.spinMult = 1.f;
    if (gBF.spinMult > BF_MAX_SPIN) gBF.spinMult = BF_MAX_SPIN;
    gBF.lastPinMask &= BF_ALL_PINS;
    if (!gBF.lastPinMask) gBF.lastPinMask = BF_ALL_PINS;
    if (std::isnan(gBF.loftFt) || gBF.loftFt < 0) gBF.loftFt = 0;
    if (gBF.loftFt > BF_MAX_LOFT) gBF.loftFt = BF_MAX_LOFT;
    if (gBF.arsSort < 0 || gBF.arsSort > 6) gBF.arsSort = 0;
}

bool BFConfigSet(const Str &k, double v) {
    bool on = v != 0;
    if (k == "tex") gBF.textureFix = on;
    else if (k == "pin") gBF.pinFix = on;
    else if (k == "speed") gBF.speedMult = (float)v;
    else if (k == "spin") gBF.spinMult = (float)v;
    else if (k == "menuHelp") gBF.menuHelp = on;
    else if (k == "spare") gBF.spareMode = on;
    else if (k == "mask") gBF.lastPinMask = (uint16_t)v;
    else if (k == "auto") gBF.spareAuto = on;
    else if (k == "fps120") gBF.fps120 = on;
    else if (k == "pinSpec2") gBF.pinSpec = on;
    else if (k == "privacyOK") gBF.privacyOK = on;
    else if (k == "unstick") gBF.unstick = on;
    else if (k == "ipv4") gBF.gameIPv4 = on;
    else if (k == "oilMirror") gBF.oilMirrorFix = on;
    else if (k == "oilBreak") gBF.oilBreakdown = on;
    else if (k == "oilInvis") gBF.oilInvisible = on;
    else if (k == "oilThick2") gBF.oilThickness = on;
    else if (k == "oilHue") gBF.oilHue = (float)v;
    else if (k == "pinImage") gBF.pinImage = on;
    else if (k == "bgImage") gBF.bgImage = on;
    else if (k == "bgTitle") gBF.bgTitle = on;
    else if (k == "pinPhys") gBF.pinPhys = on;
    else if (k == "pinFric") gBF.pinFric = (float)v;
    else if (k == "pinRate2x") gBF.pinRate2x = on;
    else if (k == "oilShowOrig") gBF.oilShowOrig = (int)v;
    else if (k == "noTap9") gBF.noTap9 = on;
    else if (k == "realOil") {}                    // was a setting up to 1.7.4: now managed (see BFLoadConfig)
    else if (k == "logHosts") gBF.logHosts = on;
    else if (k == "menuButton") gBF.menuButton = on;
    else if (k == "fbWebLogin") gBF.fbWebLogin = on;
    else if (k == "laneOther") gBF.laneOther = on;
    else if (k == "hideBallStats") gBF.hideBallStats = on;
    else if (k == "loftHud") gBF.loftHud = on;
    else if (k == "loftFt") gBF.loftFt = (float)v;
    else if (k == "arsSort") gBF.arsSort = (int)v;
    else if (k == "arsDesc") gBF.arsDesc = on;
    else if (k == "rampsMode") gBF.rampsMode = on;
    else if (k == "rampsBest") gBF.rampsBest = (int)v;
    else if (k == "pinFricV2") {}                  // marker: pinFric 0 means recommended (1.7.5)
    else return false;
    Clamp();
    return true;
}

void BFLoadConfig(void) {
    gBF.realOil = false;  // managed, not a setting: the game's own patterns keep the game's drawing (as online), custom patterns always get the real-life one
    gBF.loftHud = true;
    std::vector<uint8_t> raw;
    if (!ReadFile(sDataDir + "/config.json", raw)) return;
    Json d = Json::Parse(Str(raw.begin(), raw.end()));
    if (!d.isObj()) return;
    for (auto &kv : d.o) BFConfigSet(kv.first, kv.second.num());
    // 1.7.0-1.7.4 stored 0.25 for "recommended"; since 1.7.5 0 means recommended (0.40, or 0.25 with the double rate)
    if (!d.has("pinFricV2") && fabsf(gBF.pinFric - 0.25f) < 0.001f) gBF.pinFric = 0;
    Clamp();
}

void BFSaveConfig(void) {
    std::lock_guard<std::mutex> g(sCfgLock);
    Str s = BFConfigJson().Dump();
    WriteFile(sDataDir + "/config.json", s.data(), s.size());
}

// Called once, from Java, when the game's activity is up (iOS: BFEngineStart, 4 s after launch).
static double sStartedAt = 0;
static bool sGuardOpen = false;

void BFEngineStart(void) {
    static bool started = false;
    if (started) return;
    started = true;
    BFLoadConfig();
    // Crash guard: count launches that crashed before the game finished starting. After 2 in a row, BowlingPlus
    // stays out of the way so the game still works (the shake menu can turn it back on).
    int unfinished = ReadGuard();
    if (unfinished >= 2) {
        gBFSafeMode = true;
        BFLog("SAFE MODE: the last %d launches didn't finish starting, so BowlingPlus is paused", unfinished);
    }
    WriteGuard(unfinished + 1);
    sStartedAt = BFNow();
    sGuardOpen = true;
    BFLog("v%s (%s) started - shake the phone to open the menu", BF_VERSION, BF_PLATFORM_VERSION);
}

// A launch still running after 60 s didn't crash, even if the game is stuck loading (for example its
// server can't be reached): that must not pause BowlingPlus, whose "Fix connection" helps exactly then.
void BFEngineGuardTick(void) {
    if (!sGuardOpen || BFNow() - sStartedAt < 60) return;
    sGuardOpen = false;
    if (ReadGuard() > 0) {
        WriteGuard(0);
        BFLog("still running after 60 s (no crash) - crash guard reset");
    }
}
