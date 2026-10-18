// Fake game for the obstacle rack check (tools/tests/rack): exactly what the live "new layout only for a new rack"
// code in Game.cpp / Game.mm uses. A lane of ten pins, a game with frames (the 10th with its bonus balls), the
// locations the game goes through around a throw, and a RoBuild that hands out numbered layouts.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
enum { LOC_UPPER_SCREEN = 0, LOC_BALL_RETURNER = 1, LOC_BOTTOM_MONITOR = 2, LOC_START_POS = 3,
       LOC_THROWING = 4, LOC_ON_PINDECK = 5, LOC_REPLAYER = 6 };
struct ObjRef { void *p = nullptr; void *get() { return p; } void set(void *o) { p = o; } };
static struct { bool roRamps = true, roStools = true, roGates = true; } gBF;
struct RoShot { int kind, layout; bool moving; float moveA, moveBaseX; double moveT0; };
static RoShot sRoLive = { 0, -1, false, 0, 0, 0 };
static int sRoShowing = -1;
static int sRoParts = 0, sFrame = 0, sLoc = LOC_START_POS, sRoLastThrowFrame = -1000;
static bool sLaneRef = true, sRoThrew = true;

// the fake game
static char gGames[16][16];
static int gGame = 0, gCur = 0;
static bool gEnd = false, gInfoWorks = true;
struct BFVec3 { float x, y, z; };
// _kegsUp, as the game keeps it: written only when it racks (a pickup: the pins left standing; a new frame or a new
// rack: all ten, directly, so after a strike it never shows them down). gHit: the pins knocked over on the deck now.
static bool gUp[10] = { true, true, true, true, true, true, true, true, true, true };
static bool gHit[10] = {};
static bool gLifted = false;    // the pinsetter has taken the standing pins (under the floor, as far as the kegels go)
static uint16_t RoPinsUp() { uint16_t m = 0; for (int i = 0; i < 10; i++) if (gUp[i]) m |= 1u << i; return m; }
static bool RoPinState(int i, BFVec3 &pos, float &lean) {
    pos = BFVec3{ (float)(i % 4) * 0.3f, 18.0f + (float)(i / 4) * 0.26f, gUp[i] ? -0.0005f : -200.0f };
    lean = 0;
    if (gHit[i]) { pos.x += 0.4f; pos.z = 0.05f; lean = 85; }
    else if (gLifted) pos.z = -200;
    return true;
}
// the clock and the ball, for the pin watch's window (the ball is at the pins from the deck on)
static double gClock = 0;
#define BP175_CLOCK gClock
static float sHeadY = 18.263f;
static bool gBallAtPins = false;
static bool BpBallPos(BFVec3 &p) { p = BFVec3{ 0, gBallAtPins ? 18.3f : 5.0f, 0.1f }; return true; }
static int gBuilds = 0, gRestores = 0, gNextLayout = 1, gLastFrameBuilds = 0, gBuildKind = 2;
static bool RoFullRack() { return RoPinsUp() == 0x3FF; }
static void *GmGame() { return gGames[gGame]; }
static bool GmGameInfo(void *g, int &cur, int &count, bool &end) {
    if (!gInfoWorks || !g) return false;
    cur = gCur; count = 10; end = gEnd;
    return true;
}
static bool RoBuild(bool lastFrame) {
    gBuilds++;
    if (lastFrame) gLastFrameBuilds++;
    sRoLive = RoShot{ gBuildKind, gNextLayout++, false, 0, 0, 0 };
    sRoParts = 3;
    return true;
}
static bool RoBuildShot(const RoShot &sh, bool) {
    gRestores++;
    sRoLive = sh;
    sRoParts = 3;
    return true;
}
