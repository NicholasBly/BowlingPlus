package com.bowlingplus;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.Switch;
import android.widget.TextView;

import java.io.File;

// Your own alley background (1.6.9; iOS: OilUI.mm "alley background"). The native side (Game.cpp "your own alley
// background") puts bg_image.png on the wall's three panels behind the lanes. The picture as picked is kept
// (bg_source.png) and fitted here, the way you choose, to the wall's 2820:850 shape (bg_image.png, 2048 x 617).
public final class Bg {
    private Bg() {}

    static final int W = 2048, H = 617;
    static final String[] FITS = { "Fill", "Fit", "Stretch", "Tile" };
    static Activity act;

    static File image() { String p = N.call("bgImagePath"); return p == null ? null : new File(p); }
    static File source() { File f = image(); return f == null ? null : new File(f.getParentFile(), "bg_source.png"); }
    static int fitMode() { int m = Oil.prefs().getInt("bgFit", 0); return m < 0 || m > 3 ? 0 : m; }

    // The same four ways as iOS (BgFitted): Fill covers the wall, Fit shows all of it on a soft, darker blur of
    // itself, Stretch squeezes it to the wall, Tile repeats it at full height from the middle outwards.
    static Bitmap fitted(Bitmap src, int mode) {
        Bitmap out = Bitmap.createBitmap(W, H, Bitmap.Config.ARGB_8888);
        Canvas c = new Canvas(out);
        c.drawColor(Color.BLACK);
        Paint p = new Paint(Paint.FILTER_BITMAP_FLAG | Paint.ANTI_ALIAS_FLAG);
        float sw = Math.max(1, src.getWidth()), sh = Math.max(1, src.getHeight());
        if (mode == 2) { c.drawBitmap(src, null, new RectF(0, 0, W, H), p); return out; }
        if (mode == 3) {
            float tw = sw * H / sh;
            float x = (W - tw) / 2;
            while (x > 0) x -= tw;
            for (; x < W; x += tw) c.drawBitmap(src, null, new RectF(x, 0, x + tw, H), p);
            return out;
        }
        if (mode == 1) {
            Bitmap tiny = Bitmap.createBitmap(40, 12, Bitmap.Config.ARGB_8888);
            Canvas tc = new Canvas(tiny);
            float s = Math.max(40 / sw, 12 / sh);
            tc.drawBitmap(src, null, new RectF((40 - sw * s) / 2, (12 - sh * s) / 2, (40 + sw * s) / 2, (12 + sh * s) / 2), p);
            c.drawBitmap(tiny, null, new RectF(0, 0, W, H), p);
            c.drawColor(Color.argb(89, 0, 0, 0));
            tiny.recycle();
        }
        float s = mode == 1 ? Math.min(W / sw, H / sh) : Math.max(W / sw, H / sh);
        c.drawBitmap(src, null, new RectF((W - sw * s) / 2, (H - sh * s) / 2, (W + sw * s) / 2, (H + sh * s) / 2), p);
        return out;
    }

    static String save(int mode) {           // bg_source.png -> bg_image.png
        File s = source(), f = image();
        Bitmap src = s != null && s.exists() ? BitmapFactory.decodeFile(s.getPath()) : null;
        if (src == null || f == null) return "Pick a picture first.";
        Bitmap out = fitted(src, mode);
        try {
            f.getParentFile().mkdirs();
            File tmp = new File(f.getPath() + ".tmp");
            try (java.io.FileOutputStream fos = new java.io.FileOutputStream(tmp)) { out.compress(Bitmap.CompressFormat.PNG, 100, fos); }
            if (!tmp.renameTo(f)) return "Couldn't save the picture.";
        } catch (Throwable t) { return "Couldn't save the picture."; }
        finally { out.recycle(); src.recycle(); }
        return "Alley background: " + FITS[mode];
    }

    static String status() {
        File f = image();
        boolean have = f != null && f.exists();
        if (!Config.b("bgImage", false) || !have) return have ? "Your picture is saved. Turn the switch on to use it." : "The room's own picture.";
        return "Your picture (" + FITS[fitMode()] + ")" + (Config.b("bgTitle", false) ? ", with the alley name" : "");
    }

    // ========================= the sheet =========================
    static View holder;
    static ImageView preview;
    static TextView statusLabel, empty;
    static Switch onSwitch, titleSwitch;
    static Button[] fitButtons = new Button[4];
    static boolean showing;

