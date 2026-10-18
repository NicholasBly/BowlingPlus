// The driver and the checks for tools/tests/rack (included after the live code).
static int gChecks = 0, gBad = 0;
static void Check(bool ok, const char *what) {
    gChecks++;
    if (!ok) gBad++;
    printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
}

// One frame of RampsTick as the real one orders it outside the replayer: the fall watch, then either the throw (which
// marks the time) or the rack check.
static void Tick() {
    sFrame++;
    gClock += 1.0 / 60;
    gBallAtPins = sLoc == LOC_ON_PINDECK;
    RoFallTick();
    if (sLoc == LOC_THROWING || sLoc == LOC_ON_PINDECK) { sRoLastThrowFrame = sFrame; RoPinsTick(); return; }
    RoRackTick();
}
static void Run(int loc, int frames) { sLoc = loc; for (int i = 0; i < frames; i++) Tick(); }
static int Layout() { return sRoParts ? sRoLive.layout : 0; }

// the game's scoring, as far as racks go
static int gBall = 0;           // ball in this frame
static bool gFirstStrike = false;
static int gFrameLag = 0;
static bool gPinsetterLate = false;   // the pinsetter takes the standing pins 3 s after the ball reached them       // > 0: the game moves to the next frame this many frames after the new rack stands

static void NewGame(bool newObject) {
    if (newObject) gGame++;
    gCur = 0; gEnd = false; gBall = 0; gFirstStrike = false;
    for (int i = 0; i < 10; i++) { gUp[i] = true; gHit[i] = false; }
    sRoThrew = true;            // (GameModeTick's restart: RoClear + sRoThrew)
    sRoParts = 0;
}

// A ball: hold it, let go (RoThrowTick marks the throw), the pins in `knock` that still stand go down, then the game
// scores it and the pinsetter does what it does. Returns the layout the ball was thrown at.
static int Bowl(unsigned knock) {
    Run(LOC_START_POS, 120);
    int at = Layout();
    sLoc = LOC_THROWING; Tick();
    sRoThrew = true;            // released (RoThrowTick)
    RoPinsStart();
    Run(LOC_THROWING, 90);
    Run(LOC_ON_PINDECK, 40);
    for (int i = 0; i < 10; i++) if (knock >> i & 1 && gUp[i]) gHit[i] = true;
    Run(LOC_ON_PINDECK, 80);
    if (gPinsetterLate) { Run(LOC_ON_PINDECK, 100); gLifted = true; Run(LOC_ON_PINDECK, 60); }
    int left = 0;
    for (int i = 0; i < 10; i++) left += gUp[i] && !gHit[i];
    bool down = left == 0, newRack = false, nextFrame = false;
    if (gCur < 9) {
        if (down || gBall == 1) { newRack = nextFrame = true; }
        else gBall = 1;
    } else {
        if (gBall == 0) { gFirstStrike = down; newRack = down; gBall = 1; }
        else if (gBall == 1) {
            if (down) { newRack = true; gBall = 2; }
            else if (gFirstStrike) gBall = 2;
            else gEnd = true;
        } else gEnd = true;
    }
    Run(LOC_BALL_RETURNER, 60);  // the sweep, then the game racks
    for (int i = 0; i < 10; i++) { gUp[i] = (newRack && !gEnd) || (gUp[i] && !gHit[i]); gHit[i] = false; }
    gLifted = false;
    if (nextFrame && !gFrameLag) { gCur++; gBall = 0; }
    if (nextFrame && gFrameLag) { Run(LOC_BALL_RETURNER, gFrameLag); gCur++; gBall = 0; }
    Run(LOC_BALL_RETURNER, 150);
    return at;
}

// A ball the game takes back (thrown off the lane onto the next one): nothing falls, nothing is scored.
static int Redo() {
    Run(LOC_START_POS, 120);
    int at = Layout();
    sLoc = LOC_THROWING; Tick();
    sRoThrew = true;
    RoPinsStart();
    Run(LOC_THROWING, 120);
    Run(LOC_BALL_RETURNER, 150);
    return at;
}

static void SceneTookThem() { sRoParts = 0; sRoThrew = true; }   // RampsTick: "a new scene took them"

static const unsigned ALL = 0x3FF;

