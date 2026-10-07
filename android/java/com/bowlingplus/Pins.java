package com.bowlingplus;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

// The pin library (1.6.8; iOS: OilUI.mm "pin library"). Your pin pictures and the built-in ones (PinPresets.h,
// through N.bytes("preset:<id>")), each kept in the game's layout under <pin_image dir>/pins/<id>.png (and the picture
// as picked, <id>_src.png). Choosing one copies it to pin_image.png, the file the native side watches.
// The menu switch (pinImage) still turns custom pins on and off; the last one chosen is kept.
public final class Pins {
    private Pins() {}

    static Activity act;

    // ========================= storage =========================
    static android.content.SharedPreferences prefs() { return Oil.prefs(); }
    static JSONArray load() { try { return new JSONArray(prefs().getString("pinLibrary", "[]")); } catch (Throwable t) { return new JSONArray(); } }
    static void save(JSONArray a) { prefs().edit().putString("pinLibrary", a.toString()).apply(); }
    static String lastId() { return prefs().getString("pinActive", ""); }
    static void setLastId(String id) { prefs().edit().putString("pinActive", id == null ? "" : id).apply(); }

    static File pinImage() { String p = N.call("pinImagePath"); return p == null ? null : new File(p); }
    static File dir() { File f = pinImage(); return f == null ? null : new File(f.getParentFile(), "pins"); }
    static File file(String id) { File d = dir(); return d == null ? null : new File(d, id + ".png"); }
    static File srcFile(String id) { File d = dir(); return d == null ? null : new File(d, id + "_src.png"); }

    static List<JSONObject> presets() {
        List<JSONObject> out = new ArrayList<>();
        try {
            JSONArray a = new JSONArray(N.call("pinPresets"));
            for (int i = 0; i < a.length(); i++) {
                JSONObject p = a.getJSONObject(i);
                JSONObject e = new JSONObject();
                e.put("id", "preset." + p.optString("id"));
                e.put("key", p.optString("id"));
                e.put("name", p.optString("name"));
                e.put("sub", p.optString("sub"));
                e.put("preset", true);
                out.add(e);
            }
        } catch (Throwable ignored) {}
        return out;
    }

    static JSONObject entry(String id) {
        if (id == null || id.isEmpty()) return null;
        for (JSONObject p : presets()) if (p.optString("id").equals(id)) return p;
        JSONArray a = load();
        for (int i = 0; i < a.length(); i++) if (a.optJSONObject(i).optString("id").equals(id)) return a.optJSONObject(i);
        return null;
    }

    static String activeId() { return Config.b("pinImage", false) && entry(lastId()) != null ? lastId() : ""; }

    static byte[] readAll(File f) {
        try (java.io.FileInputStream in = new java.io.FileInputStream(f)) {
            java.io.ByteArrayOutputStream bos = new java.io.ByteArrayOutputStream();
            byte[] buf = new byte[65536];
            int n;
            while ((n = in.read(buf)) > 0) bos.write(buf, 0, n);
            return bos.toByteArray();
        } catch (Throwable t) { return null; }
    }
    static boolean writeAll(File f, byte[] data) {
        try {
            f.getParentFile().mkdirs();
            File tmp = new File(f.getPath() + ".tmp");
            try (java.io.FileOutputStream out = new java.io.FileOutputStream(tmp)) { out.write(data); }
            return tmp.renameTo(f);
        } catch (Throwable t) { return false; }
    }

    // The entry's picture in the game's layout; a preset is converted on first use (a second or two: not on the
    // UI thread). null if it can't be made.
    static synchronized File ensureFile(String id) {
        File f = file(id);
        if (f == null) return null;
        if (f.exists()) return f;
        JSONObject e = entry(id);
        if (e == null || !e.optBoolean("preset")) return null;
        Oil.Processed r = Oil.processPin(N.bytes("preset:" + e.optString("key")));
        return r.png != null && writeAll(f, r.png) ? f : null;
    }

