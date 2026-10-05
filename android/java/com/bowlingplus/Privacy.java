package com.bowlingplus;

import android.app.Activity;
import android.os.Handler;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebView;

import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

// The every-launch privacy page (Privacy.mm). The game's publisher SDK (MRGS) shows its agreement page in a
// WebView (MRGSWebViewActivity / MRGSGDPRDialog), loading the same HTML as iOS ("By clicking Sign up",
// clickButton()). BowlingPlus:
//   1. The first time, touches nothing: you accept it yourself; when it goes away, we remember (privacyOK).
//   2. From then on, the page is hidden (alpha 0) the moment it appears and its own Sign up button is pressed
//      for you (clickButton()). Any other page (updated terms) is shown and never pressed.
// We can't swizzle the SDK, so we poll the view tree for a WebView, like the iOS version polls for WKWebView.
public final class Privacy {
    static final int ST_WAIT = 0, ST_SIGNUP = 1, ST_OTHER = 2, ST_PRESSED = 3;
    static Handler timer = new Handler();
    static long startMs;
    static int accepted, hiddenCount;
    static boolean watching;
    static WebView watched;
    static final java.util.Map<WebView, Integer> state = new java.util.WeakHashMap<>();
    static final java.util.Map<WebView, Long> since = new java.util.WeakHashMap<>();
    static final java.util.Map<WebView, Long> lastProbe = new java.util.WeakHashMap<>();
    static final java.util.Map<WebView, View> hidden = new java.util.WeakHashMap<>();

    static final String PROBE_JS =
            "(function(){var t=document.body?document.body.innerText:'';"
            + "if(!t)return 'empty';"
            + "if(t.indexOf('By clicking Sign up')<0)return 'other';"
            + "if(typeof clickButton!=='function')return 'nofn';"
            + "return 'signup';})()";
    static final String ACCEPT_JS = "(function(){if(typeof clickButton!=='function')return 'nofn';clickButton();return 'ok';})()";

    private Privacy() {}

    static void start() {
        startMs = System.currentTimeMillis();
        timer.post(tick);
        pushStatus();
    }

    static final Runnable tick = new Runnable() {
        public void run() {
            try { doTick(); } catch (Throwable ignored) {}
            if (System.currentTimeMillis() - startMs < 300000 || watching) timer.postDelayed(this, 1000 / 30);
        }
    };

    static void doTick() {
        long now = System.currentTimeMillis();
        double age = (now - startMs) / 1000.0;
        // you accepted it yourself: it went away while we watched
        if (watching && (watched == null || watched.getWindowToken() == null || watched.getVisibility() != View.VISIBLE)) {
            watching = false;
            if (!Config.b("privacyOK", false)) {
                Config.set("privacyOK", true);
                N.call("logEvent", "bf\tprivacy page: you accepted it - BowlingPlus will press it for you from now on");
                pushStatus();
            }
        }
        if (gBFSafe()) return;
        List<WebView> webs = new ArrayList<>();
        Activity act = BP.activity;
        if (act == null) return;
        collect(act.getWindow().getDecorView(), webs);
        boolean anyVisible = false;
        for (final WebView w : webs) {
            if (w.getWindowToken() == null) continue;
            if (w.getVisibility() == View.VISIBLE && w.getAlpha() > 0.01f && w.getWidth() > 0) anyVisible = true;
            Integer stObj = state.get(w);
            int st = stObj == null ? ST_WAIT : stObj;
            if (stObj == null) { state.put(w, ST_WAIT); since.put(w, now); }
            boolean hide = Config.b("privacyOK", false) && age <= 300 && st != ST_OTHER;
            if (hide) {
                conceal(w);
                Long s = since.get(w);
                if (st == ST_WAIT && s != null && now - s > 8000) { reveal(w); state.put(w, ST_OTHER); continue; }
            }
            if (st != ST_WAIT && !(st == ST_SIGNUP && Config.b("privacyOK", false))) continue;
            Long lp = lastProbe.get(w);
            if (lp != null && now - lp < 250) continue;
            lastProbe.put(w, now);
            probe(w);
        }
        sPrivacyVisible = anyVisible;
    }

    static void collect(View v, List<WebView> out) {
        if (v instanceof WebView) out.add((WebView) v);
        if (v instanceof ViewGroup) { ViewGroup g = (ViewGroup) v; for (int i = 0; i < g.getChildCount(); i++) collect(g.getChildAt(i), out); }
    }

    static void probe(final WebView w) {
        w.evaluateJavascript(PROBE_JS, result -> {
            String r = unquote(result);
            if ("signup".equals(r)) {
                if (Config.b("privacyOK", false)) {
                    state.put(w, ST_PRESSED);
                    w.evaluateJavascript(ACCEPT_JS, r2 -> {
                        if ("ok".equals(unquote(r2))) { accepted++; hiddenCount++; N.call("logEvent", "bf\tprivacy page: pressed Sign up for you (out of sight)"); pushStatus(); }
                    });
                } else {
                    state.put(w, ST_SIGNUP);
                    watching = true;
                    watched = w;
                }
            } else if ("other".equals(r)) {
                state.put(w, ST_OTHER);
                reveal(w);
            }
        });
    }

    static void conceal(WebView w) {
        // hide the smallest ancestor that holds only the page (not the game's own view)
        View v = w;
        View root = BP.activity != null ? BP.activity.getWindow().getDecorView() : null;
        while (v.getParent() instanceof View && v.getParent() != root) v = (View) v.getParent();
        if (v != root && v.getAlpha() > 0.01f) { v.setAlpha(0f); hidden.put(w, v); }
        else if (w.getAlpha() > 0.01f) { w.setAlpha(0f); hidden.put(w, w); }
    }

    static void reveal(WebView w) {
        View v = hidden.remove(w);
        if (v != null) v.setAlpha(1f);
        if (w.getAlpha() < 0.01f) w.setAlpha(1f);
    }

    static boolean sGBFSafe = false;
    static boolean gBFSafe() { return false; }   // the native side gates its own actions; we just hide/press

    static volatile boolean sPrivacyVisible = false;

    static void pushStatus() {
        try {
            JSONObject j = new JSONObject();
            j.put("visible", sPrivacyVisible);
            j.put("accepted", accepted);
            j.put("debug", "privacy: remembered=" + (Config.b("privacyOK", false) ? 1 : 0) + " pressedForYou=" + accepted + " hiddenPages=" + hiddenCount + " watching=" + (watching ? 1 : 0));
            N.call("privacy", j.toString());
        } catch (Throwable ignored) {}
    }

    static String unquote(String s) {
        if (s == null) return "";
        if (s.length() >= 2 && s.startsWith("\"") && s.endsWith("\"")) return s.substring(1, s.length() - 1);
        return s;
    }
}
