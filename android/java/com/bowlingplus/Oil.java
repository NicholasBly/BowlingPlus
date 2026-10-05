package com.bowlingplus;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.Deflater;
import java.util.zip.Inflater;

// Custom oil patterns (OilUI.mm). A pattern is forward/reverse Kegel load steps; the native side draws it
// through the Kegel-accurate engine (N.call "oilCompute"). Saved in SharedPreferences, shared as a QR code
// or "BJBOIL1:" code. Also the pin-image picker (SavePinImage) and the oil color picker.
//
// What differs from iOS: QR scanning uses the camera-import-from-photo path plus pasting a code (we don't ship
// a camera preview); Kegel PDF import uses Android 15's PdfRenderer text, else a best-effort fallback.
public final class Oil {
    static final String CODE_PREFIX = "BJBOIL1:";
    static final float THICK_MAX = 75;
    static Activity act;
    static FrameLayout overlay;
    static Runnable onReload;

    private Oil() {}

    // ========================= storage =========================
    static android.content.SharedPreferences prefs() { return (act != null ? act : BP.app).getSharedPreferences("BowlingPlus", Context.MODE_PRIVATE); }

    static JSONArray loadPatterns() {
        try { return new JSONArray(prefs().getString("oilPatterns", "[]")); } catch (Throwable t) { return new JSONArray(); }
    }
    static void savePatterns(JSONArray a) { prefs().edit().putString("oilPatterns", a.toString()).apply(); }
    static String activeId() { return prefs().getString("oilActive", ""); }
    static void setActiveId(String id) { prefs().edit().putString("oilActive", id == null ? "" : id).apply(); applyActive(); }
    static void applyActive() {
        JSONObject p = patternById(activeId());
        String arg = p == null ? "" : p.toString();
        new Thread(() -> { N.call("setCustom", arg); BP.poke(); }, "BowlingPlus-setCustom").start();
    }

    static JSONObject patternById(String id) {
        if (id == null || id.isEmpty()) return null;
        for (JSONObject p : collection()) if (id.equals(p.optString("id"))) return p;
        JSONArray mine = loadPatterns();
        for (int i = 0; i < mine.length(); i++) if (id.equals(mine.optJSONObject(i).optString("id"))) return mine.optJSONObject(i);
        return null;
    }

    static void upsert(JSONObject p) {
        JSONArray all = loadPatterns();
        for (int i = 0; i < all.length(); i++) if (all.optJSONObject(i).optString("id").equals(p.optString("id"))) { try { all.put(i, p); } catch (Throwable ignored) {} savePatterns(all); if (p.optString("id").equals(activeId())) applyActive(); return; }
        all.put(p);
        savePatterns(all);
        if (p.optString("id").equals(activeId())) applyActive();
    }

    static List<JSONObject> collection() {
        List<JSONObject> out = new ArrayList<>();
        try {
            // 2012 USBC Open (Baton Rouge) and 2026 PBA Regional 37, same data as OilCollection.mm
            out.add(new JSONObject("{\"id\":\"bp.batonrouge2012\",\"collection\":true,\"name\":\"Baton Rouge\",\"event\":\"2012 USBC Open Championships\",\"feet\":39,\"ml\":25.2,\"ul\":50,\"base\":0,\"drop\":34,\"exact\":true,\"precise\":true,"
                    + "\"fwd\":[[2,38,6,14,9.9],[5,35,1,18,12.4],[6,34,1,18,14.9],[7,33,1,18,17.4],[9,31,1,18,19.9],[10,30,1,18,22.4],[12,28,1,18,24.9],[13,27,1,18,27.4],[2,38,0,22,35.0],[2,38,0,26,39.0]],"
                    + "\"rev\":[[2,38,0,30,20.0],[13,27,1,22,16.9],[11,29,1,18,14.4],[8,32,1,18,11.9],[6,34,1,18,9.4],[5,35,1,18,6.9],[2,38,0,10,0.0]]}"));
            out.add(new JSONObject("{\"id\":\"bp.pbaregional37.2026\",\"collection\":true,\"name\":\"PBA Regional 37\",\"event\":\"2026 PBA Regional\",\"feet\":37,\"ml\":32.65,\"ul\":50,\"base\":0,\"drop\":30,\"exact\":true,\"precise\":true,"
                    + "\"fwd\":[[2,38,3,14,3.92],[2,34,1,14,5.88],[7,33,3,14,11.76],[4,31,2,14,15.68],[11,30,3,18,23.24],[13,28,2,18,28.28],[2,38,0,22,37.0]],"
                    + "\"rev\":[[2,38,0,30,29.0],[11,29,2,22,22.84],[9,32,2,18,17.8],[7,33,2,18,12.76],[6,34,1,14,10.8],[2,38,3,14,4.92],[2,38,0,14,0.0]]}"));
        } catch (Throwable ignored) {}
        return out;
    }

    // ========================= library =========================
    static boolean showing;
    static LinearLayout list;
    static TextView status;