    // Puts an entry on the pins ("" or null: the game's own pins). Returns a line for the user.
    static String use(String id) {
        if (id == null || id.isEmpty()) { Config.set("pinImage", false); return "The game's own pins."; }
        File f = ensureFile(id);
        byte[] d = f != null ? readAll(f) : null;
        File target = pinImage();
        if (d == null || d.length == 0 || target == null) return "Couldn't read that pin picture.";
        if (!writeAll(target, d)) return "Couldn't save the pin picture.";
        setLastId(id);
        Config.set("pinImage", true);
        JSONObject e = entry(id);
        return "On the pins: " + (e != null ? e.optString("name") : "your picture");
    }

    static String useLast() { return use(entry(lastId()) != null ? lastId() : null); }
    static String activeName() { JSONObject e = entry(activeId()); return e != null ? e.optString("name") : null; }

    // 1.6.8: the picture you had becomes the first entry of the library ("My pin"). Runs once.
    static void migrate() {
        try {
            if (prefs().getBoolean("pinLibraryV1", false)) return;
            prefs().edit().putBoolean("pinLibraryV1", true).apply();
            File old = pinImage();
            if (old == null || !old.exists()) return;
            String id = "my." + java.util.UUID.randomUUID();
            byte[] d = readAll(old);
            if (d == null || !writeAll(file(id), d)) return;
            File src = new File(old.getParentFile(), "pin_image_source.png");
            if (src.exists()) { byte[] s = readAll(src); if (s != null) writeAll(srcFile(id), s); }
            JSONArray lib = load();
            JSONObject e = new JSONObject();
            e.put("id", id);
            e.put("name", "My pin");
            lib.put(e);
            save(lib);
            setLastId(id);
            N.logLine("pins", "pin library: your pin picture is now \"My pin\"");
        } catch (Throwable ignored) {}
    }

    // From the picker: convert (off the UI thread), save, add, put on the pins.
    static void addPicked(final byte[] imageBytes, final Oil.Cb done) {
        new Thread(() -> {
            Oil.Processed r = Oil.processPin(imageBytes);
            String id = "my." + java.util.UUID.randomUUID();
            boolean ok = r.png != null && writeAll(file(id), r.png);
            if (ok && imageBytes != null) writeAll(srcFile(id), imageBytes);
            BP.UI.post(() -> {
                if (!ok) { if (done != null) done.run(r.png == null ? r.msg : "Couldn't save the picture."); return; }
                try {
                    JSONArray lib = load();
                    JSONObject e = new JSONObject();
                    e.put("id", id);
                    e.put("name", "My pin " + (lib.length() + 1));
                    lib.put(e);
                    save(lib);
                } catch (Throwable ignored) {}
                String used = use(id);
                if (showing) select(id);
                if (done != null) done.run(used + " " + r.msg);
            });
        }).start();
    }

    // ========================= the spinning preview =========================
    // Renders the pin (native PinPreview.h, shared with iOS) about 30 times a second on its own thread.
    // Drag sideways to turn it.
    static final class Spin extends ImageView {
        private volatile boolean running = true;
        private volatile float angle = -0.6f;
        private volatile long holdUntil = 0;
        private volatile int texGen = 0;
        private volatile boolean hasTex = false;
        private float lastX;
        final TextView note;
        private Thread thread;

        Spin(Context c, TextView note) {
            super(c);
            this.note = note;
            setScaleType(ScaleType.FIT_CENTER);
            thread = new Thread(this::loop, "bp-pinspin");
            thread.start();
        }

        void stop() { running = false; }

        void show(final File f, final byte[] png) {
            final int gen = ++texGen;
            note.setText("Preparing\u2026");
            note.setVisibility(VISIBLE);
            new Thread(() -> {
                Bitmap b = png != null ? BitmapFactory.decodeByteArray(png, 0, png.length) : f != null ? BitmapFactory.decodeFile(f.getPath()) : null;
                byte[] rgba = b != null ? Oil.rgbaOnWhite(b, 1024, 1024) : null;
                if (gen != texGen) return;
                if (rgba != null) { N.pinPreviewTexture(rgba, 1024); hasTex = true; }
                post(() -> { if (gen == texGen) { note.setText(rgba != null ? "" : "Couldn't show this picture"); note.setVisibility(rgba != null ? GONE : VISIBLE); } });
            }).start();
        }

