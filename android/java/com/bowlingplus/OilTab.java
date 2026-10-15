package com.bowlingplus;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.json.JSONObject;

// The tabs on the practice pattern screen (BFOilTab in OilUI.mm): "Custom oil" at the top of the screen and, under
// it, the game mode button (1.7.5), shown only while the game's practice oil-pattern picker (the "PracticeOil"
// window) is open. Also the LOFT slider on the right edge while you hold the ball in Practice (BFLoftHud), with the
// STATS and BACKUP buttons, tucked away at the edge until swiped in when "Swipe in from the right" is on.
//
// Asking the game what's on screen has to happen on Unity's thread, and N.call blocks until Unity answers, so a
// background thread does the asking and only the views themselves are touched on the UI thread.
final class OilTab {
    private static Button tab, modeTab;
    private static LinearLayout loftBox;
    private static LoftSlider loftSlider;
    private static TextView loftValue;
    private static Button statsBtn, backupBtn;
    private static TextView closeBtn;
    private static Grip grip;                // "Swipe in from the right": the strip at the edge while the panel is tucked away
    private static boolean loftOut;          // ...swiped in (until swiped away, or the ball is thrown)
    private static boolean lobby, holding;  // the last answers from the game
    private static boolean modeDialog;
    private static Thread poller;

    static final String[] MODE_NAMES = { "Normal", "Invisible oil", "9-pin no-tap", "Ramps & obstacles" };

    private OilTab() {}

    static void start() {
        if (poller != null) return;
        poller = new Thread(() -> {
            boolean busy = true;
            for (;;) {
                try {
                    Thread.sleep(busy ? 250 : 600);              // a slower look when nothing of ours can be on screen
                    if (BP.activity == null) continue;
                    String r = N.call("hud");
                    if (r == null || r.length() < 2) continue;   // Unity didn't answer in time (loading, paused): keep what we know
                    final boolean l = r.charAt(0) == '1', h = r.charAt(1) == '1';
                    busy = l || h || (r.length() > 2 && r.charAt(2) == '1');   // the pattern screen, the ball in hand, Practice
                    BP.UI.post(() -> { lobby = l; holding = h; update(); });
                } catch (InterruptedException e) {
                    return;
                } catch (Throwable ignored) {}
            }
        }, "BowlingPlus-lobby");
        poller.setDaemon(true);
        poller.start();
    }

    static int modeNow() {
        if (Config.b("rampsMode", false)) return 3;
        if (Config.b("noTap9", false)) return 2;
        if (Config.b("oilInvis", false)) return 1;
        return 0;
    }

    static void update() {
        Activity a = BP.activity;
        FrameLayout root = UiKit.content(a);
        boolean overlay = Oil.showing || Menu.visible() || Menu.pickerVisible() || modeDialog;
        updateLoft(a, root, holding && root != null && !overlay);
        boolean show = lobby && root != null && !overlay;
        if (!show) { remove(); return; }
        if (tab != null && tab.getContext() != a) { remove(); tab = null; modeTab = null; }   // a new activity: build again
        if (tab == null) {
            tab = UiKit.button(a, "", true, false);
            tab.setTextSize(14);
            tab.setBackground(UiKit.round(UiKit.ACCENT, 16, a));
            tab.setPadding(UiKit.dp(a, 16), UiKit.dp(a, 7), UiKit.dp(a, 16), UiKit.dp(a, 7));
            tab.setElevation(UiKit.dp(a, 6));
            tab.setOnClickListener(v -> { remove(); Oil.showLibrary(BP.activity); });
            modeTab = UiKit.button(a, "", false, false);
            modeTab.setTextSize(14);
            modeTab.setTextColor(Color.WHITE);
            modeTab.setBackground(UiKit.round(Color.argb(240, 26, 28, 36), 16, a));
            modeTab.setPadding(UiKit.dp(a, 16), UiKit.dp(a, 7), UiKit.dp(a, 16), UiKit.dp(a, 7));
            modeTab.setElevation(UiKit.dp(a, 6));
            modeTab.setOnClickListener(v -> { remove(); pickMode(BP.activity); });
        }
        JSONObject p = Oil.patternById(Oil.activeId());
        tab.setText("🛢 " + (p != null ? p.optString("name", "Custom oil") : "Custom oil") + " ▾");
        modeTab.setText("🎲 Mode: " + MODE_NAMES[modeNow()] + " ▾");
        if (tab.getParent() != root) {
            remove();
            FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            lp.gravity = Gravity.TOP | Gravity.CENTER_HORIZONTAL;
            lp.topMargin = topInset(root) + UiKit.dp(a, 4);
            root.addView(tab, lp);
            FrameLayout.LayoutParams lp2 = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            lp2.gravity = Gravity.TOP | Gravity.CENTER_HORIZONTAL;
            lp2.topMargin = topInset(root) + UiKit.dp(a, 4 + 46);
            root.addView(modeTab, lp2);
        }
    }

