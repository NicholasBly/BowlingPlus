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
import java.lang.ref.WeakReference;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.concurrent.atomic.AtomicBoolean;
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

    // Called from JNI_OnLoad (Jni.cpp), on the thread that loads libmain.so: the game's activity is being
    // created at that moment. Hook the activity lifecycle, start the shake detector, and start for real once the
    // game's activity is up (onActivity).
    public static void boot() {
        try {
            app = currentApplication();
            if (app == null) { Log.e(TAG, "no application context"); return; }
            hookLifecycle((Application) app.getApplicationContext());
            UI.post(BP::startShake);   // sensors need a Looper thread
            Activity a = unityActivity();
            if (a == null) a = currentActivity();
            if (a != null) onActivity(a);
            Log.i(TAG, "BowlingPlus Java side up");
        } catch (Throwable t) { Log.e(TAG, "boot failed", t); }
    }

    // UnityPlayer.currentActivity: set by Unity as soon as its player exists (that is what loads libmain.so)
    private static Activity unityActivity() {
        try {
            java.lang.reflect.Field f = Class.forName("com.unity3d.player.UnityPlayer").getField("currentActivity");
            return (Activity) f.get(null);
        } catch (Throwable t) { return null; }
    }

    private static Application currentApplication() {
        try {
            return (Application) Class.forName("android.app.ActivityThread").getMethod("currentApplication").invoke(null);
        } catch (Throwable t) {
            Activity a = unityActivity();
            return a != null ? a.getApplication() : null;
        }
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
            // An exception thrown out of any of these callbacks crashes the game, so every one is guarded.
            public void onActivityResumed(Activity act) {
                try {
                    if (!liveActivities().contains(act)) resumed.add(new WeakReference<>(act));
                    FbLogin.inspectIntent(act);
                    onActivity(act);
                    MenuButton.refresh();
                } catch (Throwable t) { Log.e(TAG, "onActivityResumed", t); }
            }
            public void onActivityCreated(Activity act, android.os.Bundle b) { FbLogin.inspectIntent(act); }
            public void onActivityStarted(Activity act) {}
            public void onActivityPaused(Activity act) {}
            public void onActivityStopped(Activity act) {}
            public void onActivitySaveInstanceState(Activity act, android.os.Bundle b) {}
            public void onActivityDestroyed(Activity act) {
                try {
                    if (activity == act) activity = null;
                    for (int i = resumed.size() - 1; i >= 0; i--) { Activity r = resumed.get(i).get(); if (r == null || r == act) resumed.remove(i); }
                } catch (Throwable t) { Log.e(TAG, "onActivityDestroyed", t); }
            }
        });
    }

    static volatile boolean started = false;   // boot() (JNI_OnLoad's thread) and the lifecycle callback can both get here
    // every activity of the app that has been resumed and not destroyed (Privacy looks at all of them: the
    // privacy page is its own activity, MRGSWebViewActivity)
    static final List<WeakReference<Activity>> resumed = new ArrayList<>();

    static List<Activity> liveActivities() {
        List<Activity> out = new ArrayList<>();
        for (int i = resumed.size() - 1; i >= 0; i--) {
            Activity a = resumed.get(i).get();
            if (a == null || a.isFinishing()) { resumed.remove(i); continue; }
            if (!out.contains(a)) out.add(a);
        }
        return out;
    }

    static void onActivity(final Activity act) {
        if (!isGameActivity(act)) return;   // the game's UnityPlayerActivity, not an ad/SDK one
        activity = act;
        synchronized (BP.class) {
            if (started) { UI.post(() -> { applyFps(); installTap(act); }); return; }
            started = true;
        }
        UI.post(() -> {
            try {
                JSONObject a = new JSONObject();
                a.put("filesDir", act.getFilesDir().getAbsolutePath());
                a.put("device", deviceLine());
                a.put("maxHz", maxHz(act));
                String cfg = N.call("start", a.toString());   // loads settings, starts the crash guard
                Config.load(cfg);
                Oil.start();                                  // the custom pattern you had on last time
                Pins.migrate();                               // 1.6.8: your pin picture joins the pin library
                OilTab.start();                               // the "Custom oil" tab on the practice pattern screen
                MenuButton.start();                           // optional draggable on-screen button to open the menu
                FbLogin.start();                              // force the browser login flow + watch for a Facebook login
                Privacy.start();
                UiKit.toast(act, "BowlingPlus is on: shake the phone (or tap with three fingers) for the menu");
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
    // One tick in flight at most: the screen refreshes faster than the game draws (120 Hz screen, 30/60 FPS
    // game), and the game logic must run once per game frame, not once per refresh.
    private static final AtomicBoolean sTickPending = new AtomicBoolean(false), sDrainPending = new AtomicBoolean(false);
    private static final Runnable sTickJob = () -> { sTickPending.set(false); try { N.tick(); } catch (Throwable ignored) {} };
    private static final Runnable sDrainJob = () -> { sDrainPending.set(false); try { N.drain(); } catch (Throwable ignored) {} };

    static void startTick() {
        Choreographer.getInstance().postFrameCallback(new Choreographer.FrameCallback() {
            public void doFrame(long frameTimeNanos) {
                if (sTickPending.compareAndSet(false, true) && !postToGame(sTickJob)) sTickPending.set(false);
                Choreographer.getInstance().postFrameCallback(this);
            }
        });
    }

    // Also used by the menu to make the game look at its queued actions on the very next frame.
    static boolean postToGame(Runnable job) {
        try {
            if (sInvokeOnMainThread == null) resolveUnityPlayer();
            if (sInvokeOnMainThread != null && sUnityPlayer != null) {
                sInvokeOnMainThread.invoke(sUnityPlayer, job);
                return true;
            }
        } catch (Throwable ignored) {}
        return false;
    }

    // Make the game run the menu's queued actions on its very next frame. Also called by the native side
    // (Jni.cpp RunOnGame) whenever it queues something, from any thread.
    public static void poke() {
        if (sDrainPending.compareAndSet(false, true) && !postToGame(sDrainJob)) sDrainPending.set(false);
    }

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
    // Android reports acceleration INCLUDING gravity (iOS' userAcceleration doesn't), so a slow low-pass filter
    // estimates gravity and what's left is the shake. A shake = three quick direction reversals within 1.2 s,
    // each stronger than SHAKE_G. (1.3 g was too stiff for a phone as heavy as a flagship: 0.9 g is still far
    // above anything a bowling swipe does to a phone held in the hand.)
    static final double SHAKE_G = 0.9, SHAKE_WINDOW = 1.2;
    static SensorManager sensors;
    static float gx, gy, gz;
    static boolean gInit;
    static int lastSign, peakCount;
    static final double[] peakTimes = new double[8];

    static void startShake() {
        if (sensors != null || app == null) return;
        sensors = (SensorManager) app.getSystemService(Context.SENSOR_SERVICE);
        Sensor accel = sensors == null ? null : sensors.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
        if (accel == null) { Log.w(TAG, "no accelerometer: use a three-finger tap to open the menu"); return; }
        boolean ok = sensors.registerListener(new SensorEventListener() {
            public void onSensorChanged(SensorEvent e) { onAccel(e.values[0], e.values[1], e.values[2], e.timestamp / 1e9); }
            public void onAccuracyChanged(Sensor s, int a) {}
        }, accel, SensorManager.SENSOR_DELAY_GAME);
        Log.i(TAG, "shake detector " + (ok ? "listening" : "FAILED to register") + " (three-finger tap also opens the menu)");
    }

    static void onAccel(double ax0, double ay0, double az0, double t) {
        double ax = ax0 / 9.81, ay = ay0 / 9.81, az = az0 / 9.81;   // in g
        if (!gInit) { gx = (float) ax; gy = (float) ay; gz = (float) az; gInit = true; return; }
        gx = gx * 0.96f + (float) ax * 0.04f;
        gy = gy * 0.96f + (float) ay * 0.04f;
        gz = gz * 0.96f + (float) az * 0.04f;
        double hx = ax - gx, hy = ay - gy, hz = az - gz;
        if (Math.sqrt(hx * hx + hy * hy + hz * hz) < SHAKE_G) return;
        double dx = Math.abs(hx), dy = Math.abs(hy), dz = Math.abs(hz);
        double dom = (dx >= dy && dx >= dz) ? hx : (dy >= dz ? hy : hz);
        int sign = dom > 0 ? 1 : -1;
        if (sign == lastSign) return;
        lastSign = sign;
        int k = 0;
        for (int i = 0; i < peakCount; i++) if (t - peakTimes[i] < SHAKE_WINDOW) peakTimes[k++] = peakTimes[i];
        peakCount = k;
        if (peakCount < 8) peakTimes[peakCount++] = t;
        if (peakCount >= 3) { peakCount = 0; lastSign = 0; Log.i(TAG, "shake detected"); handleShake(); }
    }

    static void handleShake() {
        long now = System.currentTimeMillis();
        if (now - lastShake < 1300) return;
        lastShake = now;
        UI.post(() -> Menu.toggle(activity));
    }

    // ---- tap the game's pin layouts (BFPinTapAt) ----
    // Unity's own view takes every touch, so a listener on a view never hears about them. Instead we sit in
    // front of the activity's Window.Callback (the first stop of every touch) and only watch: every call goes
    // on to the game unchanged.
    static void installTap(Activity act) {
        try {
            Window w = act.getWindow();
            Window.Callback cb = w.getCallback();
            if (cb == null) return;
            if (Proxy.isProxyClass(cb.getClass()) && Proxy.getInvocationHandler(cb) instanceof TapSpy) return;
            Object spy = Proxy.newProxyInstance(BP.class.getClassLoader(), new Class<?>[]{ Window.Callback.class }, new TapSpy(cb, w));
            w.setCallback((Window.Callback) spy);
        } catch (Throwable t) { Log.e(TAG, "tap listener failed", t); }
    }

    static final class TapSpy implements InvocationHandler {
        final Window.Callback orig;
        final Window window;
        float downX, downY;
        long downAt;
        boolean multi;                       // a second finger touched during this gesture: not a pin tap
        boolean triOn;                       // three fingers are down together (the menu gesture)
        long triAt;
        float triX, triY;
        TapSpy(Window.Callback orig, Window window) { this.orig = orig; this.window = window; }

        @Override public Object invoke(Object proxy, Method m, Object[] args) throws Throwable {
            if (args != null && args.length == 1 && args[0] instanceof MotionEvent && "dispatchTouchEvent".equals(m.getName())) {
                try { watch((MotionEvent) args[0]); } catch (Throwable ignored) {}
            }
            try { return m.invoke(orig, args); } catch (InvocationTargetException e) { throw e.getCause() != null ? e.getCause() : e; }
        }

        void watch(MotionEvent ev) {
            int a = ev.getActionMasked(), n = ev.getPointerCount();
            View d = window.getDecorView();
            if (d == null || d.getWidth() <= 0 || d.getHeight() <= 0) return;
            float slop = 24 * d.getResources().getDisplayMetrics().density;

            // three fingers tapped together: open / close the menu (the backup for the shake)
            if (a == MotionEvent.ACTION_POINTER_DOWN && n == 3) { triOn = true; triAt = ev.getEventTime(); triX = ev.getX(0); triY = ev.getY(0); }
            else if (triOn && a == MotionEvent.ACTION_MOVE) {
                if (n < 3 || Math.abs(ev.getX(0) - triX) > slop * 2 || Math.abs(ev.getY(0) - triY) > slop * 2) triOn = false;   // a swipe, not a tap
            } else if (triOn && (a == MotionEvent.ACTION_POINTER_UP || a == MotionEvent.ACTION_UP)) {
                triOn = false;
                if (ev.getEventTime() - triAt < 800) { Log.i(TAG, "three-finger tap"); handleShake(); }
            } else if (a == MotionEvent.ACTION_CANCEL) triOn = false;

            if (a == MotionEvent.ACTION_DOWN) { downX = ev.getX(); downY = ev.getY(); downAt = ev.getEventTime(); multi = false; return; }
            if (a == MotionEvent.ACTION_POINTER_DOWN) { multi = true; return; }
            if (a != MotionEvent.ACTION_UP || multi || Menu.visible() || Menu.pickerVisible()) return;
            if (Math.abs(ev.getX() - downX) > slop || Math.abs(ev.getY() - downY) > slop || ev.getEventTime() - downAt > 600) return;   // a tap, not a swipe
            float u = ev.getX() / d.getWidth();
            float v = 1f - ev.getY() / d.getHeight();   // v up, like iOS
            N.call("pinTap", u + "," + v);               // native decides if it's on a layout (never blocks)
        }
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
                if (Config.b("fps120", false) && best > 0) lp.preferredDisplayModeId = best;
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
                    byte[] buf = Pickers.readAll(is);
                    for (String line : new String(buf, "UTF-8").split("\n"))
                        if (line.contains("- Assets/")) sb.append(line).append('\n');
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
