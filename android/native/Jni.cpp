// How BowlingPlus gets into the game on Android (the iOS Tweak.xm + Engine.mm start-up).
//
// The patched APK ships this library as lib/arm64-v8a/libmain.so and the game's own libmain.so renamed to
// libmain_orig.so. Unity's Java code loads "main" first thing; our JNI_OnLoad loads the original and hands
// it the call (so Unity starts exactly as before), then:
//  - starts the IPv4 DNS filter (it attaches to libil2cpp.so as soon as that loads),
//  - starts BowlingPlus's Java side (the menu). Its classes ship as an extra classesN.dex in the patched APK
//    (tools/patch_apk.py), so the game's own class loader finds them like any other app class.
// None of the game's own code, resources or libraries is changed.
//
// Threads: on iOS the game and UIKit share the main thread. On Android, Unity runs on its own "UnityMain"
// thread and the menu on the UI thread, so every menu action that touches the game is queued here and run
// on Unity's thread (RunOnGame), between frames, like the iOS tick.
#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include "BFShared.h"
#include "KegelParse.h"
#include "PinWrap.h"
#include "PinGuide.h"
#include "Logo.h"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "BowlingPlus", __VA_ARGS__)

void BFSetDataDir(const Str &filesDir);   // Engine.cpp
void BFEngineStart(void);
void BFEngineGuardTick(void);

static JavaVM *sVM;
static jclass sBP;                        // com.bowlingplus.BP (global ref)
static jmethodID sUi, sEncodePng, sAssetList, sHttpTest, sPoke;
static std::atomic<bool> sStarted{ false };

// ---------------------------------------------------------------------------
// JNI helpers
// ---------------------------------------------------------------------------
struct Env {
    JNIEnv *env = nullptr;
    bool attached = false;
    Env() {
        if (!sVM) return;
        if (sVM->GetEnv((void **)&env, JNI_VERSION_1_6) != JNI_OK) {
            if (sVM->AttachCurrentThread(&env, nullptr) == JNI_OK) attached = true;
            else env = nullptr;
        }
    }
    ~Env() { if (attached) sVM->DetachCurrentThread(); }
    bool clear() { if (env && env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); return true; } return false; }
};

static Str JStr(JNIEnv *env, jstring s) {
    if (!s) return Str();
    const char *c = env->GetStringUTFChars(s, nullptr);
    Str o = c ? c : "";
    if (c) env->ReleaseStringUTFChars(s, c);
    return o;
}

// Java strings come as "modified UTF-8"; ours are plain UTF-8, which NewStringUTF can reject (emoji).
// Going through a byte[] and Java's own decoder is always safe.
static jstring NewJStr(JNIEnv *env, const Str &s) {
    jbyteArray b = env->NewByteArray((jsize)s.size());
    if (!b) return nullptr;
    env->SetByteArrayRegion(b, 0, (jsize)s.size(), (const jbyte *)s.data());
    jclass strCls = env->FindClass("java/lang/String");
    jmethodID ctor = env->GetMethodID(strCls, "<init>", "([BLjava/lang/String;)V");
    jstring cs = env->NewStringUTF("UTF-8");
    jstring out = (jstring)env->NewObject(strCls, ctor, b, cs);
    env->DeleteLocalRef(b);
    env->DeleteLocalRef(cs);
    env->DeleteLocalRef(strCls);
    return out;
}

static void UiCall(const char *cmd, const Str &arg) {   // posts to the Android UI thread (BP.ui never blocks)
    Env e;
    if (!e.env || !sBP || !sUi) return;
    jstring c = e.env->NewStringUTF(cmd), a = NewJStr(e.env, arg);
    e.env->CallStaticVoidMethod(sBP, sUi, c, a);
    e.clear();
    e.env->DeleteLocalRef(c);
    if (a) e.env->DeleteLocalRef(a);
}

static void PokeGame() {   // BP.poke(): ask Unity to run our queue on its next frame
    Env e;
    if (!e.env || !sBP || !sPoke) return;
    e.env->CallStaticVoidMethod(sBP, sPoke);
    e.clear();
}