    public static void showLibrary(Activity a) {
        act = a;
        if (showing) return;
        Context c = a;
        LinearLayout stack = UiKit.row(c, false);
        LinearLayout header = UiKit.row(c, true);
        header.addView(UiKit.label(c, "Custom oil", 22, Color.WHITE, true), UiKit.lpWeight(1));
        Button close = UiKit.button(c, "\u2715", false, false);
        close.setBackgroundColor(Color.TRANSPARENT);
        close.setTextColor(UiKit.dim(0.7f));
        close.setOnClickListener(v -> hide());
        header.addView(close);
        stack.addView(header);
        status = UiKit.label(c, "", 13, UiKit.YELLOW, true);
        stack.addView(status, mt(c, 4));
        stack.addView(UiKit.label(c, "Pick a pattern, then start practice with any pattern in the game's list: your custom oil replaces it on the lane. Practice only. Tap the \u22EF button on a pattern to edit, share, duplicate or delete it.", 12, UiKit.dim(0.55f), false), mt(c, 6));
        stack.addView(legend(c), mt(c, 8));
        list = UiKit.row(c, false);
        stack.addView(list, mt(c, 8));
        Button add = UiKit.button(c, "+ New pattern", true, false);
        add.setOnClickListener(v -> Editor.edit(act, null, s -> { setActiveId(s.optString("id")); reload(); }));
        stack.addView(add, mt(c, 8));
        LinearLayout imports = UiKit.row(c, true);
        Button fromPhoto = UiKit.button(c, "QR from photo", false, true);
        Button paste = UiKit.button(c, "Paste code", false, true);
        fromPhoto.setOnClickListener(v -> importQrFromPhoto());
        paste.setOnClickListener(v -> { android.content.ClipboardManager cm = (android.content.ClipboardManager) act.getSystemService(Context.CLIPBOARD_SERVICE); android.content.ClipData cd = cm.getPrimaryClip(); importCode(cd != null && cd.getItemCount() > 0 ? cd.getItemAt(0).coerceToText(act).toString() : ""); });
        imports.addView(fromPhoto, UiKit.lpWeight(1)); imports.addView(space(c, 8)); imports.addView(paste, UiKit.lpWeight(1));
        stack.addView(imports, mt(c, 8));
        Button kegel = UiKit.button(c, "\uD83D\uDCC2  Import a Kegel pattern file", false, false);
        kegel.setOnClickListener(v -> importKegel());
        stack.addView(kegel, mt(c, 8));
        stack.addView(UiKit.label(c, "From the Kegel Pattern Library app or website: pick the downloaded pattern sheet (.pdf) or .zip (or the .Pattern / .txt inside). Imports the steps, distances and drop brush.", 11, UiKit.dim(0.45f), false), mt(c, 6));
        reload();
        showing = true;
        overlayHolder = UiKit.present(a, stack, Gravity.TOP, 420, () -> showing = false);
    }
    static View overlayHolder;

    static void hide() {
        if (!showing) return;
        showing = false;
        FrameLayout root = UiKit.content(act);
        if (root != null && overlayHolder != null) root.removeView(overlayHolder);
    }

    // Every reload() bumps this; a preview that finishes loading for an older generation is discarded instead
    // of being applied to a (possibly reused/recycled) ImageView.
    static int reloadGen = 0;
    static final java.util.concurrent.ExecutorService previewExec = java.util.concurrent.Executors.newSingleThreadExecutor();

    static void reload() {
        if (list == null) return;
        int gen = ++reloadGen;
        list.removeAllViews();
        list.addView(row(null, gen));
        list.addView(UiKit.label(act, "BOWLINGPLUS COLLECTION", 11, UiKit.dim(0.45f), true), mt(act, 4));
        for (JSONObject p : collection()) list.addView(row(p, gen));
        list.addView(UiKit.label(act, "MY PATTERNS", 11, UiKit.dim(0.45f), true), mt(act, 4));
        JSONArray mine = loadPatterns();
        for (int i = 0; i < mine.length(); i++) list.addView(row(mine.optJSONObject(i), gen));
        if (mine.length() == 0) list.addView(UiKit.label(act, "None yet. Tap + New pattern, or import one with a QR code.", 12, UiKit.dim(0.45f), false));
        JSONObject actv = patternById(activeId());
        if (status != null) status.setText(actv != null ? "On the lane in practice: " + actv.optString("name") : "Using the game's patterns");
    }