    public static void show(Activity a) {
        act = a;
        if (showing) return;
        Context c = a;
        LinearLayout stack = UiKit.row(c, false);
        LinearLayout header = UiKit.row(c, true);
        header.addView(UiKit.label(c, "Alley background", 22, Color.WHITE, true), UiKit.lpWeight(1));
        Button close = UiKit.button(c, "\u2715", false, false);
        close.setBackgroundColor(Color.TRANSPARENT);
        close.setTextColor(UiKit.dim(0.7f));
        close.setOnClickListener(v -> hide());
        header.addView(close);
        stack.addView(header);
        statusLabel = UiKit.label(c, "", 13, UiKit.YELLOW, true);
        stack.addView(statusLabel, Menu.mt(c, 4));

        FrameLayout frame = new FrameLayout(c) {
            @Override protected void onMeasure(int w, int h) {
                int width = View.MeasureSpec.getSize(w);
                super.onMeasure(w, View.MeasureSpec.makeMeasureSpec(width * 850 / 2820, View.MeasureSpec.EXACTLY));
            }
        };
        frame.setBackground(UiKit.round(UiKit.dim(0.06f), 12, c));
        frame.setClipToOutline(true);
        preview = new ImageView(c);
        preview.setScaleType(ImageView.ScaleType.CENTER_CROP);
        frame.addView(preview, new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        empty = UiKit.label(c, "The room's own picture", 13, UiKit.dim(0.5f), true);
        FrameLayout.LayoutParams elp = new FrameLayout.LayoutParams(FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT);
        elp.gravity = Gravity.CENTER;
        frame.addView(empty, elp);
        stack.addView(frame, Menu.mt(c, 10));

        onSwitch = UiKit.sw(c, Config.b("bgImage", false));
        onSwitch.setOnCheckedChangeListener((b, on) -> {
            if (on && (image() == null || !image().exists())) { onSwitch.setChecked(false); pick(false); return; }
            Config.set("bgImage", on);
            refresh();
        });
        stack.addView(Menu.rowView(c, "Use my picture", "Behind the lanes, on every lane. Looks only.", onSwitch), Menu.mt(c, 10));
        LinearLayout fits = UiKit.row(c, true);
        for (int i = 0; i < 4; i++) {
            final int mode = i;
            fitButtons[i] = UiKit.button(c, FITS[i], false, true);
            fitButtons[i].setOnClickListener(v -> {
                Oil.prefs().edit().putInt("bgFit", mode).apply();
                File s = source();
                if (s != null && s.exists()) new Thread(() -> { String m = save(mode); BP.UI.post(() -> { UiKit.toast(act, m); refresh(); }); }).start();
                refresh();
            });
            if (i > 0) fits.addView(Menu.space(c, 6));
            fits.addView(fitButtons[i], UiKit.lpWeight(1));
        }
        stack.addView(fits, Menu.mt(c, 10));
        stack.addView(UiKit.label(c, "Fill: covers the wall, cutting the edges off. Fit: all of it, on a soft blur of itself. Stretch: all of it, squeezed to the wall. Tile: repeated across.", 11, UiKit.dim(0.45f), false), Menu.mt(c, 6));
        titleSwitch = UiKit.sw(c, Config.b("bgTitle", false));
        titleSwitch.setOnCheckedChangeListener((b, on) -> { Config.set("bgTitle", on); refresh(); });
        stack.addView(Menu.rowView(c, "Show the alley name", "Keeps the room's name (Orange Tenpin Bowl...) on top of your picture.", titleSwitch), Menu.mt(c, 10));
        LinearLayout adds = UiKit.row(c, true);
        Button photos = UiKit.button(c, "+ From Photos", false, true);
        Button files = UiKit.button(c, "+ From Files", false, true);
        photos.setOnClickListener(v -> pick(false));
        files.setOnClickListener(v -> pick(true));
        adds.addView(photos, UiKit.lpWeight(1)); adds.addView(Menu.space(c, 8)); adds.addView(files, UiKit.lpWeight(1));
        stack.addView(adds, Menu.mt(c, 10));
        stack.addView(UiKit.label(c, "Best: a wide picture, about 3.3 : 1 (for example 2820 x 850).", 11, UiKit.dim(0.45f), false), Menu.mt(c, 6));
        showing = true;
        holder = UiKit.present(a, stack, Gravity.TOP, 430, () -> showing = false);
        refresh();
    }

    static void hide() {
        if (!showing) return;
        FrameLayout root = UiKit.content(act);
        if (root != null && holder != null) root.removeView(holder);
        showing = false;
    }

    static void refresh() {
        if (!showing) return;
        File f = image();
        Bitmap b = f != null && f.exists() ? BitmapFactory.decodeFile(f.getPath()) : null;
        preview.setImageBitmap(b);
        preview.setAlpha(Config.b("bgImage", false) ? 1f : 0.35f);
        empty.setVisibility(b == null ? View.VISIBLE : View.GONE);
        statusLabel.setText(status());
        int m = fitMode();
        for (int i = 0; i < 4; i++) {
            fitButtons[i].setTextColor(i == m ? Color.BLACK : Color.WHITE);
            fitButtons[i].setBackground(UiKit.round(i == m ? UiKit.YELLOW : UiKit.dim(0.08f), 10, act));
        }
    }

    static void picked(byte[] bytes) {
        new Thread(() -> {
            String msg;
            File s = source();
            if (bytes == null || s == null || BitmapFactory.decodeByteArray(bytes, 0, bytes.length) == null) msg = "That picture couldn't be read.";
            else if (!Pins.writeAll(s, bytes)) msg = "Couldn't save the picture.";
            else msg = save(fitMode());
            final String m = msg;
            BP.UI.post(() -> {
                if (m.startsWith("Alley background")) Config.set("bgImage", true);
                UiKit.toast(act, m);
                if (onSwitch != null) onSwitch.setChecked(Config.b("bgImage", false));
                refresh();
            });
        }).start();
    }

    static void pick(boolean fromFiles) {
        if (fromFiles) Pickers.pickFile(act, new String[]{ "image/*" }, (name, bytes) -> picked(bytes));
        else Pickers.pickImage(act, bmp -> { try { picked(bmp == null ? null : Oil.toPng(bmp)); } catch (Throwable t) { UiKit.toast(act, "Couldn't read that picture."); } });
    }
}
