package com.bowlingplus;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;

import java.lang.ref.WeakReference;
import java.lang.reflect.Method;
import java.util.Set;

// Facebook login help, entirely through the SDK that's already in the game - nothing about the app's identity
// is changed or faked.
//
// Why: the game's normal login hands off to the installed Facebook app (app-switch "SSO"). That path sends the
// APK's signing-key hash to Facebook, and because BowlingPlus is re-signed, Facebook rejects it ("invalid key
// hash"). The SDK also supports a browser flow (a Chrome Custom Tab opens Facebook's real login page); that
// flow identifies the app by its App ID and redirect, not the signing key, so it isn't affected. Both flows
// use the game's own, unchanged Facebook App ID, and you log into your own account on Facebook's own page.
//
// What this does:
//   * sets the shared LoginManager's behaviour to WEB_ONLY, so the next login the game starts uses the browser
//     flow instead of the app-switch. LoginManager.getInstance() is a process-wide singleton that the game's
//     login button uses too, so setting it here applies to the game's own login call. Re-asserted on a timer
//     because the game may set its own behaviour right before logging in.
//   * watches the SDK's current access token and profile and writes any change to the event log (Copy log),
//     so you can see whether a Facebook login actually succeeded and what it returned.
//   * logs what comes back from the browser (inspectIntent): Facebook's page redirects to
//     fbconnect://cct.<package>, which Android hands to com.facebook.CustomTabActivity. The redirect carries
//     either a token (only its presence is logged, never the value) or Facebook's own error message, e.g.
//     "Invalid key hash", which settles whether the browser flow works on a re-signed build.
//
// All of this is reflection against classes already in the APK, so it simply does nothing on a build where the
// Facebook SDK is absent or renamed.
final class FbLogin {
    private FbLogin() {}

    private static Class<?> loginManager, loginBehavior, accessToken, profile;
    private static Object webOnly;
    private static Method getInstance, setBehavior, getToken, getUserId, getPermissions, curToken, curProfile, pName, pId;
    private static boolean ready;
    private static String lastToken = "", lastProfile = "";

    static void start() {
        if (!Config.b("fbWebLogin", true)) return;   // on by default: it's the only login path that can work re-signed
        try {
            loginManager = Class.forName("com.facebook.login.LoginManager");
            loginBehavior = Class.forName("com.facebook.login.LoginBehavior");
            accessToken = Class.forName("com.facebook.AccessToken");
            profile = Class.forName("com.facebook.Profile");
            getInstance = loginManager.getMethod("getInstance");
            setBehavior = loginManager.getMethod("setLoginBehavior", loginBehavior);
            @SuppressWarnings({"unchecked", "rawtypes"})
            Object w = Enum.valueOf((Class<Enum>) loginBehavior.asSubclass(Enum.class), "WEB_ONLY");
            webOnly = w;
            getToken = accessToken.getMethod("getToken");
            getUserId = accessToken.getMethod("getUserId");
            getPermissions = accessToken.getMethod("getPermissions");
            curToken = accessToken.getMethod("getCurrentAccessToken");
            curProfile = profile.getMethod("getCurrentProfile");
            pName = profile.getMethod("getName");
            pId = profile.getMethod("getId");
            ready = true;
        } catch (Throwable t) {
            N.logLine("fb", "Facebook SDK not found, login help off (" + t + ")");
            return;
        }
        N.logLine("fb", "forcing the browser login flow (WEB_ONLY) and watching for a login");
        tick();
    }

    private static void tick() {
        if (!ready) return;
        if (!Config.b("fbWebLogin", true)) { ready = false; N.logLine("fb", "browser login turned off (restart the game to use the Facebook app again)"); return; }
        try { setBehavior.invoke(getInstance.invoke(null), webOnly); } catch (Throwable ignored) {}
        try { watch(); } catch (Throwable ignored) {}
        BP.UI.postDelayed(FbLogin::tick, 600);
    }

    private static WeakReference<Intent> lastIntent = new WeakReference<>(null);

    // Called by BP's lifecycle hook for every activity that is created or resumed. Only Facebook's own
    // activities are looked at. Must never throw: an exception in a lifecycle callback crashes the game.
    static void inspectIntent(Activity act) {
        try {
            if (act == null) return;
            String cls = act.getClass().getName();
            if (!cls.startsWith("com.facebook.")) return;
            Intent in = act.getIntent();
            if (in == null || in == lastIntent.get()) return;          // created + resumed: log it once
            lastIntent = new WeakReference<>(in);
            StringBuilder sb = new StringBuilder(cls.substring(cls.lastIndexOf('.') + 1));
            String url = in.getData() != null ? in.getData().toString() : null;
            if (url == null) {   // CustomTabMainActivity gets the redirect as an extra
                try { url = in.getStringExtra("CustomTabMainActivity.extra_url"); } catch (Throwable ignored) {}
            }
            if (url == null) { N.logLine("fb", sb.append(" opened").toString()); return; }
            Uri u = Uri.parse(url);
            sb.append(" redirect ").append(u.getScheme()).append("://").append(u.getHost());
            boolean token = false;
            StringBuilder err = new StringBuilder();
            for (String part : new String[]{ u.getEncodedQuery(), u.getEncodedFragment() }) {
                if (part == null) continue;
                for (String kv : part.split("&")) {
                    int eq = kv.indexOf('=');
                    String k = Uri.decode(eq < 0 ? kv : kv.substring(0, eq));
                    String v = eq < 0 ? "" : Uri.decode(kv.substring(eq + 1).replace('+', ' '));
                    if (k.equals("access_token") || k.equals("signed_request") || k.equals("code") || k.equals("id_token")) token = true;   // secrets: never logged
                    else if (k.startsWith("error")) err.append(' ').append(k).append('=').append(v.length() > 300 ? v.substring(0, 300) : v);
                }
            }
            if (token) sb.append(": login returned a token");
            else if (err.length() == 0) sb.append(": no token and no error in the redirect");
            N.logLine("fb", sb.append(err).toString());
        } catch (Throwable ignored) {}
    }

    @SuppressWarnings("unchecked")
    private static void watch() throws Exception {
        Object tok = curToken.invoke(null);
        String now = "";
        if (tok != null) {
            String uid = String.valueOf(getUserId.invoke(tok));
            Set<String> perms = (Set<String>) getPermissions.invoke(tok);
            String t = String.valueOf(getToken.invoke(tok));
            now = uid + "|" + perms + "|" + t.length();
            if (!now.equals(lastToken)) {
                // the token itself is a secret, so log only its length and the account id, never the token
                N.logLine("fb", "Facebook login OK: user id " + uid + ", permissions " + perms + ", token " + t.length() + " chars");
                N.logLine("fb", "watch the next lines for which host the game asks for your profile (turn on 'Log all server hosts')");
            }
        } else if (!lastToken.isEmpty()) {
            N.logLine("fb", "Facebook session cleared (logged out or expired)");
        }
        lastToken = now;

        Object pr = curProfile.invoke(null);
        String pnow = pr == null ? "" : (pId.invoke(pr) + "|" + pName.invoke(pr));
        if (!pnow.equals(lastProfile) && pr != null) {
            N.logLine("fb", "Facebook profile from the SDK: " + pName.invoke(pr) + " (id " + pId.invoke(pr) + ")");
        }
        lastProfile = pnow;
    }
}
