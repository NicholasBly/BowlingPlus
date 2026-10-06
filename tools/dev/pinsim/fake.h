// Fake game for the pin-turn simulation (tools/dev/pinsim). Provides exactly what the shared pin-turn block in
// Game.cpp / Game.mm uses, with real memory layouts for the objects it reads (IL2CPP arrays: data at +0x20).
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
typedef void Il2CppClass; typedef void MethodInfo; typedef void FieldInfo;
struct Il2CppObject { void *klass; void *monitor; };
struct Il2CppArray { Il2CppObject obj; void *bounds; uintptr_t max_length; };
template <typename T> inline T &At(void *obj, int offset) { return *(T *)((char *)obj + offset); }
inline void *Data(Il2CppArray *a) { return (char *)a + sizeof(Il2CppArray); }
inline size_t Len(Il2CppArray *a) { return a ? (size_t)a->max_length : 0; }
static void *Elem(Il2CppArray *a, size_t i) { return ((void **)Data(a))[i]; }
struct Ref { void *p = nullptr; void *get() { return p; } void set(void *o) { p = o; } };
enum { MODE_FUN = 0, MODE_COMPETE = 2 };
static bool sSettled = true;
static int sFrame = 0;
static int sMode = MODE_FUN;
static double gClock = 1000.0;
#define PIN_TURN_CLOCK gClock
#define PIN_TURN_LOG "pin turns: %s"
static void BFLog(const char *fmt, const char *s) { printf("  [log] "); printf(fmt, s); printf("\n"); }

// ---- the fake scene ----
static const int kPins = 10;
struct FakeQuat { float x, y, z, w; };
struct Kegel { FakeQuat rot; FakeQuat local = { 0, 0, 0, 1 }; bool goAlive = true; bool active = false; };   // a GameObject + its transform
static Kegel gKegel[kPins];
static Kegel gSetter[kPins], gMirror[kPins];   // InventaryData.pinseterKegelRenderer, PinSetterManager.mirrorOnPinsetter
static int gClassInv, gClassShot, gClassShotData, gClassRandom, gFieldInst, gFieldCur;
static int gMethGetTr, gMethGetRot, gMethGetLocal, gMethSetLocal, gMethActiveH, gClassSetterMgr;
static Il2CppArray *NewArr(size_t n, size_t elem) { Il2CppArray *a = (Il2CppArray *)calloc(1, sizeof(Il2CppArray) + n * elem); a->max_length = n; return a; }
static char gInvObj[0x200];                     // InventaryData: kegels at 0x88
static char gShotObj[0x40];                     // mdl_ShootData: Before at 0x20
static char gSetterMgrObj[0x100];                // PinSetterManager: mirrorOnPinsetter at 0x50
static Il2CppArray *gKegelsArr, *gBefore, *gSetterArr, *gMirrorArr;
static bool gStaticLookupWorks = true, gLeanReadWorks = true;
static struct { FieldInfo *invd_instance; Il2CppClass *InventaryData; const MethodInfo *GO_getTransform, *Tr_getRot, *RPT_UpdatePinPositions, *GO_activeH, *Tr_getLocalRot, *Tr_setLocalRot; } N;
static Il2CppClass *FindClass(const char *, const char *name) {
    if (!strcmp(name, "mdl_ShootCurrentData")) return gStaticLookupWorks ? &gClassShot : nullptr;
    if (!strcmp(name, "mdl_ShootData")) return &gClassShotData;
    if (!strcmp(name, "Random")) return &gClassRandom;
    if (!strcmp(name, "PinSetterManager")) return &gClassSetterMgr;
    return nullptr;
}
static const MethodInfo *FindMethod(Il2CppClass *, const char *, int, const char * = nullptr, const char * = nullptr) { return nullptr; }
static bool FieldTypeName(Il2CppClass *k, const char *name, char *out, size_t size) {
    if (k == &gClassInv && !strcmp(name, "kegels")) { snprintf(out, size, "UnityEngine.GameObject[]"); return true; }
    if (k == &gClassShotData && !strcmp(name, "Before")) { snprintf(out, size, "System.Boolean[]"); return true; }
    if (k == &gClassInv && !strcmp(name, "pinseterKegelRenderer")) { snprintf(out, size, "UnityEngine.GameObject[]"); return true; }
    if (k == &gClassSetterMgr && !strcmp(name, "mirrorOnPinsetter")) { snprintf(out, size, "UnityEngine.GameObject[]"); return true; }
    return false;
}
static int FieldOffset(Il2CppClass *k, const char *name) {
    if (k == &gClassInv && !strcmp(name, "kegels")) return 0x88;
    if (k == &gClassShotData && !strcmp(name, "Before")) return 0x20;
    if (k == &gClassInv && !strcmp(name, "pinseterKegelRenderer")) return 0xC8;
    if (k == &gClassSetterMgr && !strcmp(name, "mirrorOnPinsetter")) return 0x50;
    return -1;
}
static FieldInfo *StaticField(Il2CppClass *k, const char *name) { return (k == &gClassShot && !strcmp(name, "CurrentData")) ? &gFieldCur : nullptr; }
static void StaticRead(FieldInfo *f, void *out) { *(void **)out = (f == &gFieldCur) ? (void *)gShotObj : nullptr; }
static void *ReadStaticObj(FieldInfo *&f, Il2CppClass *, const char *) { f = &gFieldInst; return gInvObj; }
static FakeQuat gBoxed;
static Il2CppObject *Invoke(const MethodInfo *m, void *obj, void **args, bool *ok = nullptr) {
    if (ok) *ok = false;
    if (m == &gMethGetTr) {                                     // GameObject.get_transform: the transform is the kegel itself
        Kegel *k = (Kegel *)obj;
        if (!k->goAlive) return nullptr;                         // (a destroyed object throws: runtime_invoke reports !ok)
        if (ok) *ok = true;
        return (Il2CppObject *)obj;
    }
    if (m == &gMethGetLocal) {
        gBoxed = ((Kegel *)obj)->local;
        if (ok) *ok = true;
        return (Il2CppObject *)&gBoxed;
    }
    if (m == &gMethSetLocal) {
        ((Kegel *)obj)->local = *(FakeQuat *)((void **)args)[0];
        if (ok) *ok = true;
        return nullptr;
    }
    if (m == &gMethGetRot) {
        if (!gLeanReadWorks) return nullptr;
        gBoxed = ((Kegel *)obj)->rot;
        if (ok) *ok = true;
        return (Il2CppObject *)&gBoxed;
    }
    return nullptr;
}
static void *Unbox(Il2CppObject *b) { return b; }
static bool InvokeBool(const MethodInfo *m, void *obj, void **, bool def) { return m == &gMethActiveH ? ((Kegel *)obj)->active : def; }
static Il2CppObject *TypeOf(Il2CppClass *k) { return (Il2CppObject *)k; }
static Il2CppArray *gMgrList;
static Il2CppArray *FindAll(Il2CppObject *type) { return type == (Il2CppObject *)&gClassSetterMgr ? gMgrList : nullptr; }
static void *FirstAlive(Il2CppArray *a) { return a && Len(a) ? Elem(a, 0) : nullptr; }
static void *Api(const char *) { return nullptr; }