// ---------------------------------------------------------------------------
// Unity-thread job queue
// ---------------------------------------------------------------------------
struct Job {
    std::function<void()> fn;
    int state = 0;   // 0 queued, 1 running, 2 done, 3 cancelled
};
static std::mutex sJobLock;
static std::condition_variable sJobCv;
static std::deque<std::shared_ptr<Job>> sJobs;
static std::atomic<pid_t> sGameTid{ 0 };

// Runs fn on Unity's thread. timeoutMs = 0: fire and forget. Returns false if Unity didn't get to it in
// time (paused, or busy loading), in which case fn never runs. Queuing pokes Unity (BP.poke), so the queue
// is looked at on the very next frame.
static bool RunOnGame(std::function<void()> fn, int timeoutMs) {
    if (sGameTid && gettid() == sGameTid) { fn(); return true; }
    auto j = std::make_shared<Job>();
    j->fn = std::move(fn);
    {
        std::lock_guard<std::mutex> g(sJobLock);
        sJobs.push_back(j);
    }
    PokeGame();   // the per-frame tick would get to it too, but not while this (UI) thread is waiting
    if (timeoutMs <= 0) return true;
    std::unique_lock<std::mutex> lk(sJobLock);
    if (sJobCv.wait_for(lk, std::chrono::milliseconds(timeoutMs), [&] { return j->state == 2; })) return true;
    if (j->state == 0) { j->state = 3; return false; }               // never started: drop it
    sJobCv.wait(lk, [&] { return j->state == 2; });                   // already running: it uses our stack, wait
    return true;
}

static void DrainJobs() {
    for (;;) {
        std::shared_ptr<Job> j;
        {
            std::lock_guard<std::mutex> g(sJobLock);
            if (sJobs.empty()) return;
            j = sJobs.front();
            sJobs.pop_front();
            if (j->state == 3) continue;
            j->state = 1;
        }
        try { if (j->fn) j->fn(); } catch (...) { BFLog("exception in a menu action"); }
        {
            std::lock_guard<std::mutex> g(sJobLock);
            j->state = 2;
        }
        sJobCv.notify_all();
    }
}

// ---------------------------------------------------------------------------
// what Game.cpp / Log.cpp / BPTexture.cpp ask of the platform
// ---------------------------------------------------------------------------
static std::atomic<bool> sMenuVisible{ false }, sPickerVisible{ false }, sPickerOneShot{ false }, sPrivacyVisible{ false };
static std::atomic<int> sPrivacyAccepted{ 0 };
static std::mutex sStrLock;
static Str sPrivacyDebug = "privacy: (not started)", sDeviceLine = "Android", sAssetListCache;
static int sMaxHz = 0;

bool BFMenuVisible(void) { return sMenuVisible; }
bool BFMenuPickerVisible(void) { return sPickerVisible; }
bool BFMenuPickerOneShot(void) { return sPickerVisible && sPickerOneShot; }
void BFMenuShowPinPicker(uint16_t mask) {
    sPickerVisible = true;
    sPickerOneShot = false;
    UiCall("picker", Fmt("%d,0", mask));
}
void BFMenuShowPinPickerOneShot(uint16_t mask) {
    sPickerVisible = true;
    sPickerOneShot = true;
    UiCall("picker", Fmt("%d,1", mask));
}
void BFMenuHidePinPicker(void) {
    if (!sPickerVisible) return;
    sPickerVisible = false;
    UiCall("hidePicker", "");
}
void BFMenuSetSkipTutorialVisible(bool visible) { UiCall("skip", visible ? "1" : "0"); }

bool BFPrivacyPageVisible(void) { return sPrivacyVisible; }
int BFPrivacyAcceptCount(void) { return sPrivacyAccepted; }
Str BFPrivacyDebug(void) { std::lock_guard<std::mutex> g(sStrLock); return sPrivacyDebug; }
Str BFDeviceLine(void) { std::lock_guard<std::mutex> g(sStrLock); return sDeviceLine; }
int BFScreenMaxHz(void) { return sMaxHz; }

