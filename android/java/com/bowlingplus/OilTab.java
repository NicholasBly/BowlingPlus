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
// window) is open. Also the LOFT slider on the left edge while you hold the ball in Practice (BFLoftHud).
//
// Asking the game what's on screen has to happen on Unity's thread, and N.call blocks until Unity answers, so a
// background thread does the asking and only the views themselves are touched on the UI thread.
final class OilTab {
    private static Button tab, modeTab;
    private static LinearLayout loftBox;
    private static LoftSlider loftSlider;
    private static TextView loftValue;
    private static Button statsBtn;
    private static boolean lobby, holding;  // the last answers from the game
    private static boolean modeDialog;
    private static Thread poller;

    static final String[] MODE_NAMES = { "Normal", "Invisible oil", "9-pin no-tap", "Ramps & obstacles" };

    private OilTab() {}

    static void start() {
        if (poller != null) return;
        poller = new Thread(() -> {
            for (;;) {
                try {
                    Thread.sleep(250);
                    if (BP.activity == null) continue;
                    String r = N.call("hud");
                    if (r == null || r.length() < 2) continue;   // Unity didn't answer in time (loading, paused): keep what we know
                    final boolean l = r.charAt(0) == '1', h = r.charAt(1) == '1';
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
                return "Roll over the ramp or miss the barstools, or the shot scores 0." + (best > 0 ? " Best game: " + best + "." : "");
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
        if (!show || !Config.b("loftHud", true)) { removeLoft(); return; }
        if (loftBox != null && loftBox.getContext() != a) { removeLoft(); loftBox = null; }
        if (loftBox == null) {
            loftBox = UiKit.row(a, false);
            loftBox.setGravity(Gravity.CENTER_HORIZONTAL);
            loftBox.setBackground(UiKit.roundStroke(Color.argb(210, 20, 23, 28), 18, UiKit.dim(0.10f), 1, a));
            loftBox.setPadding(UiKit.dp(a, 2), UiKit.dp(a, 8), UiKit.dp(a, 2), UiKit.dp(a, 4));
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
        }
        syncStats();
        double ft = Math.max(0, Math.min(BF.MAX_LOFT, Config.d("loftFt", 0)));
        if (!loftSlider.touching && Math.abs(loftSlider.value - ft) > 0.01) { loftSlider.value = ft; loftSlider.invalidate(); }
        loftValue.setText(loftText(loftSlider.touching ? loftSlider.value : ft));
        if (loftBox.getParent() != root) {
            removeLoft();
            FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(UiKit.dp(a, 58), ViewGroup.LayoutParams.WRAP_CONTENT);
            lp.gravity = Gravity.START | Gravity.CENTER_VERTICAL;
            lp.leftMargin = UiKit.dp(a, 8);
            lp.bottomMargin = UiKit.dp(a, 60);
            root.addView(loftBox, lp);
        }
    }

    private static void syncStats() {
        if (statsBtn == null) return;
        boolean off = Config.b("hideBallStats", false);
        statsBtn.setText(off ? "STATS\nOFF" : "STATS\nON");
        statsBtn.setTextColor(off ? UiKit.dim(0.6f) : Color.rgb(20, 20, 20));
        statsBtn.setBackground(UiKit.round(off ? UiKit.dim(0.10f) : UiKit.YELLOW, 9, statsBtn.getContext()));
    }

    private static void removeLoft() {
        if (loftBox != null && loftBox.getParent() instanceof ViewGroup) ((ViewGroup) loftBox.getParent()).removeView(loftBox);
    }

    interface LoftListener { void changed(boolean done); }

    // a vertical slider: drag anywhere on it; double-tap for Off
    static final class LoftSlider extends View {
        double value;
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
                    getParent().requestDisallowInterceptTouchEvent(true);
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