        private void loop() {
            long last = System.nanoTime();
            int[] px = null;
            Bitmap[] bufs = new Bitmap[2];
            int which = 0;
            while (running) {
                long now = System.nanoTime();
                float dt = (now - last) / 1e9f;
                last = now;
                if (System.currentTimeMillis() >= holdUntil) angle += dt * 6.2831853f / 7.0f;   // one turn every 7 s
                int w = Math.min(getWidth(), 700), h = Math.min(getHeight(), 1100);
                if (hasTex && w > 8 && h > 8 && isShown()) {
                    if (px == null || px.length != w * h) { px = new int[w * h]; bufs[0] = bufs[1] = null; }
                    if (N.pinPreviewRender(angle, w, h, px)) {
                        if (bufs[which] == null || bufs[which].getWidth() != w || bufs[which].getHeight() != h) bufs[which] = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
                        final Bitmap bmp = bufs[which];
                        bmp.setPixels(px, 0, w, 0, 0, w, h);
                        which ^= 1;                            // draw the next frame into the other bitmap
                        post(() -> setImageBitmap(bmp));
                    }
                }
                try { Thread.sleep(33); } catch (InterruptedException e) { return; }
            }
        }

        @Override public boolean onTouchEvent(MotionEvent e) {
            if (e.getAction() == MotionEvent.ACTION_DOWN) { lastX = e.getX(); getParent().requestDisallowInterceptTouchEvent(true); }
            else if (e.getAction() == MotionEvent.ACTION_MOVE) { angle += (e.getX() - lastX) * 0.012f; lastX = e.getX(); }
            holdUntil = System.currentTimeMillis() + 1500;
            return true;
        }
    }

    // ========================= the sheet =========================
    static boolean showing;
    static View holder;
    static LinearLayout list;
    static TextView status, previewName;
    static Button useButton;
    static Spin spin;
    static String selected = "";
    static File gamePin;                    // the game's current pin picture (made when the library opens), or null
    static final java.util.Map<String, Bitmap> thumbs = new java.util.HashMap<>();

