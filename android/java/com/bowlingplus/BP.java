package com.bowlingplus;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Choreographer;
import android.view.Display;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;

import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.lang.reflect.Method;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;

// The Android side of BowlingPlus: start-up, the per-frame tick, the shake detector (CoreMotion on iOS),
// the pin-layout tap listener (Game.cpp BFPinTapAt), and the small bridges the native side calls back
// into (encodePng, assetList, httpTest). The menu itself is Menu.java; the oil UI is Oil.java.
//
// Nothing here is on screen until you shake. The game's own Activity, views and threads are untouched.
public final class BP {
    static final String TAG = "BowlingPlus";
    static final Handler UI = new Handler(Looper.getMainLooper());
    static Context app;
    static volatile Activity activity;
    static long lastShake = 0;

    private BP() {}

    // Called from JNI_OnLoad, on Unity's load thread. Hook into the app's activity lifecycle, then start
    // once the game's activity exists (the native engine starts with it).
    public static void boot() {
        try {
            app = currentApplication();
            if (app == null) { Log.e(TAG, "no application context"); return; }
            hookLifecycle((Application) app.getApplicationContext());
            // if an activity is already up (e.g. a late inject), start now
            Activity a = currentActivity();
            if (a != null) onActivity(a);
            Log.i(TAG, "BowlingPlus Java side up");
        } catch (Throwable t) { Log.e(TAG, "boot failed", t); }
    }

    private static Application currentApplication() throws Exception {
        Class<?> at = Class.forName("android.app.ActivityThread");
        Object thread = at.getMethod("currentActivityThread").invoke(null);
        return (Application) at.getMethod("getApplication").invoke(thread);
    }

    @SuppressWarnings("unchecked")
    private static Activity currentActivity() {
        try {
            Class<?> at = Class.forName("android.app.ActivityThread");
            Object thread = at.getMethod("currentActivityThread").invoke(null);
            java.lang.reflect.Field f = at.getDeclaredField("mActivities");
            f.setAccessible(true);
            java.util.Map<Object, Object> map = (java.util.Map<Object, Object>) f.get(thread);
            for (Object rec : map.values()) {
                Class<?> rc = rec.getClass();
                java.lang.reflect.Field paused = rc.getDeclaredField("paused");
                paused.setAccessible(true);
                if (!paused.getBoolean(rec)) {
                    java.lang.reflect.Field af = rc.getDeclaredField("activity");
                    af.setAccessible(true);
                    return (Activity) af.get(rec);
                }
            }
        } catch (Throwable ignored) {}
        return null;
    }

    private static void hookLifecycle(Application a) {
        a.registerActivityLifecycleCallbacks(new Application.ActivityLifecycleCallbacks() {
            public void onActivityResumed(Activity act) { onActivity(act); }
            public void onActivityCreated(Activity act, android.os.Bundle b) {}
            public void onActivityStarted(Activity act) {}
            public void onActivityPaused(Activity act) {}
            public void onActivityStopped(Activity act) {}
            public void onActivitySaveInstanceState(Activity act, android.os.Bundle b) {}
            public void onActivityDestroyed(Activity act) { if (activity == act) activity = null; }
        });
    }

    static boolean started = false;

    static void onActivity(final Activity act) {
        if (!isGameActivity(act)) return;   // the game's UnityPlayerActivity, not an ad/SDK one
        activity = act;
        if (started) { UI.post(() -> { applyFps(); installTap(act); }); return; }
        started = true;
        UI.post(() -> {
            try {
                JSONObject a = new JSONObject();
                a.put("filesDir", act.getFilesDir().getAbsolutePath());
                a.put("device", deviceLine());
                a.put("maxHz", maxHz(act));
                String cfg = N.call("start", a.toString());   // loads settings, starts the crash guard
                Config.load(cfg);
                Privacy.start();
                installTap(act);
                startTick();
                applyFps();
            } catch (Throwable t) { Log.e(TAG, "start failed", t); }
        });
    }