    static View row(final JSONObject p, final int gen) {
        Context c = act;
        boolean active = p != null ? p.optString("id").equals(activeId()) : (activeId().isEmpty() || patternById(activeId()) == null);
        LinearLayout box = UiKit.row(c, true);
        box.setBackground(active ? UiKit.roundStroke(Color.argb(40, 255, 204, 61), 14, UiKit.YELLOW, 1.5f, c) : UiKit.round(UiKit.dim(0.05f), 14, c));
        int pad = UiKit.dp(c, 10);
        box.setPadding(UiKit.dp(c, 12), pad, UiKit.dp(c, 6), pad);
        box.addView(UiKit.label(c, active ? "\u25C9" : "\u25CB", 20, active ? UiKit.YELLOW : UiKit.dim(0.4f), true));
        if (p != null) {
            int tw = UiKit.dp(c, 96), th = UiKit.dp(c, 30);
            ImageView thumb = new ImageView(c);
            thumb.setLayoutParams(UiKit.lp(tw, th));
            thumb.setBackgroundColor(Color.rgb(219, 179, 128));   // shown until the preview below finishes loading
            LinearLayout.LayoutParams tlp = UiKit.lp(tw, th);
            tlp.leftMargin = UiKit.dp(c, 10);
            box.addView(thumb, tlp);
            // previewFor() round-trips to the game (N.call("oilCompute"), up to 4 s), and the library can
            // show 50+ rows at once, so every preview loads on a background thread: building the list, and
            // scrolling it, never waits on the game.
            previewExec.execute(() -> {
                Bitmap bmp = previewFor(p, tw, th);
                if (bmp == null) return;
                BP.UI.post(() -> { if (gen == reloadGen && thumb.getParent() != null) thumb.setImageBitmap(bmp); });
            });
        }
        int loads = loadsOf(p);
        LinearLayout texts = UiKit.row(c, false);
        texts.addView(UiKit.label(c, p != null ? p.optString("name") : "Off: the game's pattern", 15, Color.WHITE, true));
        String sub = p == null ? "No custom oil"
                : p.optBoolean("collection") ? p.optString("event") + " \u00B7 " + p.optString("feet") + " ft \u00B7 " + p.optString("ml") + " mL"
                : p.optJSONArray("fwd").length() + " forward \u00B7 " + p.optJSONArray("rev").length() + " reverse \u00B7 " + loads + " loads";
        texts.addView(UiKit.label(c, sub, 11, UiKit.dim(0.5f), true));
        LinearLayout.LayoutParams txlp = UiKit.lpWeight(1);
        txlp.leftMargin = UiKit.dp(c, 10);
        box.addView(texts, txlp);
        if (p != null) {
            Button more = UiKit.button(c, "\u22EF", false, false);
            more.setBackgroundColor(Color.TRANSPARENT);
            more.setTextColor(UiKit.YELLOW);
            more.setOnClickListener(v -> moreMenu(p));
            box.addView(more);
        }
        box.setOnClickListener(v -> { setActiveId(p != null ? p.optString("id") : null); reload(); });
        LinearLayout.LayoutParams blp = mt(c, 8);
        box.setLayoutParams(blp);
        return box;
    }

    static int loadsOf(JSONObject p) {
        if (p == null) return 0;
        int loads = 0;
        for (String k : new String[]{ "fwd", "rev" }) {
            JSONArray a = p.optJSONArray(k);
            for (int i = 0; a != null && i < a.length(); i++) loads += a.optJSONArray(i).optInt(2);
        }
        return loads;
    }

    static void moreMenu(final JSONObject p) {
        boolean builtIn = p.optBoolean("collection");
        List<String> titles = new ArrayList<>();
        List<Runnable> acts = new ArrayList<>();
        if (!builtIn) { titles.add("\u270E  Edit"); acts.add(() -> Editor.edit(act, p, s -> reload())); }
        titles.add("\u2398  Copy code"); acts.add(() -> { copyToClip(encode(p)); UiKit.toast(act, "Code copied"); });
        titles.add("\u2B06\uFE0E  Share / QR"); acts.add(() -> share(p));
        titles.add(builtIn ? "\u29C9  Make an editable copy" : "\u29C9  Duplicate"); acts.add(() -> {
            JSONObject copy = clone(p);
            try { copy.put("id", java.util.UUID.randomUUID().toString()); copy.put("name", builtIn ? p.optString("name") + " (my copy)" : p.optString("name") + " copy"); copy.remove("collection"); } catch (Throwable ignored) {}
            upsert(copy); reload();
            if (builtIn) Editor.edit(act, copy, s -> reload());
        });
        if (!builtIn) { titles.add("\u2715  Delete"); acts.add(() -> remove(p)); }
        final List<Runnable> fActs = acts;
        new android.app.AlertDialog.Builder(act).setTitle(p.optString("name"))
                .setItems(titles.toArray(new String[0]), (d, w) -> fActs.get(w).run())
                .setNegativeButton("Cancel", null).show();
    }

    static void remove(JSONObject p) {
        JSONArray all = loadPatterns(), out = new JSONArray();
        for (int i = 0; i < all.length(); i++) if (!all.optJSONObject(i).optString("id").equals(p.optString("id"))) out.put(all.optJSONObject(i));
        savePatterns(out);
        if (p.optString("id").equals(activeId())) setActiveId(null);
        UiKit.toast(act, "Deleted \"" + p.optString("name") + "\"");
        reload();
    }

    // ========================= preview =========================
    static Bitmap previewFor(JSONObject p, int w, int h) {
        String r = N.call("oilCompute", computeArg(p).toString());
        if (r == null || r.equals("null")) return null;
        try { return renderPreview(new JSONObject(r), w, h); } catch (Throwable t) { return null; }
    }

