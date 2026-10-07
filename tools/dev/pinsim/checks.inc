static int gFails = 0, gChecks = 0;
static void Check(const char *what, bool ok) { gChecks++; printf("%-100s %s\n", what, ok ? "ok" : "FAIL"); if (!ok) gFails++; }
static bool Same(const std::vector<float> &a, const std::vector<float> &b, std::vector<int> only = {}) {
    for (int i = 0; i < kPins; i++) {
        if (!only.empty()) { bool in = false; for (int p : only) if (p == i + 1) in = true; if (!in) continue; }
        float d = fabsf(a[i] - b[i]); if (d > 180) d = 360 - d;
        if (d > 0.01f) return false;
    }
    return true;
}
static std::vector<int> All() { return { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }; }
static std::vector<int> Except(std::vector<int> out) { std::vector<int> r; for (int p : All()) { bool x = false; for (int o : out) if (o == p) x = true; if (!x) r.push_back(p); } return r; }
// What the player sees: every pin the pinsetter showed during the last cycle faced exactly like the real pin that
// then stood in its place, and so did its reflection.
static float gWorstSetter = 0, gWorstMirror = 0;
static bool PinsetterMatched() {
    bool ok = !gLifted.empty();
    for (int p : gLifted) {
        float d = AngDiff(gLiftedFacing[p - 1], RealFacing(p - 1)), m = AngDiff(gMirrorFacingShown[p - 1], RealFacing(p - 1));
        if (d > gWorstSetter) gWorstSetter = d;
        if (m > gWorstMirror) gWorstMirror = m;
        if (d > 0.5f || m > 0.5f) ok = false;
    }
    return ok;
}