    private static boolean isGameActivity(Activity act) {
        for (Class<?> c = act.getClass(); c != null; c = c.getSuperclass())
            if (c.getName().equals("com.unity3d.player.UnityPlayerActivity")) return true;
        return false;
    }

    // ---- per-frame tick (iOS: a CADisplayLink on the main thread Unity uses) ----
    // Unity runs on its own thread. We can't post onto it directly, so we post a Runnable onto Unity's
    // main-thread job queue via UnityPlayer.invokeOnMainThread(Runnable); it runs that between frames, and
    // we drive it once per screen refresh with the Choreographer. N.tick() then runs on Unity's thread.
    private static Method sInvokeOnMainThread;
    private static Object sUnityPlayer;
    private static final Runnable sTickJob = () -> { try { N.tick(); } catch (Throwable ignored) {} };

    static void startTick() {
        Choreographer.getInstance().postFrameCallback(new Choreographer.FrameCallback() {
            public void doFrame(long frameTimeNanos) {
                postToGame(sTickJob);
                Choreographer.getInstance().postFrameCallback(this);
            }
        });
    }

    // Also used by the menu to make the game look at its queued actions on the very next frame.
    static void postToGame(Runnable job) {
        try {
            if (sInvokeOnMainThread == null) resolveUnityPlayer();
            if (sInvokeOnMainThread != null && sUnityPlayer != null)
                sInvokeOnMainThread.invoke(sUnityPlayer, job);
        } catch (Throwable ignored) {}
    }

    static void poke() { postToGame(() -> { try { N.drain(); } catch (Throwable ignored) {} }); }

    private static void resolveUnityPlayer() {
        try {
            Activity act = activity;
            if (act == null) return;
            // UnityPlayerActivity.mUnityPlayer (a UnityPlayer or UnityPlayerForActivityOrService)
            Object up = null;
            for (Class<?> c = act.getClass(); c != null && up == null; c = c.getSuperclass()) {
                try {
                    java.lang.reflect.Field f = c.getDeclaredField("mUnityPlayer");
                    f.setAccessible(true);
                    up = f.get(act);
                } catch (NoSuchFieldException ignored) {}
            }
            if (up == null) return;
            Class<?> upc = Class.forName("com.unity3d.player.UnityPlayer");
            sInvokeOnMainThread = upc.getMethod("invokeOnMainThread", Runnable.class);
            sInvokeOnMainThread.setAccessible(true);
            sUnityPlayer = up;
        } catch (Throwable t) { Log.e(TAG, "couldn't find UnityPlayer.invokeOnMainThread", t); }
    }

    // ---- shake detector (Shake.mm) ----
    static SensorManager sensors;
    static float gx, gy, gz;
    static boolean gInit;
    static int lastSign, peakCount;
    static final long[] peakTimes = new long[8];

    static void startShake() {
        if (sensors != null || app == null) return;
        sensors = (SensorManager) app.getSystemService(Context.SENSOR_SERVICE);
        Sensor accel = sensors == null ? null : sensors.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
        if (accel == null) { Log.w(TAG, "no accelerometer"); return; }
        sensors.registerListener(new SensorEventListener() {
            public void onSensorChanged(SensorEvent e) { onAccel(e.values[0], e.values[1], e.values[2], e.timestamp / 1e9); }
            public void onAccuracyChanged(Sensor s, int a) {}
        }, accel, SensorManager.SENSOR_DELAY_GAME);
    }

