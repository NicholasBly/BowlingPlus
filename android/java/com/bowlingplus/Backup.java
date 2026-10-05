package com.bowlingplus;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

// Backs up the game's own local data - PlayerPrefs, any local save file or database, and (if you've logged
// into Facebook in the game before) the Facebook SDK's own cached session - into one .zip you can save or
// share. A safety net for your own records if the game's servers or its Play Store listing ever disappear.
//
// This only reads the app's own private storage, the normal way any app reads its own files: nothing here
// touches another app, the Play Store copy, or anything on a server. It doesn't try to parse the save data
// (its format isn't reverse engineered), so this is a raw copy, meant to be kept, not read.
final class Backup {
    private Backup() {}

    private static final long MAX_TOTAL = 40L * 1024 * 1024;   // stay well under what a share sheet chokes on
    private static final long MAX_FILE = 4L * 1024 * 1024;
    private static final String[] SKIP_DIRS = { "cache", "code_cache", "no_backup", "lib", "unity_temp", "il2cpp" };

    static void run(Activity act) {
        UiKit.toast(act, "Backing up...");
        new Thread(() -> {
            File out;
            try { out = build(act); } catch (Throwable t) { BP.UI.post(() -> UiKit.toast(act, "Backup failed: " + t)); return; }
            BP.UI.post(() -> share(act, out));
        }, "BowlingPlus-backup").start();
    }

    private static File build(Context c) throws IOException {
        File dataDir = c.getFilesDir().getParentFile();   // .../files -> the app's own data dir
        File cacheSub = new File(c.getCacheDir(), "BowlingPlus");
        cacheSub.mkdirs();
        String stamp = new SimpleDateFormat("yyyy-MM-dd_HHmm", Locale.US).format(new Date());
        File zip = new File(cacheSub, "bowlingplus-backup-" + stamp + ".zip");
        long[] used = { 0 };
        try (ZipOutputStream z = new ZipOutputStream(new FileOutputStream(zip))) {
            writeManifest(c, z);
            // highest value first, in case the size cap is hit partway through: PlayerPrefs (where the
            // Facebook SDK keeps its cached session too) and any local database, before the rest of files/
            if (dataDir != null) {
                addDir(z, new File(dataDir, "shared_prefs"), "shared_prefs", used);
                addDir(z, new File(dataDir, "databases"), "databases", used);
                addDir(z, new File(dataDir, "files"), "files", used);
            }
        }
        return zip;
    }

    private static void writeManifest(Context c, ZipOutputStream z) throws IOException {
        String debug = "";
        try { debug = N.call("debug"); } catch (Throwable ignored) {}   // the game's own version + state, if it answers in time
        String json = "{\n"
                + "  \"package\": \"" + esc(c.getPackageName()) + "\",\n"
                + "  \"device\": \"" + esc(Build.MANUFACTURER + " " + Build.MODEL) + "\",\n"
                + "  \"androidSdk\": " + Build.VERSION.SDK_INT + ",\n"
                + "  \"backedUpAt\": \"" + new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ssXXX", Locale.US).format(new Date()) + "\",\n"
                + "  \"gameDebugInfo\": \"" + esc(debug == null ? "" : debug) + "\",\n"
                + "  \"note\": \"This is Bowling by Jason Belmonte's own local storage on this device: PlayerPrefs "
                + "(shared_prefs/*.xml), any local save files or databases, and a cached Facebook session if you were "
                + "logged in. Nothing from the game's servers is in here - only what's stored on this phone.\"\n"
                + "}\n";
        z.putNextEntry(new ZipEntry("backup-info.json"));
        z.write(json.getBytes("UTF-8"));
        z.closeEntry();
    }

    private static String esc(String s) { return s.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n"); }

    private static void addDir(ZipOutputStream z, File dir, String prefix, long[] used) throws IOException {
        if (!dir.isDirectory()) return;
        File[] kids = dir.listFiles();
        if (kids == null) return;
        for (File f : kids) {
            if (used[0] >= MAX_TOTAL) return;
            String name = f.getName();
            if (f.isDirectory()) {
                boolean skip = false;
                for (String s : SKIP_DIRS) if (s.equalsIgnoreCase(name)) { skip = true; break; }
                if (!skip) addDir(z, f, prefix + "/" + name, used);
                continue;
            }
            if (!f.isFile() || f.length() <= 0 || f.length() > MAX_FILE) continue;
            try (InputStream in = new FileInputStream(f)) {
                z.putNextEntry(new ZipEntry(prefix + "/" + name));
                byte[] buf = new byte[8192];
                int n;
                while ((n = in.read(buf)) > 0) { z.write(buf, 0, n); used[0] += n; }
                z.closeEntry();
            } catch (IOException ignored) {}   // a file the OS won't let us read right now: skip it, keep the rest
        }
    }

    private static void share(Activity act, File zip) {
        Uri uri;
        try { uri = BpFileProvider.uriFor(act, zip); } catch (Throwable t) { uri = null; }
        if (uri == null) { UiKit.toast(act, "Backup failed"); return; }
        Intent i = new Intent(Intent.ACTION_SEND);
        i.setType("application/zip");
        i.putExtra(Intent.EXTRA_STREAM, uri);
        i.putExtra(Intent.EXTRA_SUBJECT, zip.getName());
        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        act.startActivity(Intent.createChooser(i, "Save BowlingPlus backup"));
    }
}
