package com.bowlingplus;

import android.app.Activity;
import android.graphics.Color;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;

import org.json.JSONObject;

// The "Custom oil" tab on the practice pattern screen (BFOilTab in OilUI.mm): a small button at the top of
// the screen, shown only while the game's practice oil-pattern picker (the "PracticeOil" window) is open.
// Tapping it opens the custom oil library. Its label shows the pattern you have on.
//
// Asking the game whether that screen is open has to happen on Unity's thread, and N.call blocks until
// Unity answers, so a background thread does the asking and only the button itself is touched on the UI thread.
final class OilTab {
    private static Button tab;
    private static boolean lobby;          // the last answer from the game
    private static Thread poller;

    private OilTab() {}

    static void start() {
        if (poller != null) return;
        poller = new Thread(() -> {
            for (;;) {
                try {
                    Thread.sleep(500);
                    if (BP.activity == null) continue;
                    String r = N.call("lobby");
                    if (r == null) continue;           // Unity didn't answer in time (loading, paused): keep what we know
                    final boolean now = "1".equals(r);
                    BP.UI.post(() -> { lobby = now; update(); });
                } catch (InterruptedException e) {
                    return;
                } catch (Throwable ignored) {}
            }
        }, "BowlingPlus-lobby");
        poller.setDaemon(true);
        poller.start();
    }

    static void update() {
        Activity a = BP.activity;
        FrameLayout root = UiKit.content(a);
        boolean show = lobby && root != null && !Oil.showing && !Menu.visible() && !Menu.pickerVisible();
        if (!show) { remove(); return; }
        if (tab != null && tab.getContext() != a) { remove(); tab = null; }   // a new activity: build again
        if (tab == null) {
            tab = UiKit.button(a, "", true, false);
            tab.setTextSize(14);
            tab.setBackground(UiKit.round(UiKit.ACCENT, 16, a));
            tab.setPadding(UiKit.dp(a, 16), UiKit.dp(a, 7), UiKit.dp(a, 16), UiKit.dp(a, 7));
            tab.setElevation(UiKit.dp(a, 6));
            tab.setOnClickListener(v -> { remove(); Oil.showLibrary(BP.activity); });
        }
        JSONObject p = Oil.patternById(Oil.activeId());
        tab.setText("\uD83D\uDEE2 " + (p != null ? p.optString("name", "Custom oil") : "Custom oil") + " \u25BE");
        if (tab.getParent() != root) {
            remove();
            FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            lp.gravity = Gravity.TOP | Gravity.CENTER_HORIZONTAL;
            lp.topMargin = topInset(root) + UiKit.dp(a, 4);
            root.addView(tab, lp);
        }
    }

    private static void remove() {
        if (tab != null && tab.getParent() instanceof ViewGroup) ((ViewGroup) tab.getParent()).removeView(tab);
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