std::vector<uint8_t> BFEncodePNG(const uint8_t *rgba, int w, int h) {
    std::vector<uint8_t> out;
    Env e;
    if (!e.env || !sBP || !sEncodePng || w <= 0 || h <= 0) return out;
    jbyteArray in = e.env->NewByteArray(w * h * 4);
    if (!in) { e.clear(); return out; }
    e.env->SetByteArrayRegion(in, 0, w * h * 4, (const jbyte *)rgba);
    jbyteArray png = (jbyteArray)e.env->CallStaticObjectMethod(sBP, sEncodePng, in, w, h);
    if (!e.clear() && png) {
        jsize n = e.env->GetArrayLength(png);
        out.resize((size_t)n);
        e.env->GetByteArrayRegion(png, 0, n, (jbyte *)out.data());
    }
    e.env->DeleteLocalRef(in);
    if (png) e.env->DeleteLocalRef(png);
    return out;
}

Str BFAppAssetList(void) {
    {
        std::lock_guard<std::mutex> g(sStrLock);
        if (!sAssetListCache.empty()) return sAssetListCache;
    }
    Env e;
    if (!e.env || !sBP || !sAssetList) return Str();
    jstring s = (jstring)e.env->CallStaticObjectMethod(sBP, sAssetList);
    if (e.clear() || !s) return Str();
    Str v = JStr(e.env, s);
    e.env->DeleteLocalRef(s);
    std::lock_guard<std::mutex> g(sStrLock);
    sAssetListCache = v;
    return v;
}

void BFHttpTest(const char *url) {   // called on Log.cpp's worker thread
    Env e;
    if (!e.env || !sBP || !sHttpTest) return;
    jstring u = e.env->NewStringUTF(url);
    e.env->CallStaticVoidMethod(sBP, sHttpTest, u);
    e.clear();
    e.env->DeleteLocalRef(u);
}

// ---------------------------------------------------------------------------
// Java -> native
// ---------------------------------------------------------------------------
static Json StateJson() {          // everything the menu, the oil tab and the library show (on Unity's thread)
    Json s = Json::Obj();
    s.set("status", BFStatusLine());
    s.set("ball", BFBallLine());
    s.set("arsenal", BFArsenalLine());
    s.set("fps", BFFpsLine());
    s.set("oil", BFOilStatusLine());
    s.set("pinImage", BFPinImageStatus());
    s.set("lobby", Json::Bool_(BFPracticeLobbyOpen()));
    s.set("oilReady", Json::Bool_(BFOilReady()));
    s.set("safe", Json::Bool_(gBFSafeMode));
    s.set("offline", Json::Bool_(gBFStatus.offline));
    s.set("cfg", BFConfigJson());
    return s;
}

static Json KegelFromText(const Str &textIn) {   // OilUI.mm KegelFromText, with the shared parser (KegelParse.h)
    Str text;
    for (char c : textIn) if (c != '\r') text += c;
    std::vector<std::string> lines = StrSplit(text, "\n");
    KegelFile f = KegelParseLines(lines);
    if (!f.ok) return Json();
    Json fw = Json::Arr(), rv = Json::Arr();
    for (const KegelFileStep &k : f.fwd) fw.push(JNums({ (double)k.start, (double)k.stop, (double)k.loads, (double)k.speed, k.end, (double)f.ul }));
    for (const KegelFileStep &k : f.rev) rv.push(JNums({ (double)k.start, (double)k.stop, (double)k.loads, (double)k.speed, k.end, (double)f.ul }));
    Str name = f.name;
    if (lines.size() > 2 && KegelTrim(lines[0]) != "-1") {   // the other layout: line 1 is the series, line 2 holds the name
        Str clean = StrStripTags(lines[2]);
        if (!clean.empty()) name = clean;
    }
    Json p = Json::Obj();
    p.set("name", name.empty() ? Str("Kegel pattern") : name);
    p.set("feet", f.feet); p.set("drop", f.drop); p.set("ul", f.ul); p.set("fwd", fw); p.set("rev", rv);
    return p;
}

