// Checks for tools/tests/stools (included after the live RoStoolBounce and RoStoolsTick).
#include <vector>
static int gChecks = 0, gBad = 0;
static void Check(bool ok, const char *what) {
    gChecks++;
    if (!ok) gBad++;
    printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
}
static float Seg(float ax, float ay, float bx, float by, float px, float py) { return sqrtf(SegDist2(ax, ay, bx, by, px, py)); }

static int gSteep = 0, gFar = 0;
struct Result { bool bounced, entered, through, back, faster, gutter, falseHit, second; int passed; };

// One throw at layout L from (x0, 0) towards (tx, ty) at speed v, hooking with a sideways acceleration hook, looked at
// fps times a second, as RoThrowTick runs it: every frame the ball moves, then the stool check runs on prev -> now.
static Result Throw(int layout, float x0, float tx, float ty, float speed, float hook, double fps) {
    Result r = {};
    sRoLayout = layout;
    const BPStoolLayout &L = kStoolLayouts[layout];
    float dx = tx - x0, dy = ty, dl = sqrtf(dx * dx + dy * dy);
    gBall.p = BFVec3{ x0, 0.3f, 0.108f };
    gBall.v = BFVec3{ speed * dx / dl, speed * dy / dl, 0 };
    gBall.w = BFVec3{ -30, 5, 10 };
    sRoPrev = gBall.p;
    sRoStoolHit = sRoDecided = false;
    sRoStoolMin = 9;
    int passed0 = sRoPassed, gutters0 = gGutters, hit = -1, bounces0 = sRoBounces;
    float hitY = 0;
    float before = speed;
    double dt = 1.0 / fps;
    for (int f = 0; f < 4000; f++) {
        gClock += dt;
        BFVec3 a = gBall.p;
        gBall.v.x += (sRoStoolHit ? 0.3f : 1.0f) * hook * (float)dt;   // (the knock takes 70% of the spin)
        gBall.p.x += gBall.v.x * (float)dt;
        gBall.p.y += gBall.v.y * (float)dt;
        BFVec3 b = gBall.p;
        if (!sRoStoolHit)                                 // the true path before any bounce
            for (int i = 0; i < L.n; i++) if (Seg(a.x, a.y, b.x, b.y, L.s[i].x, L.s[i].y) < kStoolReach - 0.01f) r.entered = true;
        float lx = b.x - sLaneX, ly = b.y - sFoulY;
        if (ly > sHeadY - sFoulY - 0.9f) break;            // at the pins
        bool was = sRoStoolHit;
        float sp0 = sqrtf(gBall.v.x * gBall.v.x + gBall.v.y * gBall.v.y);
        RoStoolsTick(b, lx, ly, b.z);                     // (RoThrowTick: then sRoPrev = the position it read)
        sRoPrev = b;
        if (sRoStoolHit && !was) {
            r.bounced = true;
            before = sp0;
            hitY = gBall.p.y;
            for (int i = 0; i < L.n; i++) {
                float d = sqrtf((gBall.p.x - L.s[i].x) * (gBall.p.x - L.s[i].x) + (gBall.p.y - L.s[i].y) * (gBall.p.y - L.s[i].y));
                if (d < kStoolReach + 0.02f) hit = i;
            }
        }
        if (sRoStoolHit && !sRoDecided) {
            float sp = sqrtf(gBall.v.x * gBall.v.x + gBall.v.y * gBall.v.y);
            if (!was && gBall.v.y < -0.5f * fabsf(gBall.v.x) - 1e-3f) { r.back = true; if (gSteep++ < 3) printf("    steep: %s, v (%.2f, %.2f), hook %.1f, %.0f Hz\n", L.name, gBall.v.x, gBall.v.y, hook, fps); }
            if (gBall.p.y < hitY - 1.0f) { r.back = true; if (gFar++ < 3) printf("    far back: %s, at (%.2f, %.2f) from %.2f, v (%.2f, %.2f), hook %.1f, %.0f Hz\n", L.name, gBall.p.x, gBall.p.y, hitY, gBall.v.x, gBall.v.y, hook, fps); }
            if (sp > before * 0.8f + 1e-3f && !was) r.faster = true;
            if (hit >= 0) {
                float d = sqrtf((gBall.p.x - L.s[hit].x) * (gBall.p.x - L.s[hit].x) + (gBall.p.y - L.s[hit].y) * (gBall.p.y - L.s[hit].y));
                if (d < kStoolReach - 0.02f) r.through = true;
            }
        }
        if (sRoDecided) break;
    }
    r.gutter = gGutters > gutters0;
    r.second = sRoBounces - bounces0 > 1;
    r.passed = sRoPassed - passed0;
    // never closer than the reach plus a little, yet bounced: a hit that shouldn't be
    if (r.bounced && !r.entered) {
        float m = 9;
        // (re-run the path without the stool to measure how close it would have come)
        gBall.p = BFVec3{ x0, 0.3f, 0.108f };
        gBall.v = BFVec3{ speed * dx / dl, speed * dy / dl, 0 };
        for (int f = 0; f < 4000 && gBall.p.y < 18; f++) {
            BFVec3 a = gBall.p;
            gBall.v.x += hook * (float)dt;
            gBall.p.x += gBall.v.x * (float)dt;
            gBall.p.y += gBall.v.y * (float)dt;
            for (int i = 0; i < L.n; i++) m = fminf(m, Seg(a.x, a.y, gBall.p.x, gBall.p.y, L.s[i].x, L.s[i].y));
            if (fabsf(gBall.p.x) > 0.58f) break;
        }
        r.falseHit = m > kStoolReach + 0.005f;
    }
    return r;
}