    static void onAccel(double ax0, double ay0, double az0, double t) {
        // Android reports m/s^2 (including gravity ~9.8); iOS reported g. Convert so the 1.3 threshold matches.
        double ax = ax0 / 9.81, ay = ay0 / 9.81, az = az0 / 9.81;
        if (!gInit) { gx = (float) ax; gy = (float) ay; gz = (float) az; gInit = true; return; }
        gx = gx * 0.9f + (float) ax * 0.1f;
        gy = gy * 0.9f + (float) ay * 0.1f;
        gz = gz * 0.9f + (float) az * 0.1f;
        double hx = ax - gx, hy = ay - gy, hz = az - gz;
        if (Math.sqrt(hx * hx + hy * hy + hz * hz) < 1.3) return;
        double dx = Math.abs(hx), dy = Math.abs(hy), dz = Math.abs(hz);
        double dom = (dx >= dy && dx >= dz) ? hx : (dy >= dz ? hy : hz);
        int sign = dom > 0 ? 1 : -1;
        if (sign == lastSign) return;
        lastSign = sign;
        int k = 0;
        for (int i = 0; i < peakCount; i++) if (t - peakTimes[i] < 1.0) peakTimes[k++] = peakTimes[i];
        peakCount = k;
        if (peakCount < 8) peakTimes[peakCount++] = (long) t;
        if (peakCount >= 3) { peakCount = 0; lastSign = 0; handleShake(); }
    }

    static void handleShake() {
        long now = System.currentTimeMillis();
        if (now - lastShake < 1300) return;
        lastShake = now;
        UI.post(() -> Menu.toggle(activity));
    }

    // ---- tap the game's pin layouts (BFPinTapAt): a pass-through touch spy on the game's content view ----
    static void installTap(Activity act) {
        try {
            final View content = act.findViewById(android.R.id.content);
            if (content == null || content.getTag(0x7f0b0001) != null) return;
            content.setTag(0x7f0b0001, Boolean.TRUE);
            content.setOnTouchListener((v, ev) -> {
                if (ev.getActionMasked() == MotionEvent.ACTION_UP && !Menu.visible() && !Menu.pickerVisible()
                        && v.getWidth() > 0 && v.getHeight() > 0) {
                    float u = ev.getX() / v.getWidth();
                    float vv = 1f - ev.getY() / v.getHeight();   // v up, like iOS
                    N.call("pinTap", u + "," + vv);              // native decides if it's on a layout
                }
                return false;   // never consume: the game still gets every touch
            });
        } catch (Throwable t) { Log.e(TAG, "tap listener failed", t); }
    }

