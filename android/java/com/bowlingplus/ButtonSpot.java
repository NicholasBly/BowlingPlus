package com.bowlingplus;

// Where the on-screen menu button may sit (MenuButton), as plain arithmetic so tools/tests can check it on a computer.
final class ButtonSpot {
    private ButtonSpot() {}

    // The nearest spot to (x, y), the top-left corner of a size x size button, that keeps it inside a w x h view, bottomGap
    // above its bottom, below barTop (a visible status bar; 0 for none), and clear by gap of each camera cutout at the
    // top (cuts: left, top, right, bottom for each, in the view's coordinates): under it or beside it, whichever is
    // nearer, so a dragged button slides round it.
    static int[] clamp(int x, int y, int size, int w, int h, int gap, int bottomGap, int barTop, int[] cuts) {
        int bottom = Math.max(0, h - bottomGap - size);
        x = clamp(x, 0, w - size);
        y = clamp(y, 0, bottom);
        for (int i = 0; cuts != null && i + 3 < cuts.length; i += 4) {
            int l = cuts[i], t = cuts[i + 1], r = cuts[i + 2], b = cuts[i + 3];
            if (b + gap <= 0 || t > h / 3) continue;                 // a cutout at the top only
            if (x >= r + gap || x + size <= l - gap || y >= b + gap) continue;
            int under = Math.min(b + gap, bottom);
            int side = x + size / 2 < (l + r) / 2 ? l - gap - size : r + gap;
            if (side >= 0 && side <= w - size && Math.abs(side - x) < under - y) x = side;
            else y = under;
        }
        if (barTop > 0) y = clamp(y, Math.min(barTop, bottom), bottom);
        return new int[] { x, y };
    }

    static int clamp(int v, int lo, int hi) { return v < lo ? lo : Math.min(v, Math.max(lo, hi)); }
}
