package com.bowlingplus;

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
        try { setBehavior.invoke(getInstance.invoke(null), webOnly); } catch (Throwable ignored) {}
        try { watch(); } catch (Throwable ignored) {}
        BP.UI.postDelayed(FbLogin::tick, 600);
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
