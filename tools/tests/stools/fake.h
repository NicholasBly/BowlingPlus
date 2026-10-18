// Fake ball and engine for the barstool check (tools/tests/stools): exactly what the live RoStoolBounce and
// RoStoolsTick use. The ball is a point on the lane (lane coordinates: x across from the centre, y from the foul
// line) that the engine moves by its velocity; BowlingPlus's set_position / set_velocity / angular velocity calls
// land on it. No collider of the stool's own (the game has none either: RoStoolBounce is the stool).
#include <cstdio>
#include <cstdint>
#include <cstdarg>
#include <cmath>
#include "../../../src/RampLayouts.h"
typedef void MethodInfo;
struct Il2CppObject { void *klass; void *monitor; };
struct BFVec3 { float x, y, z; };
static int gSetVel, gSetPos, gGetAng, gSetAng;
static struct { const MethodInfo *RB_setVel = &gSetVel, *RB_getAngVel = &gGetAng, *RB_setAngVel = &gSetAng; } N;
static const MethodInfo *sRoRbSetPos = &gSetPos;
static struct Ball { BFVec3 p, v, w; } gBall;
static char gRb;
static struct { Il2CppObject h; BFVec3 v; } gBox;
static void *BallBody() { return &gRb; }
static bool BpBallVel(void *, BFVec3 &v) { v = gBall.v; return true; }
static Il2CppObject *Invoke(const MethodInfo *m, void *, void **a, bool *ok = nullptr) {
    if (ok) *ok = true;
    if (m == &gSetVel) gBall.v = *(BFVec3 *)a[0];
    else if (m == &gSetPos) gBall.p = *(BFVec3 *)a[0];
    else if (m == &gSetAng) gBall.w = *(BFVec3 *)a[0];
    else if (m == &gGetAng) { gBox.v = gBall.w; return &gBox.h; }
    return nullptr;
}
static void *Unbox(Il2CppObject *o) { return (char *)o + sizeof(Il2CppObject); }
static uint32_t gRand = 12345;
static uint32_t RoRand() { gRand ^= gRand << 13; gRand ^= gRand >> 17; gRand ^= gRand << 5; return gRand; }
static bool gQuiet = true;
static void Bp175Log(const char *fmt, ...) { if (gQuiet) return; va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); printf("\n"); }
static int gGutters = 0;
static void RoToGutter(const BFVec3 &, const char *, float = 0) { gGutters++; gBall.v = BFVec3{ 0, 3, 0 }; gBall.p.x = 0.65f; }
// the lane (its centre at x 0, the foul line at y 0) and the throw's state, as in the obstacle block
static float sLaneX = 0, sFoulY = 0, sHeadY = 18.263f;
static bool sLaneRef = true;
static int sRoLayout = 0, sRoStoolHits = 0, sRoBounces = 0, sRoPassed = 0, sRoLateGutters = 0;
static float sRoStoolMin = 9;
static double gClock = 0, sRoStoolT = 0;
static float sRoStoolV = 0;
#define BP175_CLOCK gClock
static bool sRoStoolHit = false, sRoDecided = false;
static BFVec3 sRoPrev;