    static JSONObject computeArg(JSONObject p) {
        JSONObject a = new JSONObject();
        try {
            a.put("base", p.optInt("base"));
            a.put("fwd", p.optJSONArray("fwd")); a.put("rev", p.optJSONArray("rev"));
            a.put("drop", p.optInt("drop")); a.put("exact", p.optBoolean("exact"));
            a.put("feet", p.optInt("feet")); a.put("precise", p.optBoolean("precise"));
        } catch (Throwable ignored) {}
        return a;
    }

    // float grid tagged {"$f32":"base64"} by the native side
    static float[] floats(JSONObject r, String key) {
        JSONObject f = r.optJSONObject(key);
        if (f == null) return null;
        byte[] raw = android.util.Base64.decode(f.optString("$f32"), android.util.Base64.DEFAULT);
        float[] out = new float[raw.length / 4];
        java.nio.ByteBuffer.wrap(raw).order(java.nio.ByteOrder.LITTLE_ENDIAN).asFloatBuffer().get(out);
        return out;
    }

    static void thicknessRGB(float v, float[] out) {
        float[][] stops = { { 0f, 0.80f, 0.97f, 1.00f }, { 0.25f, 0.35f, 0.85f, 0.95f }, { 0.50f, 0.20f, 0.55f, 0.90f }, { 0.75f, 0.12f, 0.25f, 0.70f }, { 1.00f, 0.07f, 0.10f, 0.40f } };
        float t = Math.min(Math.max(v / THICK_MAX, 0), 1);
        for (int i = 1; i < 5; i++) if (t <= stops[i][0]) { float f = (t - stops[i - 1][0]) / (stops[i][0] - stops[i - 1][0]); for (int k = 0; k < 3; k++) out[k] = stops[i - 1][k + 1] + (stops[i][k + 1] - stops[i - 1][k + 1]) * f; return; }
        for (int k = 0; k < 3; k++) out[k] = stops[4][k + 1];
    }

    // Lane sideways: foul line left, pins right, bowler's left on top (RenderPreview).
    static Bitmap renderPreview(JSONObject r, int outW, int outH) {
        int w = r.optInt("w"), h = r.optInt("h");
        float[] g = floats(r, "grid");
        if (w <= 0 || h <= 0 || g == null || g.length < w * h) return null;
        int cols = Math.min(h, 480), rows = w;
        int[] px = new int[cols * rows];
        float[] c = new float[3];
        float wr = 0.86f, wg = 0.70f, wb = 0.50f;
        for (int y = 0; y < rows; y++) for (int x = 0; x < cols; x++) {
            int hy = (int) ((long) x * h / cols);
            float v = g[y * h + hy];
            thicknessRGB(v, c);
            float a = v > 0.01f ? 0.9f : 0;
            int rr = (int) (255 * (wr * (1 - a) + c[0] * a)), gg = (int) (255 * (wg * (1 - a) + c[1] * a)), bb = (int) (255 * (wb * (1 - a) + c[2] * a));
            px[(rows - 1 - y) * cols + x] = Color.rgb(rr, gg, bb);   // top edge = bowler's left
        }
        Bitmap small = Bitmap.createBitmap(px, cols, rows, Bitmap.Config.ARGB_8888);
        Bitmap out = Bitmap.createScaledBitmap(small, outW, outH, true);
        android.graphics.Canvas cv = new android.graphics.Canvas(out);
        android.graphics.Paint pt = new android.graphics.Paint();
        pt.setColor(Color.argb(46, 0, 0, 0));
        float ft = outW / 60f;
        cv.drawRect(15 * ft, 0, 15 * ft + 1, outH, pt);
        cv.drawRect(0, 0, 2, outH, pt);
        return out;
    }

    static View legend(Context c) {
        int w = UiKit.dp(c, 160), h = UiKit.dp(c, 10);
        Bitmap bar = Bitmap.createBitmap(w, 1, Bitmap.Config.ARGB_8888);
        float[] col = new float[3];
        for (int x = 0; x < w; x++) { thicknessRGB((x + 1f) / w * THICK_MAX, col); bar.setPixel(x, 0, Color.rgb((int) (col[0] * 255), (int) (col[1] * 255), (int) (col[2] * 255))); }
        ImageView iv = new ImageView(c);
        iv.setImageBitmap(Bitmap.createScaledBitmap(bar, w, h, false));
        iv.setLayoutParams(UiKit.lp(w, h));
        LinearLayout row = UiKit.row(c, true);
        row.setGravity(Gravity.CENTER);
        row.addView(UiKit.label(c, "Thin oil", 11, UiKit.dim(0.5f), true));
        row.addView(iv, Menu.mlH(c, 8));
        row.addView(UiKit.label(c, "Thick oil", 11, UiKit.dim(0.5f), true), Menu.mlH(c, 8));
        return row;
    }