int main() {
    SetupGame();
    InstallHook();
    sMode = MODE_FUN; PinTurnTick();
    Check("Practice: keeping turns", sTurnKeep);

    GameRack();
    std::vector<float> t0 = Turns();
    int new0 = sTurnNew;
    for (int k = 0; k < 20; k++) Pickup();
    Check("20 ball pickups: no standing pin turns", Same(Turns(), t0));
    Check("the game still draws its random numbers (10 per rack, sequence unchanged)", (int)gDraws.size() == gCallsByRacks);
    Check("... and no new turns were handed out", sTurnNew == new0);

    Throw({ 1, 2, 4 }, false);
    Check("ball 1 knocks 1, 2, 4: the rack for ball 2 keeps every standing pin's turn", Same(Turns(), t0, Except({ 1, 2, 4 })));
    Check("the pinsetter lifted the 7 standing pins showing their own turns (reflections too)", gLifted.size() == 7 && PinsetterMatched());
    for (int k = 0; k < 5; k++) Pickup();
    Check("pickups before ball 2: standing pins keep their turns", Same(Turns(), t0, Except({ 1, 2, 4 })));
    int new1 = sTurnNew;
    Throw({ 3, 5, 6, 7, 8, 9, 10 }, true);
    std::vector<float> t1 = Turns();
    Check("spare, new frame: all 10 pins get new turns", sTurnNew - new1 == 10);
    Check("the pinsetter set the new rack down already showing those new turns", gLifted.size() == 10 && PinsetterMatched());
    for (int k = 0; k < 5; k++) Pickup();
    Check("pickups in the new frame: no turns", Same(Turns(), t1));

    Throw({}, false);
    Check("gutter ball 1: the rack for ball 2 keeps all 10 turns", Same(Turns(), t1));
    Check("... and the pinsetter lifted all 10 showing them", gLifted.size() == 10 && PinsetterMatched());
    int new2 = sTurnNew;
    Throw({ 7, 10 }, true);
    std::vector<float> t2 = Turns();
    Check("open frame (ball 2 knocks 7, 10): the next frame keeps the 8 pins that never fell", Same(t2, t1, Except({ 7, 10 })));
    Check("... 7 and 10 (knocked over) get new turns", sTurnNew - new2 == 2);
    Check("... and the pinsetter's new rack matched all 10", PinsetterMatched());

    int new3 = sTurnNew;
    Throw(All(), true);
    std::vector<float> t3 = Turns();
    Check("strike: all 10 get new turns in the next frame", sTurnNew - new3 == 10);
    Check("... shown that way by the pinsetter on the way down", PinsetterMatched());

    Ticks(2.0); GameRack(); gClock += 0.0001; GameRack();
    Check("two racks back to back (Reset then ChangeBall in one frame): nothing turns", Same(Turns(), t3));

    Ticks(1.0);
    int other0 = sTurnOther;
    int32_t o = OtherRange(0, 16);
    Check("Random.Range(0, 16) from anywhere else: the game's own number, untouched", o == gDraws.back() && sTurnOther == other0 + 1);
    Pickup();
    Check("... and the next rack still matches each pin to its own turn", Same(Turns(), t3));

    Ticks(1.0); GameRack(4);                             // an exception after 4 kegels
    Pickup();
    Check("a rack cut short (exception mid-loop): the next rack lines up again", Same(Turns(), t3));

    // spare mode / pin picker: only 7 and 10 up, then a full rack
    for (int i = 0; i < kPins; i++) gKegsUp[i] = (i == 6 || i == 9);
    Ticks(2.0); GameRack();
    Check("spare-mode rack (only 7, 10 up): 7 and 10 keep their turns", Same(Turns(), t3, { 7, 10 }));
    for (int i = 0; i < kPins; i++) gKegsUp[i] = true;
    int new4 = sTurnNew;
    Ticks(2.0); GameRack();
    std::vector<float> t4 = Turns();
    Check("full rack after it: 7, 10 keep, the 8 that were off the lane get new turns", Same(t4, t3, { 7, 10 }) && sTurnNew - new4 == 8);

    Check("the pinsetter's models never turned while they were being shown", !gChangedWhileShown);

    // online: the game's own turns (remembered), pinsetter models back to the game's own; then back to Practice
    sMode = MODE_COMPETE; PinTurnTick();
    Check("online: not keeping", !sTurnKeep);
    Pickup();
    std::vector<float> on1 = Turns();
    std::vector<int32_t> last(gDraws.end() - kPins, gDraws.end());
    bool own = true;
    for (int i = 0; i < kPins; i++) if (AngDiff(on1[i], last[i] * 22.5f) > 0.01f) own = false;
    Check("online pickup: the game's own random turns", own);
    bool base = true;
    for (int i = 0; i < kPins; i++) {
        FakeQuat a = gSetter[i].local, b = gMirror[i].local;
        if (fabsf(a.x - kSetterBase.x) + fabsf(a.y - kSetterBase.y) + fabsf(a.z - kSetterBase.z) + fabsf(a.w - kSetterBase.w) > 1e-5f) base = false;
        if (fabsf(b.x - kMirrorBase.x) + fabsf(b.y - kMirrorBase.y) + fabsf(b.z - kMirrorBase.z) + fabsf(b.w - kMirrorBase.w) > 1e-5f) base = false;
    }
    Check("online: the pinsetter's models are back to the game's own", base);
    sMode = MODE_FUN; PinTurnTick();
    Pickup();
    Check("back in Practice: the first pickup keeps what the game last showed", Same(Turns(), on1));
    Throw({ 2 }, false);
    Check("... and the pinsetter lifts them showing those turns", PinsetterMatched());

    // reads that fail: never a new turn for a standing pin
    gLeanReadWorks = false;
    std::vector<float> t5 = Turns();
    for (int k = 0; k < 5; k++) Pickup();
    Check("lean can't be read: standing pins still keep their turns", Same(Turns(), t5));
    gLeanReadWorks = true;
    gStaticLookupWorks = false; sShotBefore = -2; sShotData = nullptr;
    for (int k = 0; k < 5; k++) Pickup();
    Check("which pins are up can't be read: standing pins still keep their turns", Same(Turns(), t5));
    int new5 = sTurnNew;
    Throw(All(), true);
    // (pin 2 was off the lane before this strike; with the up-state unreadable it can't be told from a standing pin,
    //  so it keeps its turn: only the 9 that fell get new ones)
    Check("... and a strike still gives the 9 fallen pins new turns, matched by the pinsetter", sTurnNew - new5 == 9 && PinsetterMatched());
    gStaticLookupWorks = true; sShotBefore = -2;

    // the sweeper's banner: moved once, to the front of the bar, sized to its window
    for (int k = 0; k < 130; k++) { sFrame++; PinTurnTick(); }
    Check("sweeper banner: Logo moved in front of the bar (z -12) and sized to the window",
          fabsf(gLogoTr.pos.z + 12.0f) < 1e-4f && fabsf(gLogoTr.scale.x - 244.4f / 256.0f) < 1e-4f && fabsf(gLogoTr.scale.y - 21.3f / 22.0f) < 1e-4f);
    Check("sweeper banner: the timer Bar too (its own height, 23)", fabsf(gBarTr.pos.z + 12.0f) < 1e-4f && fabsf(gBarTr.scale.y - 21.3f / 23.0f) < 1e-4f);
    int sets = gLogoTr.posSets;
    for (int k = 0; k < 600; k++) { sFrame++; PinTurnTick(); }
    Check("sweeper banner: not set again while it stays there", gLogoTr.posSets == sets);
    gLogoTr.pos.z = 0;                                   // as if the scene loaded again
    for (int k = 0; k < 130; k++) { sFrame++; PinTurnTick(); }
    Check("sweeper banner: put back after the scene reloads", fabsf(gLogoTr.pos.z + 12.0f) < 1e-4f && gLogoTr.posSets == sets + 1);
    Check("the engine drew exactly one number per call (sequence unchanged)", (int)gDraws.size() == gCallsByRacks + 1 /*OtherRange*/);
    char dbg[1024]; TurnDebug(dbg, sizeof(dbg));
    printf("\nworst pinsetter / reflection mismatch seen: %.3f / %.3f degrees\n%s\n", gWorstSetter, gWorstMirror, dbg);
    printf("\n%d checks, %d failed\n", gChecks, gFails);
    return gFails != 0;
}
