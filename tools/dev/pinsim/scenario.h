// The fake lane: racks exactly like the game's RunPsycsTest.UpdatePinPositions (decoded from 1.907): copy _kegsUp
// into mdl_ShootCurrentData.CurrentData.Before, then for each kegel in order move it, draw Random.Range(0, 16)
// through the game's engine-function pointer, and set rotation = turn about world Z (no lean).
static std::vector<int32_t> gDraws;                  // everything the engine drew, in order
static uint32_t gSeed = 20261006;
static int32_t EngineRandomRangeInt(int32_t lo, int32_t hi) {
    gSeed = gSeed * 1103515245u + 12345u;
    int32_t r = lo + (int32_t)((gSeed >> 16) % (uint32_t)(hi - lo));
    gDraws.push_back(r);
    return r;
}
static BFRandIntFn gSlot = EngineRandomRangeInt;     // the game's pointer (the one RandSlotIn finds)
// Random.Range(0, 16)'s one call instruction in UpdatePinPositions (noinline: a real call with a return address)
__attribute__((noinline)) static int32_t UPPRange(int32_t lo, int32_t hi) { int32_t v = gSlot(lo, hi); __asm__ volatile(""); return v; }
// another place in the game that draws Random.Range(0, 16)
__attribute__((noinline)) static int32_t OtherRange(int32_t lo, int32_t hi) { int32_t v = gSlot(lo, hi); __asm__ volatile(""); return v; }
static uintptr_t gProbe;
__attribute__((noinline)) static int32_t Probe(int32_t, int32_t) { gProbe = (uintptr_t)__builtin_return_address(0); return 0; }

static bool gKegsUp[kPins];
static FakeQuat Rot(float turnDeg, float leanDeg) {       // turn about Z, then lean about X
    float t = turnDeg * 0.0174533f * 0.5f, l = leanDeg * 0.0174533f * 0.5f;
    FakeQuat tz = { 0, 0, sinf(t), cosf(t) }, lx = { sinf(l), 0, 0, cosf(l) };
    return { lx.w * tz.x + lx.x * tz.w + lx.y * tz.z - lx.z * tz.y, lx.w * tz.y - lx.x * tz.z + lx.y * tz.w + lx.z * tz.x,
             lx.w * tz.z + lx.x * tz.y - lx.y * tz.x + lx.z * tz.w, lx.w * tz.w - lx.x * tz.x - lx.y * tz.y - lx.z * tz.z };
}
static float TurnOf(int i) {                                // degrees about Z of a pin with no lean
    const FakeQuat &q = gKegel[i].rot;
    float a = 2.0f * atan2f(q.z, q.w) * 57.29578f;
    while (a < 0) a += 360.0f;
    while (a >= 359.99f) a -= 360.0f;
    return a;
}
// The pinsetter models: their own local rotation and their joints' world rotation at the pick-up / placing pose
// (both read from the game's scene and its goDownTakePins / goDownPlaceKegs clips, see VERIFIED_NOTES 5b).
static const FakeQuat kSetterBase = { 0.9997595f, 0.0143005f, -0.0166272f, -0.0002378f };
static const FakeQuat kSetterJoint = { 0.9997596f, -0.0166267f, -0.0143009f, 0.0002377f };
static const FakeQuat kMirrorBase = { -0.9997595f, -0.0143005f, 0.0166272f, 0.0002378f };
static const FakeQuat kMirrorJoint = { -0.0143011f, 0.0002377f, -0.9997595f, 0.0166267f };
static FakeQuat Mul(FakeQuat a, FakeQuat b) {
    return { a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
             a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z };
}
static void RotV(FakeQuat q, const float v[3], float out[3]) {
    FakeQuat p = { v[0], v[1], v[2], 0 }, c = { -q.x, -q.y, -q.z, q.w }, r = Mul(Mul(q, p), c);
    out[0] = r.x; out[1] = r.y; out[2] = r.z;
}
// Which way a pin's printed front faces (degrees, in the floor plane), given its mesh root's world rotation. Every
// pin model's mesh child sits at local Rx(90) and the mesh stands along its own Y, so its own Z is a fixed
// sideways direction on the print.
static float Facing(FakeQuat root) {
    const FakeQuat rx90 = { 0.70710678f, 0, 0, 0.70710678f };
    const float f[3] = { 0, 0, 1 };
    float o[3]; RotV(Mul(root, rx90), f, o);
    return atan2f(o[1], o[0]) * 57.29578f;
}
static float AngDiff(float a, float b) { float d = fmodf(fabsf(a - b), 360.0f); return d > 180 ? 360 - d : d; }
static float RealFacing(int i) { return Facing(gKegel[i].rot); }                    // real pin (visual root = body)
static float SetterFacing(int i) { return Facing(Mul(kSetterJoint, gSetter[i].local)); }
static float MirrorFacing(int i) { return Facing(Mul(kMirrorJoint, gMirror[i].local)); }   // its reflection's front
static float FacingOfTurn(int turn) { return Facing(Rot(turn * 22.5f, 0)); }

static void Ticks(double seconds) {                       // BowlingPlus's regular tick while time passes
    for (double t = 0; t < seconds; t += 0.05) { gClock += 0.05; PinTurnTick(); }
}

