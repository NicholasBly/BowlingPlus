// Checks for tools/tests/ball (included after the live LoftVz and DampBounced).
#include <cstdlib>
static int gChecks = 0, gBad = 0;
static void Check(bool ok, const char *what) {
    gChecks++;
    if (!ok) gBad++;
    printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
}

static const float G = 9.81f;

// where a ball let go h0 above its rolling height with (vh, vz) comes down, and how high it goes
static float Lands(float vh, float vz, float h0) {
    float t = (vz + sqrtf(vz * vz + 2 * G * h0)) / G;
    return vh * t;
}
static float Top(float vz, float h0) { return h0 + (vz > 0 ? vz * vz / (2 * G) : 0); }

// A landing as the damper sees it: the ball comes down from (h0, vz0), bounces with restitution e, looked at every
// dt from a start phase. Returns the height above its rolling height at the first look where DampBounced fires, or
// -1 if it never does before the first bounce's top (the hop is then visible); hop: that top.
static float FirstCatch(float h0, float vz0, float e, double dt, double phase, bool &air, float &hop) {
    double t = phase, tEnd = 4.0;
    float prevVz = 0;
    air = false;
    // closed form: flight 1 until it lands, then a bounce of speed e * landing speed
    double tl = (vz0 + sqrt((double)vz0 * vz0 + 2 * G * h0)) / G;
    double vb = e * (G * tl - vz0);                   // upward speed after the bounce
    double tTop = tl + vb / G;
    hop = (float)(vb * vb / (2 * G));
    for (; t < tEnd; t += dt) {
        float h, vz;
        if (t < tl) { h = h0 + vz0 * t - 0.5 * G * t * t; vz = vz0 - G * t; }
        else {
            double u = t - tl;
            h = vb * u - 0.5 * G * u * u;
            vz = vb - G * u;
            if (h < 0) { h = 0; vz = 0; }
        }
        if (h > 0.03f) air = true;
        if (DampBounced(air, vz, prevVz, h)) return h;
        prevVz = vz;
        if (t > tTop) return -1;
    }
    return -1;
}

int main() {
    printf("ball (live LoftVz, DampBounced):\n");
    // loft: lands where asked unless the 1 m cap holds it, never above 1 m, never aimed down faster than 1 m/s
    int n = 0, off = 0, capped = 0, over = 0, steep = 0;
    float worst = 0;
    for (float ft = 1; ft <= BF_MAX_LOFT; ft += 0.5f)
        for (float vh = 4; vh <= 11; vh += 0.5f)
            for (float h0 = 0; h0 <= 0.40f; h0 += 0.02f) {
                bool cap = false;
                float vz = LoftVz(ft, vh, h0, cap);
                n++;
                if (vz < -1.0001f) steep++;
                if (Top(vz, h0) > 1.0005f) over++;
                if (cap) { capped++; continue; }
                if (vz <= -1.0f + 1e-4f) continue;           // aimed down as gently as allowed: lands sooner
                float d = ft * 0.3048f, err = fabsf(Lands(vh, vz, h0) - d);
                if (err > worst) worst = err;
                if (err > 0.01f) off++;
            }
    char msg[200];
    snprintf(msg, sizeof(msg), "loft: %d releases (1-%d ft, 4-11 m/s, let go 0-40 cm up) land within 1 cm of the setting (worst %.4f m; %d held by the 1 m cap)", n, BF_MAX_LOFT, worst, capped);
    Check(off == 0, msg);
    Check(over == 0, "loft: no flight goes higher than 1 m above the lane");
    Check(steep == 0, "loft: no ball is aimed down faster than 1 m/s");
    {
        bool cap = false;
        float vz = LoftVz(4, 8, 0.15f, cap), d = Lands(8, vz, 0.15f);
        snprintf(msg, sizeof(msg), "loft: 4 ft at 8 m/s let go 15 cm up lands at %.2f ft (the 1.7.5 formula gave about 7)", d / 0.3048f);
        Check(fabsf(d / 0.3048f - 4) < 0.05f, msg);
        cap = false;
        LoftVz(16, 4, 0, cap);
        Check(cap, "loft: 16 ft at 4 m/s is held by the cap (it would go 1.6 m up)");
    }

    // the damper: every landing is caught before it hops, at any frame rate and phase
    const double rates[] = { 30, 60, 90, 120, 144 };
    const float lofts[][2] = { { 0.30f, 0.0f }, { 0.15f, 1.2f }, { 0.05f, 2.0f }, { 0.0f, 2.5f }, { 0.20f, -0.8f } };
    const float es[] = { 0.15f, 0.3f, 0.5f };
    int landings = 0, missed = 0, small = 0;
    float highest = 0, smallTop = 0;
    for (double hz : rates)
        for (auto &l : lofts)
            for (float e : es)
                for (int k = 0; k < 16; k++) {
                    bool air;
                    float hop;
                    float h = FirstCatch(l[0], l[1], e, 1.0 / hz, k / 16.0 / hz, air, hop);
                    if (!air) continue;
                    landings++;
                    if (h >= 0) { if (h > highest) highest = h; continue; }
                    if (hop < 0.02f) { small++; if (hop > smallTop) smallTop = hop; }   // under 2 cm: not seen
                    else { missed++; printf("    missed: %.0f Hz, let go %.2f m up at %.1f m/s, bounce %.2f, phase %d/16, hop %.3f m\n", hz, l[0], l[1], e, k, hop); }
                }
    snprintf(msg, sizeof(msg), "damper: %d landings (30-144 Hz, 16 phases, bounce 0.15-0.5): every hop of 2 cm or more caught before its top (highest caught at %.3f m; %d smaller hops, up to %.3f m, let through)", landings, highest, small, smallTop);
    Check(missed == 0 && landings > 0, msg);

    // ...and leaves a rolling ball alone
    srand(7);
    int fired = 0;
    float prev = 0;
    for (int i = 0; i < 100000; i++) {
        float h = (rand() % 2001 - 1000) * 1e-6f;           // +-1 mm
        float vz = (rand() % 2001 - 1000) * 1e-4f;          // +-0.1 m/s
        if (DampBounced(false, vz, prev, h)) fired++;
        prev = vz;
    }
    Check(fired == 0, "damper: a ball rolling on the lane (never in the air) is never touched");
    int fired2 = 0;
    for (int i = 0; i < 1000; i++) if (DampBounced(true, 0.04f, -1.0f, 0.01f)) fired2++;
    Check(fired2 == 0, "damper: a ball barely moving up (under 5 cm/s) is left alone");
    Check(!DampBounced(true, 1.0f, -1.0f, 0.12f), "damper: above 10 cm it's a flight, not a bounce");

    printf("ball: %d/%d passed\n", gChecks - gBad, gChecks);
    return gBad ? 1 : 0;
}