    // ========================= share / import codes =========================
    static String encode(JSONObject p) {
        try {
            JSONObject j = new JSONObject();
            j.put("v", 1); j.put("n", p.optString("name", "Custom")); j.put("b", p.optInt("base"));
            j.put("f", p.optJSONArray("fwd")); j.put("r", p.optJSONArray("rev")); j.put("d", p.optInt("drop"));
            j.put("x", p.optBoolean("exact")); j.put("p", p.optBoolean("precise")); j.put("t", p.optInt("feet"));
            byte[] json = j.toString().getBytes("UTF-8");
            Deflater d = new Deflater(); d.setInput(json); d.finish();
            ByteArrayOutputStream bos = new ByteArrayOutputStream();
            byte[] buf = new byte[1024]; while (!d.finished()) bos.write(buf, 0, d.deflate(buf));
            String b = android.util.Base64.encodeToString(bos.toByteArray(), android.util.Base64.NO_WRAP | android.util.Base64.URL_SAFE | android.util.Base64.NO_PADDING);
            return CODE_PREFIX + b;
        } catch (Throwable t) { return CODE_PREFIX; }
    }

    static JSONObject decode(String code) {
        try {
            int at = code.indexOf(CODE_PREFIX);
            if (at < 0) return null;
            String b = code.substring(at + CODE_PREFIX.length()).trim().split("\\s")[0];
            byte[] z = android.util.Base64.decode(b, android.util.Base64.URL_SAFE);
            Inflater inf = new Inflater(); inf.setInput(z);
            ByteArrayOutputStream bos = new ByteArrayOutputStream();
            byte[] buf = new byte[1024]; while (!inf.finished()) { int n = inf.inflate(buf); if (n == 0 && inf.needsInput()) break; bos.write(buf, 0, n); }
            JSONObject j = new JSONObject(new String(bos.toByteArray(), "UTF-8"));
            JSONObject p = new JSONObject();
            p.put("id", java.util.UUID.randomUUID().toString());
            p.put("name", j.optString("n", "Imported"));
            p.put("base", j.optInt("b")); p.put("fwd", cleanSteps(j.optJSONArray("f"))); p.put("rev", cleanSteps(j.optJSONArray("r")));
            p.put("drop", j.optInt("d")); p.put("exact", j.optBoolean("x")); p.put("precise", j.optBoolean("p")); p.put("feet", j.optInt("t"));
            if (p.optJSONArray("fwd").length() == 0 && p.optJSONArray("rev").length() == 0) return null;
            return p;
        } catch (Throwable t) { return null; }
    }

    static void importCode(String code) {
        JSONObject p = decode(code);
        if (p == null) { UiKit.toast(act, "That isn't a BowlingPlus oil pattern code"); return; }
        upsert(p);
        UiKit.toast(act, "Imported \"" + p.optString("name") + "\"");
        reload();
    }

    static void share(final JSONObject p) {
        String code = encode(p);
        Bitmap qr = Qr.encode(code, UiKit.dp(act, 260));
        Context c = act;
        LinearLayout stack = UiKit.row(c, false);
        stack.addView(UiKit.label(c, p.optString("name"), 20, Color.WHITE, true));
        ImageView iv = new ImageView(c);
        if (qr != null) iv.setImageBitmap(qr);
        iv.setBackgroundColor(Color.WHITE);
        iv.setLayoutParams(UiKit.lp(ViewGroup.LayoutParams.MATCH_PARENT, UiKit.dp(c, 260)));
        iv.setScaleType(ImageView.ScaleType.FIT_CENTER);
        stack.addView(iv, mt(c, 8));
        stack.addView(UiKit.label(c, "Friends with BowlingPlus: shake \u2192 Custom oil \u2192 QR from photo (with a screenshot), or Paste code.", 12, UiKit.dim(0.55f), false), mt(c, 8));
        LinearLayout btns = UiKit.row(c, true);
        Button shareB = UiKit.button(c, "Share", true, false), copyB = UiKit.button(c, "Copy code", false, false), done = UiKit.button(c, "Done", false, false);
        shareB.setOnClickListener(v -> shareCode(code, qr));
        copyB.setOnClickListener(v -> { copyToClip(code); UiKit.toast(act, "Code copied"); });
        final FrameLayout[] holder = new FrameLayout[1];
        done.setOnClickListener(v -> { FrameLayout root = UiKit.content(act); if (root != null) root.removeView(holder[0]); });
        btns.addView(shareB, UiKit.lpWeight(1)); btns.addView(space(c, 8)); btns.addView(copyB, UiKit.lpWeight(1)); btns.addView(space(c, 8)); btns.addView(done, UiKit.lpWeight(1));
        stack.addView(btns, mt(c, 8));
        holder[0] = UiKit.present(act, stack, Gravity.CENTER, 360, null);
    }