    public static void show(Activity a) {
        act = a;
        if (showing) return;
        Context c = a;
        LinearLayout stack = UiKit.row(c, false);
        LinearLayout header = UiKit.row(c, true);
        header.addView(UiKit.label(c, "Pins", 22, Color.WHITE, true), UiKit.lpWeight(1));
        Button close = UiKit.button(c, "\u2715", false, false);
        close.setBackgroundColor(Color.TRANSPARENT);
        close.setTextColor(UiKit.dim(0.7f));
        close.setOnClickListener(v -> hide());
        header.addView(close);
        stack.addView(header);
        status = UiKit.label(c, "", 13, UiKit.YELLOW, true);
        stack.addView(status, Menu.mt(c, 4));

        FrameLayout stage = new FrameLayout(c);
        stage.setBackground(UiKit.round(UiKit.dim(0.05f), 16, c));
        TextView note = UiKit.label(c, "", 12, UiKit.dim(0.6f), true);
        spin = new Spin(c, note);
        FrameLayout.LayoutParams slp = new FrameLayout.LayoutParams(UiKit.dp(c, 150), UiKit.dp(c, 210));
        slp.gravity = Gravity.CENTER_HORIZONTAL | Gravity.TOP;
        slp.topMargin = UiKit.dp(c, 8);
        stage.addView(spin, slp);
        FrameLayout.LayoutParams nlp = new FrameLayout.LayoutParams(FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT);
        nlp.gravity = Gravity.CENTER_HORIZONTAL | Gravity.TOP;
        nlp.topMargin = UiKit.dp(c, 100);
        stage.addView(note, nlp);
        previewName = UiKit.label(c, "", 15, Color.WHITE, true);
        previewName.setGravity(Gravity.CENTER);
        FrameLayout.LayoutParams plp = new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.WRAP_CONTENT);
        plp.gravity = Gravity.BOTTOM;
        plp.bottomMargin = UiKit.dp(c, 10);
        stage.addView(previewName, plp);
        stack.addView(stage, new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, UiKit.dp(c, 250)));

        useButton = UiKit.button(c, "Use this pin", true, false);
        useButton.setOnClickListener(v -> {
            String msg = use(selected);
            UiKit.toast(act, msg);
            reload();
        });
        stack.addView(useButton, Menu.mt(c, 10));
        stack.addView(UiKit.label(c, "Tap a pin to see it turn above (drag it to turn it yourself), then Use this pin. Looks only: all lanes, every mode.", 12, UiKit.dim(0.55f), false), Menu.mt(c, 6));
        list = UiKit.row(c, false);
        stack.addView(list, Menu.mt(c, 4));
        LinearLayout adds = UiKit.row(c, true);
        Button photos = UiKit.button(c, "+ From Photos", false, true);
        Button files = UiKit.button(c, "+ From Files", false, true);
        Oil.Cb done = m -> { UiKit.toast(act, m); reload(); };
        photos.setOnClickListener(v -> Oil.pickPinImage(act, false, done));
        files.setOnClickListener(v -> Oil.pickPinImage(act, true, done));
        adds.addView(photos, UiKit.lpWeight(1)); adds.addView(Menu.space(c, 8)); adds.addView(files, UiKit.lpWeight(1));
        stack.addView(adds, Menu.mt(c, 10));
        Button guide = UiKit.button(c, "Get the wrap template + guide", false, true);
        guide.setOnClickListener(v -> Oil.sharePinGuide(act));
        stack.addView(guide, Menu.mt(c, 8));
        stack.addView(UiKit.label(c, "Best: draw on the wrap template (2:1, one sheet that wraps around the pin, so no seams). A square picture in the game's own layout works too.", 11, UiKit.dim(0.45f), false), Menu.mt(c, 6));

        showing = true;
        holder = UiKit.present(a, stack, Gravity.TOP, 430, Pins::closed);
        gamePin = null;
        select(activeId());
        new Thread(() -> {                     // the game's own pins as they are now (Gold, Rainbow...), drawn on Unity's thread
            File d = dir();
            if (d == null) return;
            d.mkdirs();
            File f = new File(d, "game_pins.png");
            f.delete();
            String r = N.call("gamePinPNG", f.getPath());
            final File got = r != null && !r.isEmpty() && f.exists() ? f : null;
            N.logLine("pins", "pin library: the game's own pin picture " + (got != null ? "read" : "couldn't be read, showing the plain pin"));
            BP.UI.post(() -> { gamePin = got; if (showing && selected.isEmpty() && got != null) select(""); });
        }).start();
        new Thread(() -> { for (JSONObject p : presets()) ensureFile(p.optString("id")); }).start();   // ready before you choose
    }

    static void closed() {
        showing = false;
        if (spin != null) spin.stop();
        spin = null;
    }

    static void hide() {
        if (!showing) return;
        FrameLayout root = UiKit.content(act);
        if (root != null && holder != null) root.removeView(holder);
        closed();
    }

    // Shows a pin in the preview (it isn't used until "Use this pin").
    static void select(String id) {
        selected = id == null ? "" : id;
        if (spin != null) {
            if (selected.isEmpty()) {
                if (gamePin != null) spin.show(gamePin, null); else spin.show(null, N.bytes("pinTemplate"));
                previewName.setText("The game's own pins");
            }
            else {
                JSONObject e = entry(selected);
                previewName.setText(e != null ? e.optString("name") : "");
                final String sel = selected;
                final Spin s = spin;
                File f = file(sel);
                if (f != null && f.exists()) s.show(f, null);
                else {
                    s.note.setText("Preparing\u2026");
                    new Thread(() -> { File g = ensureFile(sel); BP.UI.post(() -> { if (sel.equals(selected)) s.show(g, null); }); }).start();
                }
            }
        }
        reload();
    }

    static void reload() {
        if (list == null || act == null) return;
        Context c = act;
        list.removeAllViews();
        list.addView(row(null));
        list.addView(UiKit.label(c, "BOWLINGPLUS COLLECTION", 11, UiKit.dim(0.45f), true), Menu.mt(c, 6));
        for (JSONObject p : presets()) list.addView(row(p));
        list.addView(UiKit.label(c, "MY PINS", 11, UiKit.dim(0.45f), true), Menu.mt(c, 6));
        JSONArray mine = load();
        for (int i = 0; i < mine.length(); i++) list.addView(row(mine.optJSONObject(i)));
        if (mine.length() == 0) list.addView(UiKit.label(c, "None yet. Add a picture from Photos or Files below.", 12, UiKit.dim(0.45f), false), Menu.mt(c, 4));
        String act0 = activeId();
        JSONObject ae = entry(act0);
        if (status != null) status.setText(ae != null ? "On the pins: " + ae.optString("name") : "The game's own pins");
        if (useButton != null) {
            useButton.setVisibility(selected.equals(act0) ? View.GONE : View.VISIBLE);
            useButton.setText(selected.isEmpty() ? "Use the game's own pins" : "Use this pin");
        }
    }

    static Bitmap thumbFor(final JSONObject e, final ImageView into) {
        final String id = e.optString("id");
        synchronized (thumbs) { if (thumbs.containsKey(id)) return thumbs.get(id); }
        new Thread(() -> {
            byte[] png = e.optBoolean("preset") ? N.bytes("preset:" + e.optString("key")) : null;
            Bitmap src = null;
            try {
                BitmapFactory.Options o = new BitmapFactory.Options();
                o.inSampleSize = 8;                                  // a thumbnail is tiny
                if (png != null) src = BitmapFactory.decodeByteArray(png, 0, png.length, o);
                else {
                    File s = srcFile(id), f = file(id);
                    File use = s != null && s.exists() ? s : f;
                    if (use != null && use.exists()) src = BitmapFactory.decodeFile(use.getPath(), o);
                }
            } catch (Throwable ignored) {}
            if (src == null) return;
            int tw = UiKit.dp(act, 72), th = UiKit.dp(act, 36);
            Bitmap t = Bitmap.createBitmap(tw, th, Bitmap.Config.ARGB_8888);
            android.graphics.Canvas cv = new android.graphics.Canvas(t);
            cv.drawColor(Color.WHITE);
            float a = src.getWidth() / (float) Math.max(1, src.getHeight());
            float w = Math.min(tw, th * a), h = w / a;
            cv.drawBitmap(src, null, new android.graphics.RectF((tw - w) / 2, (th - h) / 2, (tw + w) / 2, (th + h) / 2), new android.graphics.Paint(android.graphics.Paint.FILTER_BITMAP_FLAG));
            synchronized (thumbs) { thumbs.put(id, t); }
            BP.UI.post(() -> { if (into.getParent() != null) into.setImageBitmap(t); });
        }).start();
        return null;
    }

    static View row(final JSONObject e) {
        Context c = act;
        final String id = e != null ? e.optString("id") : "";
        boolean active = id.equals(activeId());
        boolean sel = id.equals(selected);
        LinearLayout box = UiKit.row(c, true);
        box.setGravity(Gravity.CENTER_VERTICAL);
        box.setBackground(sel ? UiKit.roundStroke(Color.argb(40, 255, 204, 61), 14, UiKit.YELLOW, 1.5f, c) : UiKit.round(UiKit.dim(0.05f), 14, c));
        int pad = UiKit.dp(c, 8);
        box.setPadding(UiKit.dp(c, 12), pad, UiKit.dp(c, 6), pad);
        box.setMinimumHeight(UiKit.dp(c, 56));
        box.addView(UiKit.label(c, active ? "\u25C9" : "\u25CB", 20, active ? UiKit.YELLOW : UiKit.dim(0.4f), true));
        if (e != null) {
            ImageView thumb = new ImageView(c);
            thumb.setScaleType(ImageView.ScaleType.CENTER_CROP);
            thumb.setBackgroundColor(Color.WHITE);
            Bitmap t = thumbFor(e, thumb);
            if (t != null) thumb.setImageBitmap(t);
            LinearLayout.LayoutParams tlp = UiKit.lp(UiKit.dp(c, 72), UiKit.dp(c, 36));
            tlp.leftMargin = UiKit.dp(c, 10);
            box.addView(thumb, tlp);
        }
        LinearLayout texts = UiKit.row(c, false);
        texts.addView(UiKit.label(c, e != null ? e.optString("name") : "Off: the game's own pins", 15, Color.WHITE, true));
        String sub = e == null ? "No custom picture" : e.optBoolean("preset") ? e.optString("sub") : active ? "On the pins" : "Your picture";
        texts.addView(UiKit.label(c, sub, 11, UiKit.dim(0.5f), true));
        LinearLayout.LayoutParams txlp = UiKit.lpWeight(1);
        txlp.leftMargin = UiKit.dp(c, 10);
        box.addView(texts, txlp);
        if (e != null && !e.optBoolean("preset")) {
            Button more = UiKit.button(c, "\u22EF", false, false);
            more.setBackgroundColor(Color.TRANSPARENT);
            more.setTextColor(UiKit.YELLOW);
            more.setOnClickListener(v -> moreMenu(e));
            box.addView(more);
        }
        box.setOnClickListener(v -> select(id));
        box.setLayoutParams(Menu.mt(c, 8));
        return box;
    }

    static void moreMenu(final JSONObject e) {
        String[] titles = { "\u270E  Rename", "\u2B06\uFE0E  Share the picture", "\u2715  Delete" };
        new android.app.AlertDialog.Builder(act).setTitle(e.optString("name"))
                .setItems(titles, (d, w) -> { if (w == 0) rename(e); else if (w == 1) share(e); else confirmDelete(e); })
                .setNegativeButton("Cancel", null).show();
    }

    static void rename(final JSONObject e) {
        final EditText f = UiKit.field(act, "Name");
        f.setText(e.optString("name"));
        new android.app.AlertDialog.Builder(act).setTitle("Rename").setView(f)
                .setPositiveButton("Save", (d, w) -> {
                    String name = f.getText().toString().trim();
                    if (name.isEmpty()) return;
                    JSONArray lib = load();
                    for (int i = 0; i < lib.length(); i++) {
                        JSONObject x = lib.optJSONObject(i);
                        if (x.optString("id").equals(e.optString("id"))) try { x.put("name", name); } catch (Throwable ignored) {}
                    }
                    save(lib);
                    select(selected);
                })
                .setNegativeButton("Cancel", null).show();
    }

    static void share(final JSONObject e) {
        try {
            String id = e.optString("id");
            File s = srcFile(id);
            File from = s != null && s.exists() ? s : file(id);
            File dir = new File(act.getCacheDir(), "BowlingPlus");
            dir.mkdirs();
            File out = new File(dir, e.optString("name").replaceAll("[^A-Za-z0-9 ._-]", "_") + ".png");
            byte[] d = readAll(from);
            if (d == null || !writeAll(out, d)) { UiKit.toast(act, "Couldn't share the picture."); return; }
            android.net.Uri u = Oil.androidUri(out);
            android.content.Intent i = new android.content.Intent(android.content.Intent.ACTION_SEND);
            i.setType("image/png");
            i.putExtra(android.content.Intent.EXTRA_STREAM, u);
            i.addFlags(android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION);
            act.startActivity(android.content.Intent.createChooser(i, e.optString("name")));
        } catch (Throwable t) { UiKit.toast(act, "Couldn't share the picture."); }
    }

    static void confirmDelete(final JSONObject e) {
        new android.app.AlertDialog.Builder(act).setTitle("Delete \"" + e.optString("name") + "\"?")
                .setPositiveButton("Delete", (d, w) -> remove(e))
                .setNegativeButton("Cancel", null).show();
    }

    static void remove(JSONObject e) {
        String id = e.optString("id");
        JSONArray all = load(), out = new JSONArray();
        for (int i = 0; i < all.length(); i++) if (!all.optJSONObject(i).optString("id").equals(id)) out.put(all.optJSONObject(i));
        save(out);
        File f = file(id), s = srcFile(id);
        if (f != null) f.delete();
        if (s != null) s.delete();
        synchronized (thumbs) { thumbs.remove(id); }
        if (id.equals(lastId())) {
            setLastId(null);
            if (Config.b("pinImage", false)) use(null);
        }
        UiKit.toast(act, "Deleted \"" + e.optString("name") + "\"");
        select(selected.equals(id) ? activeId() : selected);
    }
}