static bool GameCmd(const Str &cmd, const Str &arg, Str &out) {
    if (cmd == "state") { out = StateJson().Dump(); return true; }
    if (cmd == "debug") { out = BFDebugInfo(); return true; }
    if (cmd == "lobby") { out = BFPracticeLobbyOpen() ? "1" : "0"; return true; }   // the practice pattern screen is open
    if (cmd == "spare") { BFApplySpareSelection((uint16_t)atoi(arg.c_str())); return true; }
    if (cmd == "spareNow") { BFApplySpareSelectionNow((uint16_t)atoi(arg.c_str())); return true; }
    if (cmd == "spareDismissed") { BFSpareDismissed(); return true; }
    if (cmd == "arsenal") { BFSetArsenalQuery(arg); return true; }
    if (cmd == "skipTutorial") { BFRequestSkipTutorial(); return true; }
    if (cmd == "applyHue") { BFOilApplyHue(); return true; }
    if (cmd == "pinTap") {
        float u = 0, v = 0;
        if (sscanf(arg.c_str(), "%f,%f", &u, &v) == 2) BFPinTapAt(u, v);
        return true;
    }
    if (cmd == "setCustom") {
        bool ok = false;
        Json p = arg.empty() ? Json() : Json::Parse(arg, &ok);
        BFOilSetCustom(p.isObj() ? &p : nullptr);
        return true;
    }
    if (cmd == "oilBuiltins") { out = (BFOilReady() ? BFOilBuiltins() : Json::Arr()).Dump(); return true; }
    if (cmd == "builtinSpec") { out = BFOilBuiltinSpec(atoi(arg.c_str())).Dump(); return true; }
    if (cmd == "oilCompute") {   // {base, fwd, rev, drop, exact, feet, precise}; no fwd/rev = the game's own pattern
        if (!BFOilReady()) { out = "null"; return true; }
        Json a = Json::Parse(arg);
        const Json *fwd = a.find("fwd"), *rev = a.find("rev");
        Json r = BFOilCompute(a["base"].i(), fwd, rev, a["drop"].i(), a["exact"].truthy(), a["feet"].i(), a["precise"].truthy());
        out = r.Dump();
        return true;
    }
    return false;
}

static jstring JNICALL N_call(JNIEnv *env, jclass, jstring jcmd, jstring jarg) {
    Str cmd = JStr(env, jcmd), arg = JStr(env, jarg), out;
    // ---- no game access: run right here
    if (cmd == "start") {   // {filesDir, device, maxHz}
        Json a = Json::Parse(arg);
        {
            std::lock_guard<std::mutex> g(sStrLock);
            sDeviceLine = a["device"].str();
        }
        sMaxHz = a["maxHz"].i();
        BFSetDataDir(a["filesDir"].str());
        BFEngineStart();
        sStarted = true;
        return NewJStr(env, BFConfigJson().Dump());
    }
    if (cmd == "config") return NewJStr(env, BFConfigJson().Dump());
    if (cmd == "set") {          // "key=value"
        size_t eq = arg.find('=');
        if (eq != Str::npos && BFConfigSet(arg.substr(0, eq), atof(arg.c_str() + eq + 1))) BFSaveConfig();
        return nullptr;
    }
    if (cmd == "maxHz") { sMaxHz = atoi(arg.c_str()); return nullptr; }
    if (cmd == "log") return NewJStr(env, BFLogText());
    if (cmd == "logEvent") {     // "source\tmessage"
        size_t t = arg.find('\t');
        if (t != Str::npos) BFLogEvent(arg.substr(0, t), arg.substr(t + 1));
        else BFLogEvent("java", arg);
        return nullptr;
    }
    if (cmd == "netTest") { BFNetTest(); return nullptr; }
    if (cmd == "menuVisible") { sMenuVisible = arg == "1"; return nullptr; }
    if (cmd == "pickerHidden") { sPickerVisible = false; return nullptr; }
    if (cmd == "privacy") {      // {visible, accepted, debug}
        Json a = Json::Parse(arg);
        sPrivacyVisible = a["visible"].truthy();
        sPrivacyAccepted = a["accepted"].i();
        std::lock_guard<std::mutex> g(sStrLock);
        sPrivacyDebug = a["debug"].str();
        return nullptr;
    }
    if (cmd == "exitSafe") { BFExitSafeMode(); return nullptr; }
    if (cmd == "pinImagePath") return NewJStr(env, BFPinImagePath());
    if (cmd == "kegelText") return NewJStr(env, KegelFromText(arg).Dump());
    // ---- everything else touches the game: Unity's thread
    if (!sStarted) return nullptr;
    bool known = true;
    int timeout = cmd == "pinTap" ? 0 : (cmd == "debug" || cmd == "oilCompute" || cmd == "oilBuiltins" || cmd == "builtinSpec") ? 4000 : 1500;
    if (timeout == 0) {
        RunOnGame([cmd, arg] { Str o; GameCmd(cmd, arg, o); }, 0);
        return nullptr;
    }
    bool ran = RunOnGame([&] { known = GameCmd(cmd, arg, out); }, timeout);
    if (!ran || !known) return nullptr;
    return out.empty() ? nullptr : NewJStr(env, out);
}