int main() {
    printf("obstacle racks (live RoRackTick):\n");
    Run(LOC_START_POS, 60);
    Check(gBuilds == 1 && Layout() == 1, "a layout comes up for the first rack");

    // a whole game of strikes: 12 racks, a new layout for each
    {
        NewGame(true);
        int b0 = gBuilds, prev = -1, distinct = 0;
        for (int i = 0; i < 12; i++) { int l = Bowl(ALL); if (l != prev) distinct++; prev = l; }
        Check(gEnd, "12 strikes end the game");
        Check(distinct == 12, "a perfect game: 12 racks, 12 layouts");
        Check(gBuilds - b0 == 12, "...and nothing else was built");
        Check(gLastFrameBuilds >= 3, "the 10th frame's racks are built as the last frame (moving ramps allowed)");
    }

    // open frames: the second ball faces the first ball's layout
    {
        NewGame(true);
        bool same = true, differ = true;
        int prev2 = -1;
        for (int f = 0; f < 10; f++) {
            int a = Bowl(0x0F0), b = Bowl(0x00F);
            same = same && a == b;
            differ = differ && a != prev2;
            prev2 = b;
        }
        Check(gEnd, "ten open frames end the game");
        Check(same, "open frames: both balls of a frame are thrown at the same layout");
        Check(differ, "open frames: every frame gets a new one");
    }

    // the exploit: a ball the game takes back must not re-roll the layout
    {
        NewGame(true);
        int a = Bowl(0);        // (nothing down; the second ball follows)
        int r1 = Redo(), r2 = Redo(), r3 = Redo();
        int b = Bowl(ALL);      // spare
        Check(a == r1 && r1 == r2 && r2 == r3 && r3 == b, "redos (a ball off the lane) keep the layout, as does a first-ball gutter");
        int c = Bowl(ALL);
        Check(c != b, "...and the next frame still gets a new one");
        int r4 = Redo(), d = Bowl(0x001);
        Check(r4 == d && d != c, "a redo on a fresh rack (after the strike) keeps that rack's layout too");
    }

    // the 10th frame
    {
        NewGame(true);
        for (int f = 0; f < 9; f++) Bowl(ALL);
        int a = Bowl(0);        // gutter
        int b = Bowl(ALL);      // spare: a new rack
        int c = Bowl(0x001);
        Check(a == b, "10th frame: a gutter first ball keeps the layout for the second");
        Check(c != b, "10th frame: the rack after a spare gets a new layout");
        Check(gEnd, "...and the game ends after the third ball");
        NewGame(true);
        for (int f = 0; f < 9; f++) Bowl(ALL);
        int s1 = Bowl(ALL), s2 = Bowl(0x0FF), s3 = Bowl(0x300);
        Check(s1 != s2 && s2 == s3, "10th frame: strike then 8 then the spare: new layout after the strike, the same for the last ball");
        NewGame(true);
        for (int f = 0; f < 9; f++) Bowl(ALL);
        int o1 = Bowl(0x0FF), r = Redo(), o2 = Bowl(0x100);
        Check(o1 == r && r == o2 && gEnd, "10th frame open: one layout, a redo keeps it");
        NewGame(true);
        for (int f = 0; f < 9; f++) Bowl(ALL);
        gPinsetterLate = true;
        int g1 = Bowl(0), g2 = Bowl(0x00F);
        gPinsetterLate = false;
        Check(g1 == g2, "10th frame gutter: the pinsetter taking the pins up 3 s later isn't a knock (same layout)");
    }

    // the obstacles taken off the lane by something else come back the same for the same rack
    {
        NewGame(true);
        int a = Bowl(0x003);
        int restores = gRestores;
        SceneTookThem();
        int b = Bowl(0x00C);
        Check(a == b && gRestores == restores + 1, "taken away mid-frame (a new scene), pins down: the same layout comes back for the second ball");
        SceneTookThem();
        int c = Bowl(ALL);
        Check(c != b, "taken away after the frame's last ball: the next frame gets a new one");
        gBuildKind = 2;
        int d = Bowl(0x001);
        gBF.roStools = false;   // barstools switched off in the menu
        SceneTookThem();
        int e = Bowl(0x002);
        gBF.roStools = true;
        Check(d != e, "a kind switched off in the menu isn't put back");
    }

    // a restart (the Mode button) starts again with a new layout, even on the same game object at frame 1
    {
        NewGame(true);
        int a = Bowl(0x001);
        NewGame(false);
        int b = Bowl(0x001);
        Check(a != b, "restart at frame 1 of the same game object: a new layout");
    }

    // the game's frame number moving a moment after the new rack stands: one new layout, not two
    {
        NewGame(true);
        gFrameLag = 200;        // after the rack is set and the lane is calm
        int b0 = gBuilds;
        int a = Bowl(ALL), b = Bowl(0x001), c = Bowl(0x002);
        Check(a != b && b == c, "frame number late: the strike's new rack gets one layout for both balls");
        Check(gBuilds - b0 == 3, "...built once per rack (3 racks: frame 1, the strike's, the open frame's)");
        gFrameLag = 0;
    }

    // without the game's frame (fields not found): every full rack after a throw gets a new one, as up to 1.7.6
    {
        NewGame(true);
        gInfoWorks = false;
        int a = Bowl(0), b = Bowl(0x001);
        Check(a != b, "frame unknown: falls back to a new layout after each throw at a full rack");
        gInfoWorks = true;
    }

    // RoRackDecide on its own
    //                 parts  full   threw  info   same   reset  restore holding
    Check(RoRackDecide(true,  true,  false, true,  false, true,  true,  true) == RACK_TRACK, "decide: no throw since the build only tracks the game");
    Check(RoRackDecide(true,  true,  true,  true,  true,  false, true,  true) == RACK_KEEP, "decide: same rack, thrown at, next ball in hand: keep");
    Check(RoRackDecide(true,  true,  true,  true,  true,  false, true,  false) == RACK_WAIT, "decide: same rack but not holding the next ball yet: wait (the game may move on)");
    Check(RoRackDecide(true,  true,  true,  true,  true,  true,  true,  true) == RACK_NEW, "decide: same frame, a full rack set again (10th frame): new");
    Check(RoRackDecide(true,  false, true,  true,  false, false, true,  true) == RACK_WAIT, "decide: pins down with obstacles up: wait");
    Check(RoRackDecide(false, false, true,  true,  true,  false, true,  false) == RACK_RESTORE, "decide: gone mid-frame: put back");
    Check(RoRackDecide(false, false, true,  true,  false, false, true,  true) == RACK_WAIT, "decide: gone, another rack, pins down: wait for a full one");
    Check(RoRackDecide(false, true,  true,  true,  true,  false, false, true) == RACK_NEW, "decide: nothing to put back: new");

    printf("obstacle racks: %d/%d passed\n", gChecks - gBad, gChecks);
    return gBad ? 1 : 0;
}
