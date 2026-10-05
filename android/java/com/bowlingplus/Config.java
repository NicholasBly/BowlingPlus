package com.bowlingplus;

import org.json.JSONObject;

// A UI-side mirror of the native settings (BFConfig). The native side owns them and saves them; every change
// goes through set(), which writes to native (which clamps, saves, and applies on the next frame). The menu
// reads the latest with state() each time it opens.
public final class Config {
    private static JSONObject cfg = new JSONObject();

    private Config() {}

    static synchronized void load(String json) {
        try { if (json != null) cfg = new JSONObject(json); } catch (Throwable ignored) {}
    }

    static synchronized JSONObject get() { return cfg; }

    // The native side sends switches as booleans, but a value set from here may be a number: accept both.
    static boolean b(String key, boolean def) {
        Object v = get().opt(key);
        if (v instanceof Boolean) return (Boolean) v;
        if (v instanceof Number) return ((Number) v).doubleValue() != 0;
        if (v instanceof String) return "true".equals(v) || "1".equals(v);
        return def;
    }
    static double d(String key, double def) { return get().optDouble(key, def); }
    static int i(String key, int def) { return get().optInt(key, def); }

    static synchronized void set(String key, boolean v) {
        try { cfg.put(key, v); } catch (Throwable ignored) {}
        send(key, v ? 1.0 : 0.0);
    }

    static synchronized void set(String key, double v) {
        try { cfg.put(key, v); } catch (Throwable ignored) {}
        send(key, v);
    }

    private static void send(String key, double v) {
        N.call("set", key + "=" + v);
        // some switches change what the window should ask for (120 Hz)
        if (key.equals("fps120")) BP.UI.post(BP::applyFps);
    }

    // Pull the native side's current view (after it clamps, after the game changes something).
    static void refresh() {
        String s = N.call("config");
        if (s != null) load(s);
    }
}
