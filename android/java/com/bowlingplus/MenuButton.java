package com.bowlingplus;

import android.app.Activity;
import android.graphics.Color;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

// An optional on-screen alternative to shaking the phone: a small round button, shown at the top of the
// screen by default, that opens the menu on a tap. Press and drag it anywhere on screen; its spot is
// remembered. Shake and the three-finger tap (BP.java) keep working regardless - this is an extra way in,
// not a replacement, since it's one more thing that could end up dragged off-screen or otherwise stuck.
final class MenuButton {
    private MenuButton() {}

    private static TextView btn;
    private static Activity builtFor;
    private static float downRawX, downRawY, startX, startY;
    private static boolean dragging;

    // position as a fraction of the screen (0..1), independent of rotation/screen size; (-1,-1) = not set yet
    static float posX = -1, posY = -1;

    static void start() { refresh(); }

    // Called whenever it might need to appear/disappear/move: after the config toggle, after the activity
    // changes, when the menu opens or closes, on rotation (BP's touch/lifecycle hooks call this; it's cheap).
    static void refresh() {
        Activity act = BP.activity;
        // out of the way of every panel (menu, pin picker, oil library, pattern editor, color picker)
        boolean want = Config.b("menuButton", false) && act != null && !Menu.visible() && !Menu.pickerVisible() && !UiKit.overlayOpen();
        FrameLayout root = UiKit.content(act);
        if (!want || root == null) { remove(); return; }
        if (btn != null && builtFor != act) remove();   // a different activity now: rebuild in it
        if (btn == null) build(act, root);
        if (btn.getParent() != root) { remove(); attach(act, root); }
        btn.bringToFront();
    }

    private static void build(Activity act, FrameLayout root) {
        loadPosition(act);
        TextView b = new TextView(act);
        b.setText("\uD83C\uDFB3");
        b.setTextSize(20);
        b.setGravity(Gravity.CENTER);
        int size = UiKit.dp(act, 48);
        b.setLayoutParams(new FrameLayout.LayoutParams(size, size));
        b.setBackground(UiKit.round(Color.argb(210, 30, 30, 34), 24, act));
        b.setElevation(UiKit.dp(act, 6));
        b.setOnTouchListener(MenuButton::onTouch);
        btn = b;
        builtFor = act;
    }

    private static void attach(Activity act, FrameLayout root) {
        root.addView(btn);
        place(act, root);
    }

    private static void remove() {
        if (btn != null && btn.getParent() instanceof ViewGroup) ((ViewGroup) btn.getParent()).removeView(btn);
    }

    // posX/posY (0..1) -> actual pixels, clamped so the whole button stays on screen and clear of the status
    // bar / camera cutout (clampPos).
    private static void place(Activity act, FrameLayout root) {
        int size = UiKit.dp(act, 48);
        int w = root.getWidth(), h = root.getHeight();
        if (w <= 0 || h <= 0) { root.post(() -> place(act, root)); return; }   // not laid out yet
        int[] at = clampPos(root, Math.round(posX * w) - size / 2, Math.round(posY * h) - size / 2, size);
        FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams) btn.getLayoutParams();
        lp.gravity = Gravity.TOP | Gravity.START;
        lp.leftMargin = at[0];
        lp.topMargin = at[1];
        btn.setLayoutParams(lp);
    }

    private static boolean onTouch(View v, MotionEvent ev) {
        FrameLayout root = (FrameLayout) v.getParent();
        if (root == null) return false;
        switch (ev.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                downRawX = ev.getRawX(); downRawY = ev.getRawY();
                startX = ((FrameLayout.LayoutParams) v.getLayoutParams()).leftMargin;
                startY = ((FrameLayout.LayoutParams) v.getLayoutParams()).topMargin;
                dragging = false;
                return true;
            case MotionEvent.ACTION_MOVE: {
                float dx = ev.getRawX() - downRawX, dy = ev.getRawY() - downRawY;
                float slop = 10 * v.getResources().getDisplayMetrics().density;
                if (!dragging && Math.hypot(dx, dy) < slop) return true;   // still a tap, not yet a drag
                dragging = true;
                int[] at = clampPos(root, Math.round(startX + dx), Math.round(startY + dy), v.getWidth());
                FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams) v.getLayoutParams();
                lp.leftMargin = at[0]; lp.topMargin = at[1];
                v.setLayoutParams(lp);
                return true;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                if (dragging) {
                    FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams) v.getLayoutParams();
                    int size = v.getWidth();
                    posX = (lp.leftMargin + size / 2f) / root.getWidth();
                    posY = (lp.topMargin + size / 2f) / root.getHeight();
                    savePosition(builtFor);
                } else if (ev.getActionMasked() == MotionEvent.ACTION_UP) {
                    Menu.toggle(builtFor);
                }
                dragging = false;
                return true;
        }
        return false;
    }

    // The nearest spot to (x, y) (the button's top-left corner, in the content view) that keeps the button on screen,
    // 24 dp above the bottom, below a visible status bar, and clear of the camera cutout (ButtonSpot). The cutout's
    // rectangles and the status bar are in window coordinates, so they're moved into the content view's (the first
    // 1.7.6 build compared them as they were, which holds the button too low if the content starts below the top of
    // the window).
    private static int[] clampPos(View root, int x, int y, int size) {
        int w = root.getWidth(), h = root.getHeight();
        int gap = UiKit.dp(root.getContext(), 4), bottomGap = UiKit.dp(root.getContext(), 24);
        int barTop = 0;
        int[] cuts = null;
        try {
            android.view.WindowInsets ins = root.getRootWindowInsets();
            if (ins != null) {
                int[] loc = new int[2];
                root.getLocationInWindow(loc);
                int bar = ins.getSystemWindowInsetTop(), cut = 0;
                if (android.os.Build.VERSION.SDK_INT >= 28 && ins.getDisplayCutout() != null) {
                    cut = ins.getDisplayCutout().getSafeInsetTop();
                    java.util.List<android.graphics.Rect> rects = ins.getDisplayCutout().getBoundingRects();
                    cuts = new int[rects.size() * 4];
                    for (int i = 0; i < rects.size(); i++) {
                        android.graphics.Rect r = rects.get(i);
                        cuts[i * 4] = r.left - loc[0]; cuts[i * 4 + 1] = r.top - loc[1];
                        cuts[i * 4 + 2] = r.right - loc[0]; cuts[i * 4 + 3] = r.bottom - loc[1];
                    }
                }
                if (bar > cut + 2) barTop = Math.max(0, bar - loc[1]);   // the status bar is showing
            }
        } catch (Throwable ignored) {}
        return ButtonSpot.clamp(x, y, size, w, h, gap, bottomGap, barTop, cuts);
    }

    // ---- remembered position: fraction of screen width/height, so it survives rotation and different devices
    private static void loadPosition(Activity act) {
        if (posX >= 0) return;   // already loaded this run
        android.content.SharedPreferences p = act.getSharedPreferences("BowlingPlusUI", android.content.Context.MODE_PRIVATE);
        posX = p.getFloat("btnX", 0.5f);
        posY = p.getFloat("btnY", 0.10f);   // top-center by default
    }

    private static void savePosition(Activity act) {
        if (act == null) return;
        act.getSharedPreferences("BowlingPlusUI", android.content.Context.MODE_PRIVATE)
                .edit().putFloat("btnX", posX).putFloat("btnY", posY).apply();
    }
}
