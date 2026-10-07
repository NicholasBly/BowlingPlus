package com.bowlingplus;

// Constants shared with the native BFShared.h.
final class BF {
    static final int ALL_PINS = 0x3FF;
    static final String VERSION = "1.6.9";
    static final float MAX_SPEED = 5.0f;
    static final float MAX_SPIN = 17.0f;

    private BF() {}

    // GitHub's latest-release tag vs ours: "1.4.3" / "v1.4.3". >0 means a > b.
    static int compareVersions(String a, String b) {
        String[] x = a.split("\\."), y = b.split("\\.");
        for (int i = 0; i < Math.max(x.length, y.length); i++) {
            int p = i < x.length ? parse(x[i]) : 0, q = i < y.length ? parse(y[i]) : 0;
            if (p != q) return p < q ? -1 : 1;
        }
        return 0;
    }

    private static int parse(String s) { try { return Integer.parseInt(s.replaceAll("[^0-9]", "")); } catch (Throwable t) { return 0; } }
}