static int gRacks = 0, gCallsByRacks = 0, gCallsOther = 0;
static void GameRack(int stopAfter = kPins) {            // UpdatePinPositions (stopAfter < 10: an exception mid-loop)
    gRacks++;
    for (int i = 0; i < kPins; i++) ((bool *)Data(gBefore))[i] = gKegsUp[i];
    for (int i = 0; i < kPins && i < stopAfter; i++) {
        int r = UPPRange(0, 16);
        gCallsByRacks++;
        gKegel[i].rot = Rot(r * 360.0f * 0.0625f, 0);
        gClock += 0.00002;                                  // the loop takes microseconds per kegel
    }
}
static float Rnd(float a, float b) { gSeed = gSeed * 1664525u + 1013904223u; return a + (b - a) * ((gSeed >> 8) & 0xFFFF) / 65535.0f; }
// A throw: standing pins wobble (up to 3 degrees of lean, a few degrees of twist); knocked pins end up lying.
// Then the game updates _kegsUp (or stands all 10 when the frame ends) and Reset racks again.
static std::vector<int> gLifted;                         // pins the pinsetter showed during the last throw's cycle
static float gLiftedFacing[kPins], gMirrorFacingShown[kPins];
static bool gChangedWhileShown = false;
// The pinsetter cycle between a throw and the next rack: it shows its models for the pins that will stand
// (standing pins lifted for the second ball, or the full new rack on the way down), then hides them.
static void PinsetterShows(const std::vector<int> &pins) {
    gLifted = pins;
    for (int p : pins) { gSetter[p - 1].active = gMirror[p - 1].active = true; gLiftedFacing[p - 1] = SetterFacing(p - 1); gMirrorFacingShown[p - 1] = MirrorFacing(p - 1); }
    Ticks(2.0);                                           // BowlingPlus keeps ticking while they're shown
    for (int p : pins) if (AngDiff(SetterFacing(p - 1), gLiftedFacing[p - 1]) > 0.01f) gChangedWhileShown = true;
    for (int p : pins) { gSetter[p - 1].active = gMirror[p - 1].active = false; }
}
static void Throw(std::vector<int> knock, bool frameEnds) {
    Ticks(2.0);
    for (int i = 0; i < kPins; i++) {
        if (!gKegsUp[i]) continue;
        bool k = false;
        for (int p : knock) if (p == i + 1) k = true;
        float turn = TurnOf(i) + Rnd(-6, 6);
        gKegel[i].rot = k ? Rot(turn + Rnd(0, 360), Rnd(60, 90)) : Rot(turn, Rnd(0, 3));
        if (k) gKegsUp[i] = false;
    }
    Ticks(2.0);                                           // pins settle, the pinsetter comes down
    std::vector<int> shown;
    for (int i = 0; i < kPins; i++) if (frameEnds || gKegsUp[i]) shown.push_back(i + 1);
    if (frameEnds) for (int i = 0; i < kPins; i++) gKegsUp[i] = true;
    PinsetterShows(shown);
    GameRack();
}
static void Pickup() { Ticks(1.5); GameRack(); }          // ChangeBall: same pins up
static std::vector<float> Turns() { std::vector<float> t; for (int i = 0; i < kPins; i++) t.push_back(TurnOf(i)); return t; }
static std::vector<int32_t> LastRackDraws() { return std::vector<int32_t>(gDraws.end() - kPins, gDraws.end()); }

static void SetupGame() {
    N.InventaryData = &gClassInv;
    N.GO_getTransform = &gMethGetTr;
    N.Tr_getRot = &gMethGetRot;
    N.GO_activeH = &gMethActiveH;
    N.Tr_getLocalRot = &gMethGetLocal;
    N.Tr_setLocalRot = &gMethSetLocal;
    gSetterArr = NewArr(kPins, sizeof(void *));
    gMirrorArr = NewArr(kPins, sizeof(void *));
    for (int i = 0; i < kPins; i++) {
        gSetter[i].local = kSetterBase; gMirror[i].local = kMirrorBase;
        ((void **)Data(gSetterArr))[i] = &gSetter[i];
        ((void **)Data(gMirrorArr))[i] = &gMirror[i];
    }
    At<Il2CppArray *>(gInvObj, 0xC8) = gSetterArr;
    At<Il2CppArray *>(gSetterMgrObj, 0x50) = gMirrorArr;
    gMgrList = NewArr(1, sizeof(void *));
    ((void **)Data(gMgrList))[0] = gSetterMgrObj;
    gKegelsArr = NewArr(kPins, sizeof(void *));
    for (int i = 0; i < kPins; i++) ((void **)Data(gKegelsArr))[i] = &gKegel[i];
    At<Il2CppArray *>(gInvObj, 0x88) = gKegelsArr;
    gBefore = NewArr(kPins, 1);
    At<Il2CppArray *>(gShotObj, 0x20) = gBefore;
    for (int i = 0; i < kPins; i++) gKegsUp[i] = true;
}
// What TurnHookInstall does once it has found these in the game's code (tested on the real binaries separately)
static void InstallHook() {
    gSlot = Probe; UPPRange(0, 16); uintptr_t site = gProbe;
    gSlot = EngineRandomRangeInt;
    sRandEngine = gSlot; sRandSlot = &gSlot;
    sTurnSite[0] = site; sTurnSites = 1;
    gSlot = BFRandomRangeInt;
    sTurnHook = 1; sTurnWhy = "in place";
    PinTurnTick();
}
