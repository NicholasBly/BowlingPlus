package com.bowlingplus;

// The bridge to the native side (Jni.cpp registers these). Everything the menu, the oil library and the
// pin picture need goes through N.call(cmd, arg); a few take raw bytes.
public final class N {
    private N() {}

    // cmd + arg (JSON or a small string) -> result (JSON or text), or null. Commands that read or change
    // game state are run on Unity's thread inside the native side; UI-only ones return at once.
    public static native String call(String cmd, String arg);

    public static native void tick();     // once per frame on Unity's thread (BP schedules this)
    public static native void drain();    // run just the menu's queued game actions, now

    public static native byte[] bytes(String name);                         // embedded PNGs: pinGuide, wrapTemplate, logo...
    public static native byte[] pinWrap(byte[] src, int ww, int wh, byte[] tmpl, int side);   // wrap sheet -> game layout + fill
    public static native void pinFill(byte[] rgba, int side, boolean cleanEdges);              // game-layout fill

    // convenience
    static String call(String cmd) { return call(cmd, ""); }
}