    private static void remove() {
        if (tab != null && tab.getParent() instanceof ViewGroup) ((ViewGroup) tab.getParent()).removeView(tab);
        if (modeTab != null && modeTab.getParent() instanceof ViewGroup) ((ViewGroup) modeTab.getParent()).removeView(modeTab);
    }

    // ---- the game mode picker: picking one restarts the game at frame 1 (BFGameModeSelect, on Unity's thread)
    static String modeHelp(int m) {
        switch (m) {
            case 1: return "The oil is hidden and each game plays a random unlocked pattern.";
            case 2: return "9 or more on a full rack counts as a strike.";
            case 3: {
                int best = Config.i("rampsBest", 0);
                return "Roll over the ramp, miss the barstools, get through the gates, or the shot scores 0. Pick which ones you get in the menu (Game modes)." + (best > 0 ? " Best game: " + best + "." : "");
            }
        }
        return "The game as it comes.";
    }

    static void pickMode(final Activity a) {
        if (a == null) return;
        Config.refresh();
        final int now = modeNow();
        String[] items = new String[MODE_NAMES.length];
        for (int i = 0; i < items.length; i++) items[i] = MODE_NAMES[i] + "\n" + modeHelp(i);
        modeDialog = true;
        new AlertDialog.Builder(a, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                .setTitle("Game mode (starts the game again at frame 1)")
                .setSingleChoiceItems(items, now, (dlg, which) -> {
                    dlg.dismiss();
                    final int m = which;
                    UiKit.toast(a, MODE_NAMES[m] + ": starting again at frame 1");
                    new Thread(() -> {
                        N.call("mode", String.valueOf(m));
                        Config.refresh();
                        BP.UI.post(OilTab::update);
                    }, "BowlingPlus-mode").start();
                })
                .setNegativeButton("Cancel", null)
                .setOnDismissListener(d -> { modeDialog = false; update(); })
                .show();
    }

    // ---- the LOFT slider (holding the ball, Practice): 0 (the game's own release) to BF.MAX_LOFT ft, in half feet
    private static String loftText(double ft) {
        if (ft < 0.25) return "Off";
        return ft == Math.floor(ft) ? String.format(java.util.Locale.US, "%.0f ft", ft) : String.format(java.util.Locale.US, "%.1f ft", ft);
    }

    private static void updateLoft(Activity a, FrameLayout root, boolean show) {
        if (!show || !Config.b("loftHud", true)) { removeLoft(); loftOut = false; return; }
        if (loftBox != null && loftBox.getContext() != a) { removeLoft(); loftBox = null; grip = null; }
        if (loftBox == null) {
            loftBox = new SwipeBox(a);
            loftBox.setOrientation(LinearLayout.VERTICAL);
            loftBox.setGravity(Gravity.CENTER_HORIZONTAL);
            loftBox.setBackground(UiKit.roundStroke(Color.argb(210, 20, 23, 28), 18, UiKit.dim(0.10f), 1, a));
            loftBox.setPadding(UiKit.dp(a, 2), UiKit.dp(a, 8), UiKit.dp(a, 2), UiKit.dp(a, 4));
            closeBtn = UiKit.label(a, "\u203A\u203A", 17, UiKit.dim(0.55f), true);   // "Swipe in from the right": put it away
            closeBtn.setGravity(Gravity.CENTER);
            closeBtn.setOnClickListener(v -> tuck());
            loftBox.addView(closeBtn, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, UiKit.dp(a, 22)));
            TextView title = UiKit.label(a, "LOFT", 11, UiKit.YELLOW, true);
            title.setGravity(Gravity.CENTER);
            loftValue = UiKit.label(a, "", 13, Color.WHITE, true);
            loftValue.setGravity(Gravity.CENTER);
            loftSlider = new LoftSlider(a);
            loftSlider.listener = done -> {
                Config.set("loftFt", loftSlider.value);
                loftValue.setText(loftText(loftSlider.value));
            };
            loftBox.addView(title, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            loftBox.addView(loftValue, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            loftBox.addView(loftSlider, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, UiKit.dp(a, 170)));
            statsBtn = UiKit.button(a, "", false, true);                // the ball stats on and off, right here
            statsBtn.setTextSize(9);
            statsBtn.setPadding(0, 0, 0, 0);
            statsBtn.setMinHeight(0); statsBtn.setMinimumHeight(0); statsBtn.setMinWidth(0); statsBtn.setMinimumWidth(0);
            statsBtn.setOnClickListener(v -> { Config.set("hideBallStats", !Config.b("hideBallStats", false)); syncStats(); });
            LinearLayout.LayoutParams sp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, UiKit.dp(a, 30));
            sp.leftMargin = sp.rightMargin = UiKit.dp(a, 4);
            loftBox.addView(statsBtn, sp);
            backupBtn = UiKit.button(a, "", false, true);              // backup ball: the spin mirrored at release
            backupBtn.setTextSize(9);
            backupBtn.setPadding(0, 0, 0, 0);
            backupBtn.setMinHeight(0); backupBtn.setMinimumHeight(0); backupBtn.setMinWidth(0); backupBtn.setMinimumWidth(0);
            backupBtn.setOnClickListener(v -> { Config.set("backupBall", !Config.b("backupBall", false)); syncStats(); });
            LinearLayout.LayoutParams bp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, UiKit.dp(a, 30));
            bp.leftMargin = bp.rightMargin = UiKit.dp(a, 4);
            bp.topMargin = UiKit.dp(a, 3);
            loftBox.addView(backupBtn, bp);
            grip = new Grip(a);
        }
        syncStats();
        double ft = Math.max(0, Math.min(BF.MAX_LOFT, Config.d("loftFt", 0)));
        if (!loftSlider.touching && Math.abs(loftSlider.value - ft) > 0.01) { loftSlider.value = ft; loftSlider.invalidate(); }
        loftValue.setText(loftText(loftSlider.touching ? loftSlider.value : ft));
        if (loftBox.getParent() != root) {
            removeLoft();
            FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(UiKit.dp(a, 58), ViewGroup.LayoutParams.WRAP_CONTENT);
            lp.gravity = Gravity.END | Gravity.CENTER_VERTICAL;      // the right edge: the left one has the game's blue tab
            lp.rightMargin = UiKit.dp(a, 8);
            lp.bottomMargin = UiKit.dp(a, 60);
            root.addView(loftBox, lp);
        }
        // hidden while a finger is down on the game (a throw's swipe) and for a moment after: after a throw the game has
        // left "holding the ball" by then and the panel stays away; after a tap it comes back
        boolean away = fingerDown || android.os.SystemClock.uptimeMillis() - fingerUpAt < 400;
        boolean tucked = Config.b("loftSwipe", false) && !loftOut;
        closeBtn.setVisibility(Config.b("loftSwipe", false) ? View.VISIBLE : View.GONE);
        loftBox.setVisibility(away || tucked ? View.INVISIBLE : View.VISIBLE);
        if (tucked && grip.getParent() != root) {
            if (grip.getParent() instanceof ViewGroup) ((ViewGroup) grip.getParent()).removeView(grip);
            FrameLayout.LayoutParams gl = new FrameLayout.LayoutParams(UiKit.dp(a, 24), UiKit.dp(a, 230));
            gl.gravity = Gravity.END | Gravity.CENTER_VERTICAL;
            gl.bottomMargin = UiKit.dp(a, 60);
            root.addView(grip, gl);
        } else if (!tucked && grip.getParent() instanceof ViewGroup) ((ViewGroup) grip.getParent()).removeView(grip);
        if (grip.getParent() != null) { grip.setVisibility(away ? View.INVISIBLE : View.VISIBLE); grip.bringToFront(); }
        loftBox.bringToFront();
    }

    private static void reveal() {
        if (loftOut || loftBox == null) return;
        loftOut = true;
        fingerDown = false; fingerUpAt = 0;
        update();
        loftBox.setTranslationX(UiKit.dp(loftBox.getContext(), 80));
        loftBox.animate().translationX(0).setDuration(180).start();
    }

    private static void tuck() {
        if (!Config.b("loftSwipe", false) || loftBox == null) return;
        loftBox.animate().translationX(UiKit.dp(loftBox.getContext(), 90)).setDuration(160).withEndAction(() -> {
            loftOut = false;
            loftBox.setTranslationX(0);
            update();
        }).start();
    }

    // the panel: in "Swipe in from the right", a drag to the right over it (starting sideways, so the slider's up-and-down
    // drags stay the slider's) moves it with the finger and puts it away; a slider move it interrupted is undone
    static final class SwipeBox extends LinearLayout {
        private float sx, sy;
        private long downAt;
        private boolean dragging;
        SwipeBox(Context c) { super(c); }
        private boolean sideways(MotionEvent e) {
            float dx = e.getRawX() - sx, dy = e.getRawY() - sy;   // screen coordinates: the panel moves under the finger
            return Config.b("loftSwipe", false) && dx > UiKit.dp(getContext(), 18) && dx > 1.3f * Math.abs(dy);
        }
        private void startDrag() {
            dragging = true;
            if (loftSlider != null && loftSlider.downAt >= downAt && Math.abs(loftSlider.value - loftSlider.startValue) > 0.01) {
                loftSlider.value = loftSlider.startValue;
                loftSlider.invalidate();
                Config.set("loftFt", loftSlider.value);
                if (loftValue != null) loftValue.setText(loftText(loftSlider.value));
            }
        }
        private void down(MotionEvent e) { sx = e.getRawX(); sy = e.getRawY(); downAt = e.getEventTime(); dragging = false; }
        @Override public boolean onInterceptTouchEvent(MotionEvent e) {
            if (e.getActionMasked() == MotionEvent.ACTION_DOWN) { down(e); return false; }
            if (e.getActionMasked() == MotionEvent.ACTION_MOVE && !dragging && sideways(e)) { startDrag(); return true; }
            return dragging;
        }
        @Override public boolean onTouchEvent(MotionEvent e) {   // the panel itself, or a drag taken from a control
            float dx = e.getRawX() - sx;
            switch (e.getActionMasked()) {
                case MotionEvent.ACTION_DOWN: down(e); return true;
                case MotionEvent.ACTION_MOVE:
                    if (!dragging && sideways(e)) startDrag();
                    if (dragging) setTranslationX(Math.max(0, dx));
                    return true;
                case MotionEvent.ACTION_UP: case MotionEvent.ACTION_CANCEL:
                    if (dragging) {
                        dragging = false;
                        if (getTranslationX() > UiKit.dp(getContext(), 36)) tuck();
                        else animate().translationX(0).setDuration(150).start();
                    }
                    return true;
            }
            return true;
        }
    }

    // the grip at the right edge: swipe in from it, or tap it
    static final class Grip extends View {
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF r = new RectF();
        private float sx;
        Grip(Context c) { super(c); }
        @Override protected void onDraw(Canvas cv) {
            float w = UiKit.dp(getContext(), 5), h = UiKit.dp(getContext(), 46), x = getWidth() - UiKit.dp(getContext(), 4);
            paint.setColor(Color.argb(115, 255, 255, 255));
            r.set(x - w, (getHeight() - h) / 2f, x, (getHeight() + h) / 2f);
            cv.drawRoundRect(r, w / 2, w / 2, paint);
        }
        @Override public boolean onTouchEvent(MotionEvent e) {
            switch (e.getActionMasked()) {
                case MotionEvent.ACTION_DOWN: sx = e.getX(); return true;
                case MotionEvent.ACTION_MOVE: if (sx - e.getX() > UiKit.dp(getContext(), 20)) reveal(); return true;
                case MotionEvent.ACTION_UP: if (Math.abs(sx - e.getX()) < UiKit.dp(getContext(), 10)) reveal(); return true;
            }
            return true;
        }
    }

    // From BP.TapSpy, which sees every touch before the game does (UI thread). The game hides its own buttons the moment
    // a throw's swipe starts; the LOFT panel goes with them, at once.
    private static boolean fingerDown;
    private static long fingerUpAt;

    static void touchDown(float x, float y) {
        boolean boxUp = loftBox != null && loftBox.getParent() != null && loftBox.getVisibility() == View.VISIBLE;
        boolean gripUp = grip != null && grip.getParent() != null && grip.getVisibility() == View.VISIBLE;
        if (!boxUp && !gripUp) return;
        if (boxUp && inside(loftBox, x, y)) return;   // on the panel
        if (gripUp && inside(grip, x, y)) return;     // on the grip
        fingerDown = true;
        if (loftBox != null) loftBox.setVisibility(View.INVISIBLE);
        if (grip != null) grip.setVisibility(View.INVISIBLE);
    }

    private static boolean inside(View v, float x, float y) {
        int[] at = new int[2];
        v.getLocationInWindow(at);
        return x >= at[0] && x < at[0] + v.getWidth() && y >= at[1] && y < at[1] + v.getHeight();
    }

    static void touchUp() {
        if (!fingerDown) return;
        fingerDown = false;
        fingerUpAt = android.os.SystemClock.uptimeMillis();
    }

    private static void syncStats() {
        if (statsBtn == null) return;
        boolean off = Config.b("hideBallStats", false);
        statsBtn.setText(off ? "STATS\nOFF" : "STATS\nON");
        statsBtn.setTextColor(off ? UiKit.dim(0.6f) : Color.rgb(20, 20, 20));
        statsBtn.setBackground(UiKit.round(off ? UiKit.dim(0.10f) : UiKit.YELLOW, 9, statsBtn.getContext()));
        if (backupBtn == null) return;
        boolean bk = Config.b("backupBall", false);
        backupBtn.setText(bk ? "BACKUP\nON" : "BACKUP\nOFF");
        backupBtn.setTextColor(bk ? Color.rgb(20, 20, 20) : UiKit.dim(0.6f));
        backupBtn.setBackground(UiKit.round(bk ? UiKit.YELLOW : UiKit.dim(0.10f), 9, backupBtn.getContext()));
    }

    private static void removeLoft() {
        if (grip != null && grip.getParent() instanceof ViewGroup) ((ViewGroup) grip.getParent()).removeView(grip);
        if (loftBox != null && loftBox.getParent() instanceof ViewGroup) ((ViewGroup) loftBox.getParent()).removeView(loftBox);
    }

    interface LoftListener { void changed(boolean done); }

    // a vertical slider: drag anywhere on it; double-tap for Off
    static final class LoftSlider extends View {
        double value, startValue;           // startValue: when the last touch began (a swipe that puts the panel away undoes it)
        long downAt;
        boolean touching;
        LoftListener listener;
        private long lastTap;
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final RectF r = new RectF();

        LoftSlider(Context c) { super(c); }

        private float top() { return UiKit.dp(getContext(), 14); }
        private float bottom() { return getHeight() - UiKit.dp(getContext(), 14); }

        @Override protected void onDraw(Canvas cv) {
            float cx = getWidth() / 2f, w = UiKit.dp(getContext(), 5), t = top(), b = bottom();
            float y = b - (float) (value / BF.MAX_LOFT) * (b - t);
            paint.setColor(UiKit.dim(0.18f));
            r.set(cx - w, t, cx + w, b);
            cv.drawRoundRect(r, w, w, paint);
            paint.setColor(UiKit.ACCENT);
            r.set(cx - w, y, cx + w, b);
            cv.drawRoundRect(r, w, w, paint);
            paint.setColor(UiKit.dim(0.35f));
            for (int ft = 4; ft < BF.MAX_LOFT; ft += 4) {
                float ty = b - (float) ft / BF.MAX_LOFT * (b - t);
                cv.drawRect(cx - w - UiKit.dp(getContext(), 5), ty - 1, cx - w - UiKit.dp(getContext(), 1), ty + 1, paint);
            }
            paint.setColor(UiKit.YELLOW);
            cv.drawCircle(cx, y, UiKit.dp(getContext(), 12), paint);
        }

        @Override public boolean onTouchEvent(MotionEvent e) {
            switch (e.getActionMasked()) {
                case MotionEvent.ACTION_DOWN: {
                    long now = e.getEventTime();
                    if (now - lastTap < 300) { value = 0; touching = false; invalidate(); if (listener != null) listener.changed(true); lastTap = 0; return true; }
                    lastTap = now;
                    touching = true;
                    startValue = value;
                    downAt = now;                // (no requestDisallowIntercept: the panel may take a sideways drag)
                    track(e.getY(), false);
                    return true;
                }
                case MotionEvent.ACTION_MOVE: track(e.getY(), false); return true;
                case MotionEvent.ACTION_UP: touching = false; track(e.getY(), true); return true;
                case MotionEvent.ACTION_CANCEL: touching = false; if (listener != null) listener.changed(true); return true;
            }
            return super.onTouchEvent(e);
        }

        private void track(float y, boolean done) {
            float t = top(), b = bottom();
            double frac = Math.max(0, Math.min(1, (b - y) / Math.max(1f, b - t)));
            double v = Math.round(frac * BF.MAX_LOFT * 2) / 2.0;
            if (v != value || done) {
                value = v;
                invalidate();
                if (listener != null) listener.changed(done);
            }
        }
    }

    // keep clear of the status bar and the camera cut-out (the game is portrait, so they're at the top)
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
}