static void JNICALL N_tick(JNIEnv *, jclass) {   // once per frame, on Unity's thread
    sGameTid = gettid();
    DrainJobs();
    if (!sStarted) return;
    BFEngineGuardTick();
    BFEngineTick();
}

static void JNICALL N_drain(JNIEnv *, jclass) {  // just the menu's queued actions
    sGameTid = gettid();
    DrainJobs();
}

static jbyteArray JNICALL N_bytes(JNIEnv *env, jclass, jstring jname) {   // the embedded PNGs (PinGuide.h, Logo.h)
    Str n = JStr(env, jname);
    const unsigned char *p = nullptr;
    unsigned int len = 0;
    if (n == "pinGuide") { p = kBPPinGuidePNG; len = kBPPinGuidePNGLen; }
    else if (n == "pinTemplate") { p = kBPPinTemplatePNG; len = kBPPinTemplatePNGLen; }
    else if (n == "wrapGuide") { p = kBPPinWrapGuidePNG; len = kBPPinWrapGuidePNGLen; }
    else if (n == "wrapTemplate") { p = kBPPinWrapTemplatePNG; len = kBPPinWrapTemplatePNGLen; }
    else if (n == "logo") { p = kBPLogoPNG; len = kBPLogoPNGLen; }
    if (!p) return nullptr;
    jbyteArray a = env->NewByteArray((jsize)len);
    if (a) env->SetByteArrayRegion(a, 0, (jsize)len, (const jbyte *)p);
    return a;
}

// OilUI.mm SavePinImage, the two pixel steps (PinWrap.h, shared with iOS). RGBA rows top-down.
static jbyteArray JNICALL N_pinWrap(JNIEnv *env, jclass, jbyteArray jsrc, jint ww, jint wh, jbyteArray jtmpl, jint side) {
    if (!jsrc || ww <= 0 || wh <= 0 || side <= 0 || env->GetArrayLength(jsrc) < ww * wh * 4) return nullptr;
    std::vector<uint8_t> src((size_t)ww * wh * 4), tmpl, out((size_t)side * side * 4, 0);
    env->GetByteArrayRegion(jsrc, 0, ww * wh * 4, (jbyte *)src.data());
    if (jtmpl && env->GetArrayLength(jtmpl) >= side * side * 4) {
        tmpl.resize((size_t)side * side * 4);
        env->GetByteArrayRegion(jtmpl, 0, side * side * 4, (jbyte *)tmpl.data());
    }
    PinWrapToLayout(src.data(), ww, wh, tmpl.empty() ? nullptr : tmpl.data(), out.data(), side);
    PinFillAround(out.data(), side, 0);
    jbyteArray a = env->NewByteArray(side * side * 4);
    if (a) env->SetByteArrayRegion(a, 0, side * side * 4, (const jbyte *)out.data());
    return a;
}

