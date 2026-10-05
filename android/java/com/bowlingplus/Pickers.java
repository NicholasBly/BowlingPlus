package com.bowlingplus;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Bundle;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;

// Photo and file pickers. We have no Activity of our own in the manifest (we don't touch it), so we launch
// the system picker from the game's Activity and catch the result by temporarily wrapping its
// onActivityResult through a one-shot callback, using startActivityForResult with a reserved request code.
public final class Pickers {
    public interface ImageCb { void run(Bitmap bmp); }
    public interface FileCb { void run(String name, byte[] bytes); }
    public interface Cb { void run(Bitmap b); }

    static final int REQ_IMAGE = 0x6B01;
    static final int REQ_FILE = 0x6B02;
    static ImageCb imageCb;
    static FileCb fileCb;

    private Pickers() {}

    static void pickImage(Activity act, ImageCb cb) {
        imageCb = cb;
        Intent i = new Intent(Intent.ACTION_GET_CONTENT);
        i.setType("image/*");
        i.addCategory(Intent.CATEGORY_OPENABLE);
        launch(act, Intent.createChooser(i, "Pick a picture"), REQ_IMAGE);
    }

    static void pickFile(Activity act, String[] mimes, FileCb cb) {
        fileCb = cb;
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType(mimes.length == 1 ? mimes[0] : "*/*");
        if (mimes.length > 1) i.putExtra(Intent.EXTRA_MIME_TYPES, mimes);
        launch(act, i, REQ_FILE);
    }

    // A tiny transparent relay Activity (declared in the merged manifest) forwards the result here.
    static void launch(Activity act, Intent intent, int req) {
        try {
            Intent relay = new Intent(act, Relay.class);
            relay.putExtra("req", req);
            relay.putExtra("target", intent);
            act.startActivity(relay);
        } catch (Throwable t) {
            // relay missing (manifest not merged): fall back to the game's Activity, which may crash its
            // own onActivityResult handling, so only used if Relay isn't present
            try { act.startActivityForResult(intent, req); } catch (Throwable ignored) {}
        }
    }

    static void deliver(int req, int result, Intent data) {
        Uri uri = result == Activity.RESULT_OK && data != null ? data.getData() : null;
        if (req == REQ_IMAGE) {
            ImageCb cb = imageCb; imageCb = null;
            if (cb == null) return;
            Bitmap bmp = uri != null ? loadBitmap(uri) : null;
            BP.UI.post(() -> cb.run(bmp));
        } else if (req == REQ_FILE) {
            FileCb cb = fileCb; fileCb = null;
            if (cb == null) return;
            byte[] bytes = uri != null ? readBytes(uri) : null;
            String name = uri != null ? nameOf(uri) : null;
            BP.UI.post(() -> cb.run(name, bytes));
        }
    }

    static Bitmap loadBitmap(Uri uri) {
        try (InputStream is = BP.app.getContentResolver().openInputStream(uri)) {
            byte[] raw = readAll(is);
            BitmapFactory.Options o = new BitmapFactory.Options();
            o.inJustDecodeBounds = true;
            BitmapFactory.decodeByteArray(raw, 0, raw.length, o);
            int max = 4096, sample = 1;
            while (o.outWidth / sample > max || o.outHeight / sample > max) sample *= 2;
            o.inJustDecodeBounds = false;
            o.inSampleSize = sample;
            return BitmapFactory.decodeByteArray(raw, 0, raw.length, o);
        } catch (Throwable t) { return null; }
    }

    static byte[] readBytes(Uri uri) {
        try (InputStream is = BP.app.getContentResolver().openInputStream(uri)) { return readAll(is); } catch (Throwable t) { return null; }
    }

    static byte[] readAll(InputStream is) throws Exception {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        byte[] buf = new byte[65536];
        int n;
        while ((n = is.read(buf)) > 0) bos.write(buf, 0, n);
        return bos.toByteArray();
    }

    static String nameOf(Uri uri) {
        try (android.database.Cursor cu = BP.app.getContentResolver().query(uri, null, null, null, null)) {
            if (cu != null && cu.moveToFirst()) {
                int i = cu.getColumnIndex(android.provider.OpenableColumns.DISPLAY_NAME);
                if (i >= 0) return cu.getString(i);
            }
        } catch (Throwable ignored) {}
        String p = uri.getLastPathSegment();
        return p != null ? p.substring(p.lastIndexOf('/') + 1) : "file";
    }

    // Transparent one-shot Activity that runs the real picker and relays its result.
    public static final class Relay extends Activity {
        int req;
        @Override protected void onCreate(Bundle b) {
            super.onCreate(b);
            overridePendingTransition(0, 0);
            if (b != null) { finish(); return; }
            req = getIntent().getIntExtra("req", 0);
            Intent target = getIntent().getParcelableExtra("target");
            try { startActivityForResult(target, req); } catch (Throwable t) { finish(); }
        }
        @Override protected void onActivityResult(int rc, int result, Intent data) {
            try { Pickers.deliver(rc, result, data); } catch (Throwable ignored) {}
            finish();
            overridePendingTransition(0, 0);
        }
    }
}
