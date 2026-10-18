// Checks for the iOS menu button's spot (tools/tests/button), included after the live BFButtonSpot.
static int gChecks = 0, gBad = 0;
static void Check(bool ok, const char *what) {
    gChecks++;
    if (!ok) gBad++;
    printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
}

struct Phone { const char *name; double w, h, top, bottom, cutW, cutH, cutY; };   // points; the island / notch

int main() {
    printf("menu button, iOS (live BFButtonSpot):\n");
    const Phone phones[] = {
        { "iPhone 17 Pro (Dynamic Island)", 402, 874, 62, 34, 126, 37, 11 },
        { "iPhone 16 Pro Max (Dynamic Island)", 440, 956, 62, 34, 126, 37, 11 },
        { "iPhone 14 Pro (Dynamic Island)", 393, 852, 59, 34, 126, 37, 11 },
        { "iPhone 13 (notch)", 390, 844, 47, 34, 162, 33, 0 },
        { "iPhone X (notch)", 375, 812, 44, 34, 209, 30, 0 },
        { "iPhone SE (no notch)", 375, 667, 20, 0, 0, 0, 0 },
    };
    const double r = 24;
    char msg[240];
    for (const Phone &P : phones) {
        int inside = 0, hits = 0, moved = 0, n = 0, topOk = 0, topN = 0;
        double worstJump = 0;
        for (double py = -50; py <= P.h + 50; py += 3)
            for (double px = -50; px <= P.w + 50; px += 3) {
                double x, y, x2, y2;
                BFButtonSpot(px, py, r, P.w, P.h, P.top, P.bottom, &x, &y);
                n++;
                if (x >= r - 1e-9 && x <= P.w - r + 1e-9 && y >= r + 4 - 1e-9 && y <= P.h - P.bottom - r + 1e-9) inside++;
                // the island / notch as a rectangle: the button's circle must not touch it
                if (P.cutW > 0) {
                    double cx = fmax(P.w / 2 - P.cutW / 2, fmin(x, P.w / 2 + P.cutW / 2));
                    double cy = fmax(P.cutY, fmin(y, P.cutY + P.cutH));
                    if ((x - cx) * (x - cx) + (y - cy) * (y - cy) < r * r) hits++;
                }
                // a saved spot comes back to the same place (the position is saved as where it ended up)
                BFButtonSpot(x, y, r, P.w, P.h, P.top, P.bottom, &x2, &y2);
                if (fabs(x2 - x) > 1e-6 || fabs(y2 - y) > 1e-6) moved++;
                // the top edge: away from the island it goes all the way up
                if (py <= 0 && P.cutW > 0 && fabs(px - P.w / 2) > P.cutW / 2 + 6 + r && px > r && px < P.w - r) { topN++; if (y <= r + 4 + 1e-9) topOk++; }
                // a 3-point move of the finger never makes it jump further than the way round the island
                double xa, ya;
                BFButtonSpot(px + 3, py, r, P.w, P.h, P.top, P.bottom, &xa, &ya);
                double j = hypot(xa - x, ya - y);
                if (j > worstJump) worstJump = j;
            }
        snprintf(msg, sizeof(msg), "%s: on screen everywhere (%d/%d), never over the island/notch (%d), a saved spot stays put (%d moved)", P.name, inside, n, hits, moved);
        Check(inside == n && hits == 0 && moved == 0, msg);
        if (P.cutW > 0) {
            snprintf(msg, sizeof(msg), "%s: dragged to the top away from the island, it goes up to 4 pt from the top (%d/%d)", P.name, topOk, topN);
            Check(topN > 0 && topOk == topN, msg);
        }
        double x, y;
        BFButtonSpot(P.w / 2, 0, r, P.w, P.h, P.top, P.bottom, &x, &y);
        snprintf(msg, sizeof(msg), "%s: dragged to the top middle it sits under the island (y %.0f)", P.name, y);
        Check(fabs(x - P.w / 2) < 1e-9 && (P.cutW == 0 ? y == r + 4 : y >= P.top + r - 1e-9), msg);
        // the 1.7.6 first-build complaint: held about 86 pt down across most of the top edge
        int low = 0, cols = 0;
        for (double px = r; px <= P.w - r; px += 1) { BFButtonSpot(px, 0, r, P.w, P.h, P.top, P.bottom, &x, &y); cols++; if (y > r + 4 + 1e-9) low++; }
        // (an island is held to its own width plus the button's; a notch's width varies by model, so all of them get
        // the widest, the iPhone X's 209 points)
        double held = ((P.top >= 55 ? 126 : 210) + 2 * (6 + r) + 2) / (P.w - 2 * r);
        snprintf(msg, sizeof(msg), "%s: held below the top over %.0f%% of the top edge (at most %.0f%%: the cutout and the button); a 3 pt move jumps at most %.0f pt (going round it)", P.name, 100.0 * low / cols, 100.0 * fmin(held, 1.0), worstJump);
        Check(low <= cols * held && worstJump < 2 * (P.top + r), msg);
    }
    double mx, my;
    BFButtonSpot(201, 400, 24, 402, 874, 62, 34, &mx, &my);
    Check(mx == 201 && my == 400, "the middle of the screen is left alone");
    printf("menu button, iOS: %d/%d passed\n", gChecks - gBad, gChecks);
    return gBad ? 1 : 0;
}