    // ---- 120 FPS: ask the window for the display's fastest mode while it's on ----
    static void applyFps() {
        try {
            Activity act = activity;
            if (act == null) return;
            Window w = act.getWindow();
            WindowManager.LayoutParams lp = w.getAttributes();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                int best = bestModeId(act);
                if (Config.get().optBoolean("fps120", false) && best > 0) lp.preferredDisplayModeId = best;
                else lp.preferredDisplayModeId = 0;
                w.setAttributes(lp);
            }
        } catch (Throwable ignored) {}
    }

    static int bestModeId(Activity act) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return 0;
        Display d = displayOf(act);
        if (d == null) return 0;
        Display.Mode cur = d.getMode();
        Display.Mode best = cur;
        for (Display.Mode m : d.getSupportedModes())
            if (m.getPhysicalWidth() == cur.getPhysicalWidth() && m.getPhysicalHeight() == cur.getPhysicalHeight()
                    && m.getRefreshRate() > best.getRefreshRate() + 0.5f) best = m;
        return best.getRefreshRate() > cur.getRefreshRate() + 0.5f ? best.getModeId() : 0;
    }

    static int maxHz(Activity act) {
        try {
            Display d = displayOf(act);
            if (d == null) return 60;
            float max = d.getRefreshRate();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M)
                for (Display.Mode m : d.getSupportedModes()) max = Math.max(max, m.getRefreshRate());
            return Math.round(max);
        } catch (Throwable t) { return 60; }
    }

    @SuppressWarnings("deprecation")
    static Display displayOf(Activity act) {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && act.getDisplay() != null) return act.getDisplay();
        } catch (Throwable ignored) {}
        return act.getWindowManager().getDefaultDisplay();
    }

    static String deviceLine() {
        return "Android " + Build.VERSION.RELEASE + " (API " + Build.VERSION.SDK_INT + ") | " + Build.MANUFACTURER + " " + Build.MODEL;
    }

    // =========================================================================
    // called from the native side (Jni.cpp)
    // =========================================================================

    // post a command to the Android UI thread: show/hide the menu, the pin picker, the skip button
    public static void ui(final String cmd, final String arg) {
        UI.post(() -> {
            try {
                switch (cmd) {
                    case "picker": {
                        String[] p = arg.split(",");
                        Menu.showPicker(activity, Integer.parseInt(p[0]), p.length > 1 && p[1].equals("1"));
                        break;
                    }
                    case "hidePicker": Menu.hidePicker(); break;
                    case "skip": Menu.setSkipVisible(activity, arg.equals("1")); break;
                }
            } catch (Throwable t) { Log.e(TAG, "ui " + cmd, t); }
        });
    }

    public static byte[] encodePng(byte[] rgba, int w, int h) {
        try {
            Bitmap bmp = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
            int[] px = new int[w * h];
            for (int i = 0; i < w * h; i++) {
                int o = i * 4;
                px[i] = Color.argb(rgba[o + 3] & 0xff, rgba[o] & 0xff, rgba[o + 1] & 0xff, rgba[o + 2] & 0xff);
            }
            bmp.setPixels(px, 0, w, 0, 0, w, h);
            ByteArrayOutputStream bos = new ByteArrayOutputStream();
            bmp.compress(Bitmap.CompressFormat.PNG, 100, bos);
            bmp.recycle();
            return bos.toByteArray();
        } catch (Throwable t) { return null; }
    }

    // Every "- Assets/..." line of every .manifest in the APK's AssetBundles folder (Game.cpp AppAssetNames).
    public static String assetList() {
        try {
            if (app == null) return "";
            AssetManager am = app.getAssets();
            StringBuilder sb = new StringBuilder();
            collectManifests(am, "AssetBundles", sb);
            return sb.toString();
        } catch (Throwable t) { return ""; }
    }

    private static void collectManifests(AssetManager am, String dir, StringBuilder sb) throws Exception {
        String[] list = am.list(dir);
        if (list == null) return;
        for (String name : list) {
            String path = dir + "/" + name;
            if (name.endsWith(".manifest")) {
                try (InputStream is = am.open(path)) {
                    byte[] buf = new byte[is.available()];
                    int n = is.read(buf);
                    if (n > 0) {
                        for (String line : new String(buf, 0, n).split("\n"))
                            if (line.contains("- Assets/")) sb.append(line).append('\n');
                    }
                } catch (Throwable ignored) {}
            } else {
                String[] sub = am.list(path);
                if (sub != null && sub.length > 0) collectManifests(am, path, sb);
            }
        }
    }

    public static void httpTest(final String url) {
        try {
            long t = System.nanoTime();
            HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
            c.setConnectTimeout(10000);
            c.setReadTimeout(12000);
            c.setInstanceFollowRedirects(true);
            int code = c.getResponseCode();
            InputStream is = code >= 400 ? c.getErrorStream() : c.getInputStream();
            int bytes = 0;
            if (is != null) { byte[] b = new byte[4096]; int n; while ((n = is.read(b)) > 0) bytes += n; is.close(); }
            double ms = (System.nanoTime() - t) / 1e6;
            logEvent("net", String.format("HTTP %s: status %d, %d bytes in %.0f ms", url, code, bytes, ms));
            c.disconnect();
        } catch (Throwable e) {
            logEvent("net", "HTTP " + url + ": FAILED (" + e + ")");
        }
    }

    static void logEvent(String src, String msg) { try { N.call("logEvent", src + "\t" + msg); } catch (Throwable ignored) {} }
}