    static void shareCode(String code, Bitmap qr) {
        try {
            Intent i = new Intent(Intent.ACTION_SEND);
            i.setType("text/plain");
            i.putExtra(Intent.EXTRA_TEXT, code);
            if (qr != null) {
                java.io.File f = new java.io.File(act.getCacheDir(), "oil_qr.png");
                try (java.io.FileOutputStream fos = new java.io.FileOutputStream(f)) { qr.compress(Bitmap.CompressFormat.PNG, 100, fos); }
                Uri u = androidUri(f);
                if (u != null) { i.putExtra(Intent.EXTRA_STREAM, u); i.setType("image/png"); i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION); }
            }
            act.startActivity(Intent.createChooser(i, "Share oil pattern"));
        } catch (Throwable t) { copyToClip(code); UiKit.toast(act, "Code copied"); }
    }

    static Uri androidUri(java.io.File f) {
        try { return BpFileProvider.uriFor(act, f); } catch (Throwable t) { return null; }
    }

    // QR from a chosen photo (ZXing), the Android stand-in for the live camera scanner
    static Pickers.Cb qrCb;
    static void importQrFromPhoto() {
        Pickers.pickImage(act, bmp -> {
            String code = bmp != null ? Qr.decode(bmp) : null;
            if (code != null && code.startsWith(CODE_PREFIX)) importCode(code);
            else UiKit.toast(act, "No oil pattern QR code in that picture");
        });
    }

    // ========================= Kegel file import =========================
    static void importKegel() {
        Pickers.pickFile(act, new String[]{ "*/*" }, (name, bytes) -> {
            UiKit.toast(act, "Reading " + name + "\u2026");
            // Parsing a PDF (PdfRenderer, or several regex passes over its raw text for older Android) can take
            // real time; do it off the main thread so the library stays scrollable while it works.
            new Thread(() -> {
                JSONObject p = kegelImport(bytes, name);
                BP.UI.post(() -> {
                    if (p == null) { UiKit.toast(act, "That file isn't a Kegel pattern (.pdf, .zip, .Pattern or .txt from the Kegel Pattern Library)"); return; }
                    upsert(p);
                    setActiveId(p.optString("id"));
                    UiKit.toast(act, "Imported \"" + p.optString("name") + "\" from Kegel");
                    reload();
                });
            }, "BowlingPlus-kegel-import").start();
        });
    }

    static JSONObject kegelImport(byte[] data, String fileName) {
        if (data == null || data.length < 4) return null;
        JSONObject k = null;
        try {
            if (data[0] == 'P' && data[1] == 'K') {
                byte[] pat = Kegel.zipFile(data, ".pattern");
                if (pat != null) k = Kegel.fromJson(pat);
                if (k == null) { byte[] t = Kegel.zipFile(data, ".txt"); if (t != null) k = kegelFromText(new String(t, "UTF-8")); }
                if (k == null) { byte[] pdf = Kegel.zipFile(data, ".pdf"); if (pdf != null) k = Kegel.fromPdf(pdf, fileName); }
            } else if (data[0] == '%' && data[1] == 'P' && data[2] == 'D' && data[3] == 'F') {
                k = Kegel.fromPdf(data, fileName);
            } else {
                k = Kegel.fromJson(data);
                if (k == null) k = kegelFromText(new String(data, "UTF-8"));
            }
        } catch (Throwable ignored) {}
        if (k == null) return null;
        try {
            JSONObject p = new JSONObject(k.toString());
            p.put("id", java.util.UUID.randomUUID().toString());
            p.put("fwd", cleanSteps(k.optJSONArray("fwd")));
            p.put("rev", cleanSteps(k.optJSONArray("rev")));
            p.put("base", 0); p.put("exact", true);
            p.put("precise", k.optBoolean("precise", false));
            String name = k.optString("name", "").trim();
            p.put("name", !name.isEmpty() ? name.substring(0, Math.min(40, name.length())) : stripExt(fileName));
            return p;
        } catch (Throwable t) { return null; }
    }

    // .txt uses the shared native parser (same as the game's 48 patterns) so the two builds agree
    static JSONObject kegelFromText(String text) {
        String r = N.call("kegelText", text == null ? "" : text);
        try { JSONObject j = r == null ? null : new JSONObject(r); return j != null && j.length() > 0 ? j : null; } catch (Throwable t) { return null; }
    }

    static JSONArray cleanSteps(JSONArray raw) {
        JSONArray out = new JSONArray();
        if (raw == null) return out;
        for (int i = 0; i < raw.length() && out.length() < 40; i++) {
            JSONArray s = raw.optJSONArray(i);
            if (s == null || s.length() < 4) continue;
            int a = clamp(s.optInt(0, 2), 1, 39), b = clamp(s.optInt(1, 38), 1, 39);
            float ft = s.length() > 4 ? (float) s.optDouble(4, 0) : 0;
            ft = Math.round(Math.min(Math.max(ft, 0), 70) * 100) / 100f;
            int ul = s.length() > 5 ? clamp(s.optInt(5, 50), 5, 150) : 50;
            JSONArray step = new JSONArray();
            step.put(Math.min(a, b)); step.put(Math.max(a, b)); step.put(clamp(s.optInt(2, 2), 0, 99)); step.put(clamp(s.optInt(3, 14), 6, 30));
            step.put(Double.valueOf(ft));   // put(double) throws JSONException; put(Object) does not
            step.put(ul);
            out.put(step);
        }
        return out;
    }

    // ========================= oil color picker =========================
    public static void showColorPicker(Activity a) {
        act = a;
        Context c = a;
        LinearLayout stack = UiKit.row(c, false);
        stack.addView(UiKit.label(c, "Oil color", 20, Color.WHITE, true));
        stack.addView(UiKit.label(c, "The color the lane shows oil in. Thicker oil is a bit darker, like in the game.", 12, UiKit.dim(0.55f), false), mt(c, 4));
        final TextView preview = UiKit.label(c, "", 13, Color.rgb(38, 38, 38), true);
        preview.setGravity(Gravity.CENTER);
        preview.setLayoutParams(UiKit.lp(ViewGroup.LayoutParams.MATCH_PARENT, UiKit.dp(c, 54)));
        stack.addView(preview, mt(c, 8));
        final android.widget.SeekBar slider = new android.widget.SeekBar(c);
        slider.setMax(999);
        stack.addView(slider, mt(c, 8));
        final Runnable show = () -> {
            double hue = Config.d("oilHue", -1);
            boolean game = hue < 0;
            preview.setBackground(UiKit.round(game ? Color.rgb(219, 179, 128) : oilOnWood((float) hue), 12, c));
            preview.setText(game ? "The game's own color" : "Oil on the lane");
        };
        slider.setOnSeekBarChangeListener(Menu.sliderListener(() -> { Config.set("oilHue", slider.getProgress() / 1000.0); N.call("applyHue"); BP.poke(); show.run(); }));
        LinearLayout dots = UiKit.row(c, true);
        dots.setGravity(Gravity.CENTER);
        double[] hues = { 0.0, 0.08, 0.13, 0.33, 0.47, 0.58, 0.75, 0.9 };
        for (double h : hues) {
            final double hue = h;
            View b = new View(c);
            b.setBackground(UiKit.round(Color.HSVToColor(new float[]{ (float) (h * 360), 0.85f, 0.95f }), 15, c));
            LinearLayout.LayoutParams dlp = UiKit.lp(UiKit.dp(c, 30), UiKit.dp(c, 30));
            dlp.leftMargin = dlp.rightMargin = UiKit.dp(c, 4);
            b.setOnClickListener(v -> { Config.set("oilHue", hue); N.call("applyHue"); BP.poke(); slider.setProgress((int) (hue * 1000)); show.run(); });
            dots.addView(b, dlp);
        }
        stack.addView(dots, mt(c, 10));
        LinearLayout btns = UiKit.row(c, true);
        Button reset = UiKit.button(c, "Use the game's color", false, false), done = UiKit.button(c, "Done", true, false);
        final FrameLayout[] holder = new FrameLayout[1];
        reset.setOnClickListener(v -> { Config.set("oilHue", -1.0); N.call("applyHue"); BP.poke(); show.run(); });
        done.setOnClickListener(v -> { FrameLayout root = UiKit.content(act); if (root != null) root.removeView(holder[0]); });
        btns.addView(reset, UiKit.lpWeight(1)); btns.addView(space(c, 8)); btns.addView(done, UiKit.lpWeight(1));
        stack.addView(btns, mt(c, 8));
        if (Config.d("oilHue", -1) < 0) slider.setProgress(400); else slider.setProgress((int) (Config.d("oilHue", 0) * 1000));
        show.run();
        holder[0] = UiKit.present(a, stack, Gravity.CENTER, 360, null);
    }

    static int oilOnWood(float hue) {
        float[] c = new float[3];
        Color.colorToHSV(Color.HSVToColor(new float[]{ hue * 360, 1, 1 }), new float[3]);
        int rgb = Color.HSVToColor(new float[]{ hue * 360, 1, 1 });
        float[] wood = { 0.86f, 0.70f, 0.50f };
        float[] rr = { Color.red(rgb) / 255f, Color.green(rgb) / 255f, Color.blue(rgb) / 255f };
        for (int i = 0; i < 3; i++) c[i] = wood[i] * (1 + 0.45f * (rr[i] - 1));
        return Color.rgb((int) (c[0] * 255), (int) (c[1] * 255), (int) (c[2] * 255));
    }

    // ========================= pin image =========================
    public static void pickPinImage(Activity a, boolean fromFiles, final Cb done) {
        act = a;
        if (fromFiles) Pickers.pickFile(a, new String[]{ "image/*" }, (name, bytes) -> savePin(bytes, done));
        else Pickers.pickImage(a, bmp -> { try { savePin(bmp == null ? null : toPng(bmp), done); } catch (Throwable t) { done.run("Couldn't read that picture."); } });
    }

    public interface Cb { void run(String message); }

    static void savePin(byte[] imageBytes, Cb done) {
        new Thread(() -> {
            String msg = savePinImage(imageBytes, true);
            BP.UI.post(() -> { if (msg != null) done.run(msg); });
        }).start();
    }

    // SavePinImage: wrap (~2:1) -> native wrap conversion + fill; square/other -> stretch + fill
    static String savePinImage(byte[] imageBytes, boolean turnOn) {
        if (imageBytes == null) return "That picture couldn't be read.";
        Bitmap img = android.graphics.BitmapFactory.decodeByteArray(imageBytes, 0, imageBytes.length);
        if (img == null || img.getWidth() < 8 || img.getHeight() < 8) return "That picture couldn't be read.";
        int w = img.getWidth(), h = img.getHeight();
        boolean wrap = w >= h * 1.5, square = Math.abs(w - h) < 2;
        int side;
        byte[] out;
        if (wrap) {
            int ww = (int) Math.min(4096, Math.max(512, w)), wh = (int) Math.min(2048, Math.max(256, (long) h * ww / w));
            side = ww >= 3000 ? 2048 : 1024;
            byte[] src = rgbaOnWhite(img, ww, wh);
            byte[] tmpl = rgbaOnWhite(android.graphics.BitmapFactory.decodeByteArray(N.bytes("pinTemplate"), 0, N.bytes("pinTemplate").length), side, side);
            out = N.pinWrap(src, ww, wh, tmpl, side);
        } else {
            side = (int) Math.min(2048, Math.max(512, Math.max(w, h)));
            out = rgbaOnWhite(img, side, side);
            if (out != null) N.pinFill(out, side, true);
        }
        if (out == null) return "Couldn't read that picture.";
        byte[] png = BP.encodePng(out, side, side);
        String path = N.call("pinImagePath");
        try {
            new java.io.File(path).getParentFile().mkdirs();
            try (java.io.FileOutputStream fos = new java.io.FileOutputStream(path)) { fos.write(png); }
            if (turnOn) {
                try (java.io.FileOutputStream fos = new java.io.FileOutputStream(new java.io.File(new java.io.File(path).getParent(), "pin_image_source.png"))) { fos.write(imageBytes); }
                Config.set("pinImage", true);
            }
        } catch (Throwable t) { return "Couldn't save the picture."; }
        if (wrap) return "Wrap picture put on the pins (" + side + " px). Seams always match in this layout.";
        return square ? "Pin picture saved (" + side + " px). It's on the pins now."
                : "Pin picture saved. It wasn't square or 2:1, so it was stretched to " + side + " x " + side + " (game layout).";
    }

    // draw the image onto white at w x h, return RGBA rows top-down
    static byte[] rgbaOnWhite(Bitmap img, int w, int h) {
        Bitmap bmp = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
        android.graphics.Canvas cv = new android.graphics.Canvas(bmp);
        cv.drawColor(Color.WHITE);
        if (img != null) cv.drawBitmap(img, new android.graphics.Rect(0, 0, img.getWidth(), img.getHeight()), new android.graphics.Rect(0, 0, w, h), new android.graphics.Paint(android.graphics.Paint.FILTER_BITMAP_FLAG));
        int[] px = new int[w * h];
        bmp.getPixels(px, 0, w, 0, 0, w, h);
        byte[] out = new byte[w * h * 4];
        for (int i = 0; i < w * h; i++) { int c = px[i], o = i * 4; out[o] = (byte) Color.red(c); out[o + 1] = (byte) Color.green(c); out[o + 2] = (byte) Color.blue(c); out[o + 3] = (byte) Color.alpha(c); }
        bmp.recycle();
        return out;
    }

    static byte[] toPng(Bitmap bmp) { ByteArrayOutputStream bos = new ByteArrayOutputStream(); bmp.compress(Bitmap.CompressFormat.PNG, 100, bos); return bos.toByteArray(); }

    public static void sharePinGuide(Activity a) {
        act = a;
        try {
            java.io.File dir = new java.io.File(a.getCacheDir(), "BowlingPlus");
            dir.mkdirs();
            java.io.File wguide = writeAsset(dir, "BowlingPlus pin wrap guide.png", "wrapGuide");
            java.io.File wtmpl = writeAsset(dir, "BowlingPlus pin wrap template 2048x1024.png", "wrapTemplate");
            ArrayList<Uri> uris = new ArrayList<>();
            for (java.io.File f : new java.io.File[]{ wguide, wtmpl }) { Uri u = androidUri(f); if (u != null) uris.add(u); }
            Intent i = new Intent(Intent.ACTION_SEND_MULTIPLE);
            i.setType("image/png");
            i.putParcelableArrayListExtra(Intent.EXTRA_STREAM, uris);
            i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            a.startActivity(Intent.createChooser(i, "Pin wrap template + guide"));
        } catch (Throwable t) { UiKit.toast(a, "Couldn't share the template."); }
    }

    static java.io.File writeAsset(java.io.File dir, String name, String key) throws Exception {
        java.io.File f = new java.io.File(dir, name);
        try (java.io.FileOutputStream fos = new java.io.FileOutputStream(f)) { fos.write(N.bytes(key)); }
        return f;
    }

    static void start() { applyActive(); }   // the pattern you had on last time

    // ---- small helpers ----
    static LinearLayout.LayoutParams mt(Context c, int topDp) { return Menu.mt(c, topDp); }
    static View space(Context c, int w) { return Menu.space(c, w); }
    static int clamp(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
    static JSONObject clone(JSONObject p) { try { return new JSONObject(p.toString()); } catch (Throwable t) { return new JSONObject(); } }
    static void copyToClip(String s) { ((android.content.ClipboardManager) act.getSystemService(Context.CLIPBOARD_SERVICE)).setPrimaryClip(android.content.ClipData.newPlainText("BowlingPlus", s)); }
    static String stripExt(String f) { if (f == null) return "Kegel pattern"; int d = f.lastIndexOf('.'); return (d > 0 ? f.substring(0, d) : f).replace('_', ' '); }
}
