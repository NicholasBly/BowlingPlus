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
    // bar / camera cutout, the same inset OilTab uses.
    private static void place(Activity act, FrameLayout root) {
        int size = UiKit.dp(act, 48);
        int w = root.getWidth(), h = root.getHeight();
        if (w <= 0 || h <= 0) { root.post(() -> place(act, root)); return; }   // not laid out yet
        int top = topInset(root), bottom = UiKit.dp(act, 24);
        int x = clamp(Math.round(posX * w) - size / 2, 0, w - size);
        int y = clamp(Math.round(posY * h) - size / 2, top, h - bottom - size);
        FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams) btn.getLayoutParams();
        lp.gravity = Gravity.TOP | Gravity.START;
        lp.leftMargin = x;
        lp.topMargin = y;
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
                int size = v.getWidth();
                int x = clamp(Math.round(startX + dx), 0, root.getWidth() - size);
                int y = clamp(Math.round(startY + dy), topInset(root), root.getHeight() - UiKit.dp(builtFor, 24) - size);
                FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams) v.getLayoutParams();
                lp.leftMargin = x; lp.topMargin = y;
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

    private static int clamp(int v, int lo, int hi) { return v < lo ? lo : Math.min(v, Math.max(lo, hi)); }

    private static int topInset(View v) {
        int top = 0;
        try {
            android.view.WindowInsets ins = v.getRootWindowInsets();
            if (ins != null) {
                top = ins.getSystemWindowInsetTop();
                if (android.os.Build.VERSION.SDK_INT >= 28 && ins.getDisplayCutout() != null)
                    top = Math.max(top, ins.getDisplayCutout().getSafeInsetTop());
            }
        } catch (Throwable ignored) {}
        return top;
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