static void JNICALL N_pinFill(JNIEnv *env, jclass, jbyteArray jpx, jint side, jboolean clean) {
    if (!jpx || side <= 0 || env->GetArrayLength(jpx) < side * side * 4) return;
    jbyte *p = env->GetByteArrayElements(jpx, nullptr);
    if (!p) return;
    PinFillAround((uint8_t *)p, side, clean ? 1 : 0);
    env->ReleaseByteArrayElements(jpx, p, 0);
}

// ---------------------------------------------------------------------------
// start-up
// ---------------------------------------------------------------------------
// Inside JNI_OnLoad, FindClass uses the class loader that is loading this library: the game's own, which
// also holds our classesN.dex.
static jclass FindAppClass(JNIEnv *env, const char *name) {
    jclass c = env->FindClass(name);
    if (env->ExceptionCheck() || !c) {
        env->ExceptionClear();
        LOGE("class %s not found - was the APK patched with tools/patch_apk.py (it adds BowlingPlus's dex)?", name);
        return nullptr;
    }
    return c;
}

static void StartJavaSide(JNIEnv *env) {
    jclass bp = FindAppClass(env, "com/bowlingplus/BP");
    jclass n = FindAppClass(env, "com/bowlingplus/N");
    if (!bp || !n) return;
    static const JNINativeMethod methods[] = {
        { (char *)"call", (char *)"(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", (void *)N_call },
        { (char *)"tick", (char *)"()V", (void *)N_tick },
        { (char *)"drain", (char *)"()V", (void *)N_drain },
        { (char *)"bytes", (char *)"(Ljava/lang/String;)[B", (void *)N_bytes },
        { (char *)"pinWrap", (char *)"([BII[BI)[B", (void *)N_pinWrap },
        { (char *)"pinFill", (char *)"([BIZ)V", (void *)N_pinFill },
    };
    if (env->RegisterNatives(n, methods, sizeof(methods) / sizeof(methods[0])) != JNI_OK) {
        env->ExceptionClear();
        LOGE("RegisterNatives failed");
        return;
    }
    sBP = (jclass)env->NewGlobalRef(bp);
    sUi = env->GetStaticMethodID(bp, "ui", "(Ljava/lang/String;Ljava/lang/String;)V");
    sEncodePng = env->GetStaticMethodID(bp, "encodePng", "([BII)[B");
    sAssetList = env->GetStaticMethodID(bp, "assetList", "()Ljava/lang/String;");
    sHttpTest = env->GetStaticMethodID(bp, "httpTest", "(Ljava/lang/String;)V");
    sPoke = env->GetStaticMethodID(bp, "poke", "()V");
    if (env->ExceptionCheck()) { env->ExceptionClear(); LOGE("a BP bridge method is missing"); }
    jmethodID boot = env->GetStaticMethodID(bp, "boot", "()V");
    if (env->ExceptionCheck() || !boot) { env->ExceptionClear(); LOGE("BP.boot missing"); return; }
    env->CallStaticVoidMethod(bp, boot);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
}

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    sVM = vm;
    jint version = JNI_VERSION_1_6;
    // 1) Unity first, exactly as before
    void *orig = dlopen("libmain_orig.so", RTLD_NOW);
    if (!orig) {
        LOGE("libmain_orig.so not found (%s) - the APK wasn't patched with tools/patch_apk.py?", dlerror());
    } else if (auto onload = (jint (*)(JavaVM *, void *))dlsym(orig, "JNI_OnLoad")) {
        version = onload(vm, reserved);
    }
    // 2) BowlingPlus (never let a failure here stop the game)
    try {
        BFDnsHookStart();   // before the game's first server lookup
        JNIEnv *env = nullptr;
        if (vm->GetEnv((void **)&env, JNI_VERSION_1_6) == JNI_OK && env) StartJavaSide(env);
    } catch (...) {
        LOGE("start-up failed");
    }
    return version;
}
