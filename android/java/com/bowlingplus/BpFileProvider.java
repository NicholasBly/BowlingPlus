package com.bowlingplus;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import android.webkit.MimeTypeMap;

import java.io.File;

// A tiny read-only content provider for sharing files from the app's cache (QR images, the pin wrap template).
// Android's androidx FileProvider needs an @xml/paths resource, which would mean editing resources.arsc; this
// avoids that. It only ever serves files directly inside getCacheDir()/BowlingPlus or the cache root, read-only.
// Authority: <pkg>.bowlingplus.fileprovider (declared by the manifest patch).
public final class BpFileProvider extends ContentProvider {
    static String authority(android.content.Context c) { return c.getPackageName() + ".bowlingplus.fileprovider"; }

    public static Uri uriFor(android.content.Context c, File f) {
        return new Uri.Builder().scheme("content").authority(authority(c)).encodedPath(f.getName()).build();
    }

    private File resolve(Uri uri) {
        String name = uri.getLastPathSegment();
        if (name == null || name.contains("..") || name.contains("/")) return null;
        File cache = getContext().getCacheDir();
        File inSub = new File(new File(cache, "BowlingPlus"), name);
        if (inSub.exists()) return inSub;
        File inRoot = new File(cache, name);
        return inRoot.exists() ? inRoot : null;
    }

    @Override public boolean onCreate() { return true; }

    @Override public ParcelFileDescriptor openFile(Uri uri, String mode) throws java.io.FileNotFoundException {
        File f = resolve(uri);
        if (f == null) throw new java.io.FileNotFoundException(String.valueOf(uri));
        return ParcelFileDescriptor.open(f, ParcelFileDescriptor.MODE_READ_ONLY);
    }

    @Override public Cursor query(Uri uri, String[] projection, String sel, String[] args, String sort) {
        File f = resolve(uri);
        if (f == null) return null;
        String[] cols = projection != null ? projection : new String[]{ OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE };
        MatrixCursor cur = new MatrixCursor(cols);
        Object[] row = new Object[cols.length];
        for (int i = 0; i < cols.length; i++) {
            if (OpenableColumns.DISPLAY_NAME.equals(cols[i])) row[i] = f.getName();
            else if (OpenableColumns.SIZE.equals(cols[i])) row[i] = f.length();
        }
        cur.addRow(row);
        return cur;
    }

    @Override public String getType(Uri uri) {
        String name = uri.getLastPathSegment();
        if (name != null) {
            String ext = MimeTypeMap.getFileExtensionFromUrl(name);
            String mime = ext != null ? MimeTypeMap.getSingleton().getMimeTypeFromExtension(ext.toLowerCase()) : null;
            if (mime != null) return mime;
        }
        return "application/octet-stream";
    }

    @Override public Uri insert(Uri uri, ContentValues v) { return null; }
    @Override public int delete(Uri uri, String sel, String[] args) { return 0; }
    @Override public int update(Uri uri, ContentValues v, String sel, String[] args) { return 0; }
}
