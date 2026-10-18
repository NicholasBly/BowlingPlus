package com.bowlingplus;

// Checks for the Android menu button's spot (tools/tests/button): compiled with the live ButtonSpot.java.
public final class ButtonSpotTest {
    static int checks = 0, bad = 0;
    static void check(boolean ok, String what) {
        checks++;
        if (!ok) bad++;
        System.out.println("  " + (ok ? "ok   " : "FAIL ") + what);
    }

    // a phone in pixels: the view, the button, a punch-hole or notch cutout (left, top, right, bottom), a status bar
    static void phone(String name, int w, int h, float density, int[] cut, int bar) {
        int size = Math.round(48 * density), gap = Math.round(4 * density), bottomGap = Math.round(24 * density);
        int n = 0, inside = 0, hits = 0, moved = 0, topN = 0, topOk = 0;
        int step = Math.max(1, w / 120);
        for (int py = -size; py <= h + size; py += step)
            for (int px = -size; px <= w + size; px += step) {
                int[] a = ButtonSpot.clamp(px, py, size, w, h, gap, bottomGap, bar, cut);
                n++;
                if (a[0] >= 0 && a[0] <= w - size && a[1] >= Math.max(0, bar) && a[1] <= h - bottomGap - size) inside++;
                for (int i = 0; cut != null && i < cut.length; i += 4)
                    if (a[0] < cut[i + 2] + gap && a[0] + size > cut[i] - gap && a[1] < cut[i + 3] + gap && a[1] + size > cut[i + 1] - gap) hits++;
                int[] b = ButtonSpot.clamp(a[0], a[1], size, w, h, gap, bottomGap, bar, cut);
                if (b[0] != a[0] || b[1] != a[1]) moved++;
                if (py <= 0 && px >= 0 && px <= w - size && cut != null && (px + size < cut[0] - gap || px > cut[2] + gap)) { topN++; if (a[1] == Math.max(0, bar)) topOk++; }
            }
        check(inside == n && hits == 0 && moved == 0, name + ": on screen everywhere (" + inside + "/" + n + "), never over the cutout (" + hits + "), a saved spot stays put (" + moved + " moved)");
        if (cut != null) check(topN > 0 && topOk == topN, name + ": dragged to the top beside the cutout, it goes all the way up (" + topOk + "/" + topN + ")");
    }

    public static void main(String[] args) {
        System.out.println("menu button, Android (live ButtonSpot):");
        phone("1440x3200 punch hole, no status bar", 1440, 3200, 3.5f, new int[] { 680, 40, 760, 120 }, 0);
        phone("1080x2400 punch hole, status bar showing", 1080, 2400, 2.625f, new int[] { 500, 30, 580, 110 }, 110);
        phone("1080x2340 notch, no status bar", 1080, 2340, 2.75f, new int[] { 340, 0, 740, 90 }, 0);
        phone("1260x2800 corner hole", 1260, 2800, 3.0f, new int[] { 60, 40, 140, 120 }, 0);
        phone("1080x1920 no cutout", 1080, 1920, 2.625f, null, 0);
        phone("content below the cutout (it's above the view)", 1080, 2300, 2.625f, new int[] { 500, -90, 580, -10 }, 0);
        int[] mid = ButtonSpot.clamp(500, 1000, 126, 1080, 2400, 11, 63, 0, new int[] { 500, 30, 580, 110 });
        check(mid[0] == 500 && mid[1] == 1000, "the middle of the screen is left alone");
        System.out.println("menu button, Android: " + (checks - bad) + "/" + checks + " passed");
        System.exit(bad == 0 ? 0 : 1);
    }
}