int main() {
    printf("barstools (live RoStoolBounce, RoStoolsTick), reach %.3f m:\n", kStoolReach);
    const double rates[] = { 30, 60, 120, 144 };
    const float speeds[] = { 4.0f, 7.0f, 10.5f };
    const float hooks[] = { 0.0f, 1.2f, -1.2f };
    long seconds = 0;
    long throws = 0, hits = 0, missed = 0, through = 0, back = 0, faster = 0, noGutter = 0, falseHits = 0, passed = 0;
    for (int l = 0; l < kStoolLayoutCount; l++)
        for (double fps : rates)
            for (float sp : speeds)
                for (float hook : hooks)
                    for (int i = 0; i < kStoolLayouts[l].n; i++)
                        for (float x0 = -0.45f; x0 <= 0.451f; x0 += 0.15f)
                            for (float off = -0.30f; off <= 0.301f; off += 0.02f) {
                                const BPStool &S = kStoolLayouts[l].s[i];
                                Result r = Throw(l, x0, S.x + off, S.y, sp, hook, fps);
                                throws++;
                                if (r.bounced) hits++;
                                if (r.entered && !r.bounced) { if (missed < 5) printf("    missed: %s, stool %d, from %.2f aimed %.2f off, %.1f m/s, hook %.1f, %.0f Hz\n", kStoolLayouts[l].name, i + 1, x0, off, sp, hook, fps); missed++; }
                                if (r.through) { if (through < 5) printf("    through: %s, stool %d, from %.2f aimed %.2f off, %.1f m/s, hook %.1f, %.0f Hz\n", kStoolLayouts[l].name, i + 1, x0, off, sp, hook, fps); through++; }
                                if (r.back) back++;
                                if (r.faster) faster++;
                                if (r.bounced && !r.gutter) { if (noGutter < 5) printf("    no gutter: %s, stool %d, from %.2f aimed %.2f off, %.1f m/s, hook %.1f, %.0f Hz\n", kStoolLayouts[l].name, i + 1, x0, off, sp, hook, fps); noGutter++; }
                                if (r.falseHit) falseHits++;
                                if (r.second) seconds++;
                                passed += r.passed;
                            }
    char msg[200];
    snprintf(msg, sizeof(msg), "%ld throws (14 layouts, 30-144 Hz, 4-10.5 m/s, straight and hooking both ways, 31 aims per stool): %ld hit a stool", throws, hits);
    Check(throws > 0 && hits > 0, msg);
    printf("  (%ld of the hits met a second stool and went straight into the gutter)\n", seconds);
    snprintf(msg, sizeof(msg), "every ball whose path comes within reach is bounced (%ld not)", missed);
    Check(missed == 0, msg);
    snprintf(msg, sizeof(msg), "after a bounce the ball never goes on into the stool (%ld did)", through);
    Check(through == 0, msg);
    snprintf(msg, sizeof(msg), "a bounced ball never heads back up the lane more steeply than 1 in 2 or more than 1 m (%ld did), never speeds up (%ld did)", back, faster);
    Check(back == 0 && faster == 0, msg);
    snprintf(msg, sizeof(msg), "every bounced ball ends in the gutter before the pins (%ld didn't)", noGutter);
    Check(noGutter == 0, msg);
    snprintf(msg, sizeof(msg), "no bounce for a ball that stays clear of the reach (%ld false hits)", falseHits);
    Check(falseHits == 0, msg);
    snprintf(msg, sizeof(msg), "the throw's own pass-through counter (Copy debug info: stool passes) stays 0 (%ld)", passed);
    Check(passed == 0, msg);
    printf("barstools: %d/%d passed\n", gChecks - gBad, gChecks);
    return gBad ? 1 : 0;
}
