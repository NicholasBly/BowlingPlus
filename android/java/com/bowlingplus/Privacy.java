package com.bowlingplus;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebView;

import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

// The every-launch privacy page (Privacy.mm). The game's publisher SDK (MRGS) shows its agreement page in a
// WebView (MRGSWebViewActivity / MRGSGDPRDialog), loading the same HTML as iOS ("By clicking Sign up",
// clickButton()). BowlingPlus:
//   1. The first time, touches nothing: you accept it yourself; when it goes away, we remember (privacyOK).
//   2. From then on, the page is hidden (alpha 0) the moment it appears and its own Sign up button is pressed
//      for you (clickButton()). Any other page (updated terms) is shown and never pressed.
// We can't swizzle the SDK, so we poll the view tree for a WebView, like the iOS version polls for WKWebView.
public final class Privacy {
    static final int ST_WAIT = 0, ST_SIGNUP = 1, ST_OTHER = 2, ST_PRESSED = 3;
    static final Handler timer = new Handler(Looper.getMainLooper());
    static long startMs;
    static int accepted, hiddenCount;
    static boolean watching, sawSignUp;
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
    // First time only: notice that YOU pressed Sign up. The page's button calls clickButton(), which submits
    // its form; the wrapper marks the page title first (Java reads it every tick) and submits 300 ms later, so
    // the mark is seen before the page goes away. Closing the page any other way is not taken as agreement.
    static final String MARK_JS = "(function(){if(window.__bpMark)return 'ok';if(typeof clickButton!=='function')return 'nofn';"
            + "var o=clickButton;window.__bpMark=1;window.clickButton=function(){try{document.title='bp:signed-up';}catch(e){}setTimeout(o,300);};return 'ok';})()";

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
        if (watching && watched != null) {
            String t = null;
            try { t = watched.getTitle(); } catch (Throwable ignored) {}
            if (t != null && t.startsWith("bp:signed-up")) sawSignUp = true;
        }
        // it went away while we watched: remember it only if you pressed Sign up
        if (watching && (watched == null || watched.getWindowToken() == null || watched.getVisibility() != View.VISIBLE)) {
            watching = false;
            if (sawSignUp && !Config.b("privacyOK", false)) {
                Config.set("privacyOK", true);
                N.call("logEvent", "bf\tprivacy page: you accepted it - BowlingPlus will press it for you from now on");
                pushStatus();
            } else if (!sawSignUp) {
                N.call("logEvent", "bf\tprivacy page: closed without Sign up - not remembered, it will show again");
            }
            sawSignUp = false;
        }
        if (gBFSafe()) return;
        List<WebView> webs = new ArrayList<>();
        for (View root : roots()) collect(root, webs);
        boolean anyVisible = false;
        for (final WebView w : webs) {
            if (w.getWindowToken() == null) continue;
            if (w.getVisibility() == View.VISIBLE && w.getAlpha() > 0.01f && w.getWidth() > 0) anyVisible = true;
            Integer stObj = state.get(w);
            int st = stObj == null ? ST_WAIT : stObj;
            if (stObj == null) { state.put(w, ST_WAIT); since.put(w, now); }
            // Hide right away only when it's MRGS's page; any other web view (an ad, Facebook's login dialog)
            // stays visible until the probe has confirmed it's the Sign-up page.
            boolean hide = Config.b("privacyOK", false) && age <= 300 && st != ST_OTHER
                    && (st == ST_SIGNUP || st == ST_PRESSED || isMrgs(w));
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

    // Every window of the app: the activities we've seen (the page is its own activity, MRGSWebViewActivity)
    // plus, when Android allows reading it, the window list itself (dialogs and popups).
    static List<View> roots() {
        List<View> out = new ArrayList<>();
        for (Activity a : BP.liveActivities()) {
            try { View d = a.getWindow().getDecorView(); if (d != null && !out.contains(d)) out.add(d); } catch (Throwable ignored) {}
        }
        try {
            Class<?> wmg = Class.forName("android.view.WindowManagerGlobal");
            Object g = wmg.getMethod("getInstance").invoke(null);
            java.lang.reflect.Field f = wmg.getDeclaredField("mViews");
            f.setAccessible(true);
            Object views = f.get(g);
            if (views instanceof List) for (Object v : new ArrayList<Object>((List<?>) views)) if (v instanceof View && !out.contains(v)) out.add((View) v);
        } catch (Throwable ignored) {}
        return out;
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
                    sawSignUp = false;
                    w.evaluateJavascript(MARK_JS, null);
                }
            } else if ("other".equals(r)) {
                state.put(w, ST_OTHER);
                reveal(w);
            }
        });
    }

    // MRGS's own activity (MRGSWebViewActivity) or an MRGS view somewhere above the web view.
    static boolean isMrgs(WebView w) {
        try {
            for (Object o = w; o instanceof View; o = ((View) o).getParent())
                if (o.getClass().getName().toLowerCase(Locale.US).contains("mrgs")) return true;
            Context c = w.getContext();
            for (int i = 0; i < 8 && c instanceof ContextWrapper; i++) {
                if (c instanceof Activity) return c.getClass().getName().toLowerCase(Locale.US).contains("mrgs");
                c = ((ContextWrapper) c).getBaseContext();
            }
        } catch (Throwable ignored) {}
        return false;
    }

    static void conceal(WebView w) {
        // Hide the top-most ancestor that holds only the page. Stop below the window's content frame
        // (android.R.id.content also holds the game's own view when the page is shown over the game) and
        // never climb into Unity's player view: going higher would make the whole game invisible.
        View v = w;
        View root = w.getRootView();
        while (v.getParent() instanceof View) {
            View p = (View) v.getParent();
            if (p == root || p.getId() == android.R.id.content || p.getClass().getName().startsWith("com.unity3d.")) break;
            v = p;
        }
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
