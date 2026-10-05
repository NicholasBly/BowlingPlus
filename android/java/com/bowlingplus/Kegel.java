package com.bowlingplus;

import android.os.Build;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.zip.Inflater;

// Kegel pattern files (OilUI.mm). .Pattern is Kegel's JSON (exact distances); PDF is the data sheet; .txt
// goes through the shared native parser (Oil.kegelFromText). The .txt path is identical to the game's own
// 48 patterns, so the two builds agree.
final class Kegel {
    private Kegel() {}

    // .Pattern JSON: Name, Distance, ReverseDropBrushDistance, Forward/ReverseLoadscreens
    static JSONObject fromJson(byte[] data) {
        try {
            int off = (data.length >= 3 && (data[0] & 0xff) == 0xEF) ? 3 : 0;   // strip BOM
            JSONObject j = new JSONObject(new String(data, off, data.length - off, "UTF-8"));
            if (!j.has("ForwardLoadscreens")) return null;
            JSONArray[] fr = { new JSONArray(), new JSONArray() };
            int ul = 50;
            String[] keys = { "ForwardLoadscreens", "ReverseLoadscreens" };
            for (int d = 0; d < 2; d++) {
                JSONArray src = j.optJSONArray(keys[d]);
                for (int i = 0; src != null && i < src.length(); i++) {
                    JSONObject s = src.optJSONObject(i);
                    if (s == null || s.optInt("SpeedIps") <= 0 || s.optInt("Stop") <= 0) continue;
                    if (s.optInt("Microliter") > 0) ul = s.optInt("Microliter");
                    JSONArray step = new JSONArray();
                    step.put(s.optInt("Start")); step.put(s.optInt("Stop")); step.put(s.optInt("Loads"));
                    step.put(s.optInt("SpeedIps")); step.put(s.optDouble("EndDistance")); step.put(s.optInt("Microliter", 50));
                    fr[d].put(step);
                }
            }
            if (fr[0].length() == 0) return null;
            JSONObject p = new JSONObject();
            p.put("name", j.optString("Name", "Kegel pattern"));
            p.put("feet", j.optInt("Distance")); p.put("drop", j.optInt("ReverseDropBrushDistance"));
            p.put("ul", ul); p.put("fwd", fr[0]); p.put("rev", fr[1]); p.put("precise", true);
            return p;
        } catch (Throwable t) { return null; }
    }

    // PDF data sheet: one row per step "# START STOP LOADS MICS SPEED BUFF TANK from -> to T.OIL".
    static JSONObject fromPdf(byte[] data, String fileName) {
        String text = pdfText(data);
        if (text == null || text.isEmpty()) return null;
        String title = Oil.stripExt(fileName);
        return fromPdfText(text, title);
    }

    static JSONObject fromPdfText(String text, String title) {
        try {
            Pattern row = Pattern.compile(
                    "(?<![\\d.,])(\\d{1,2})\\s+(\\d{1,2}\\s?[LR]|20)\\s+(\\d{1,2}\\s?[LR]|20)\\s+(\\d{1,2})\\s+(\\d{1,3})\\s+(\\d{1,2})\\s+(\\d{1,4})"
                    + "\\s+(?:[A-Za-z][^\\d]{0,24}?)?\\s*(\\d{1,2}(?:\\.\\d+)?)\\s*(?:\u2192|->|>|\u2013|-|to)?\\s*(\\d{1,2}(?:\\.\\d+)?)\\s+[\\d,]+",
                    Pattern.CASE_INSENSITIVE);
            Pattern feet = Pattern.compile("(\\d{1,2})\\s*FEET", Pattern.CASE_INSENSITIVE);
            Matcher revM = Pattern.compile("REVERSE\\s+LOADS", Pattern.CASE_INSENSITIVE).matcher(text);
            int revAt = revM.find() ? revM.start() : -1;
            JSONArray[] fr = { new JSONArray(), new JSONArray() };
            int ul = 0;
            int[] lastNum = { 0, 0 };
            Matcher m = row.matcher(text);
            while (m.find()) {
                int num = Integer.parseInt(m.group(1));
                int d = revAt >= 0 ? (m.start() > revAt ? 1 : 0) : ((lastNum[1] > 0 || (num == 1 && lastNum[0] > 0)) ? 1 : 0);
                if (num != lastNum[d] + 1) continue;
                lastNum[d] = num;
                if (ul == 0) ul = Integer.parseInt(m.group(5));
                JSONArray step = new JSONArray();
                step.put(board(m.group(2))); step.put(board(m.group(3))); step.put(Integer.parseInt(m.group(4)));
                step.put(Integer.parseInt(m.group(6))); step.put(Double.parseDouble(m.group(9))); step.put(Integer.parseInt(m.group(5)));
                fr[d].put(step);
            }
            if (fr[0].length() == 0) return null;
            Matcher fm = feet.matcher(text);
            int distance = 0, drop = 0, count = 0;
            while (fm.find()) { int v = Integer.parseInt(fm.group(1)); if (count == 0) distance = v; else if (count == 1) drop = v; count++; }
            Matcher dl = Pattern.compile("DROP\\s*BRUSH:", Pattern.CASE_INSENSITIVE).matcher(text);
            if (dl.find()) {
                Matcher dm = feet.matcher(text.substring(dl.end(), Math.min(text.length(), dl.end() + 24)));
                if (dm.find()) drop = Integer.parseInt(dm.group(1));
            }
            JSONObject p = new JSONObject();
            p.put("name", title == null || title.isEmpty() ? "Kegel pattern" : title);
            p.put("feet", distance); p.put("drop", drop); p.put("ul", ul == 0 ? 50 : ul); p.put("fwd", fr[0]); p.put("rev", fr[1]);
            return p;
        } catch (Throwable t) { return null; }
    }

    static int board(String t) {
        t = t.replace(" ", "").toUpperCase();
        int n = 0;
        try { n = Integer.parseInt(t.replaceAll("[^0-9]", "")); } catch (Throwable ignored) {}
        return t.endsWith("R") ? 40 - n : n;
    }

    // Android 15+ (API 35) has PdfDocument text; older versions fall back to raw stream text, which works on
    // some uncompressed-text PDFs and fails cleanly otherwise (the Kegel .Pattern / .txt import still works).
    static String pdfText(byte[] data) {
        if (Build.VERSION.SDK_INT >= 35) {
            try {
                Class<?> loader = Class.forName("android.graphics.pdf.PdfDocument");   // placeholder: real text API below
            } catch (Throwable ignored) {}
            String viaRenderer = pdfTextViaRenderer(data);
            if (viaRenderer != null && !viaRenderer.isEmpty()) return viaRenderer;
        }
        return pdfTextRaw(data);
    }

    // API 35 PdfRenderer adds getPage().getTextContents(); reached by reflection so older builds compile.
    static String pdfTextViaRenderer(byte[] data) {
        try {
            java.io.File tmp = java.io.File.createTempFile("kegel", ".pdf", BP.app.getCacheDir());
            try (java.io.FileOutputStream fos = new java.io.FileOutputStream(tmp)) { fos.write(data); }
            android.os.ParcelFileDescriptor pfd = android.os.ParcelFileDescriptor.open(tmp, android.os.ParcelFileDescriptor.MODE_READ_ONLY);
            android.graphics.pdf.PdfRenderer r = new android.graphics.pdf.PdfRenderer(pfd);
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < r.getPageCount(); i++) {
                android.graphics.pdf.PdfRenderer.Page page = r.openPage(i);
                try {
                    java.lang.reflect.Method getText = page.getClass().getMethod("getTextContents");
                    java.util.List<?> contents = (java.util.List<?>) getText.invoke(page);
                    for (Object pc : contents) {
                        java.lang.reflect.Method getText2 = pc.getClass().getMethod("getText");
                        sb.append(getText2.invoke(pc)).append('\n');
                    }
                } catch (Throwable ignored) {} finally { page.close(); }
            }
            r.close(); pfd.close(); tmp.delete();
            return sb.toString();
        } catch (Throwable t) { return null; }
    }

    // Last resort: pull "(...)Tj" and "[...]TJ" text strings out of uncompressed PDF content streams.
    static String pdfTextRaw(byte[] data) {
        try {
            String s = new String(data, "ISO-8859-1");
            StringBuilder out = new StringBuilder();
            Matcher m = Pattern.compile("\\(((?:[^()\\\\]|\\\\.)*)\\)\\s*Tj", Pattern.DOTALL).matcher(s);
            while (m.find()) out.append(unescape(m.group(1))).append(' ');
            Matcher m2 = Pattern.compile("\\[((?:[^\\]])*)\\]\\s*TJ", Pattern.DOTALL).matcher(s);
            while (m2.find()) {
                Matcher inner = Pattern.compile("\\(((?:[^()\\\\]|\\\\.)*)\\)").matcher(m2.group(1));
                while (inner.find()) out.append(unescape(inner.group(1)));
                out.append(' ');
            }
            return out.toString();
        } catch (Throwable t) { return null; }
    }

    static String unescape(String s) { return s.replace("\\(", "(").replace("\\)", ")").replace("\\\\", "\\").replace("\\n", "\n").replace("\\r", " ").replace("\\t", " "); }

    // First entry in a zip whose name ends with ext (stored or deflated), skipping __MACOSX copies.
    static byte[] zipFile(byte[] zip, String ext) {
        try {
            int n = zip.length;
            if (n < 22) return null;
            int eocd = -1;
            for (int o = n - 22; o >= 0 && n - o < 70000; o--) if (u32(zip, o) == 0x06054b50) { eocd = o; break; }
            if (eocd < 0) return null;
            int count = u16(zip, eocd + 10), cd = (int) u32(zip, eocd + 16);
            for (int i = 0; i < count && cd + 46 <= n; i++) {
                if (u32(zip, cd) != 0x02014b50) break;
                int method = u16(zip, cd + 10), csize = (int) u32(zip, cd + 20), usize = (int) u32(zip, cd + 24);
                int nl = u16(zip, cd + 28), xl = u16(zip, cd + 30), cl = u16(zip, cd + 32), lho = (int) u32(zip, cd + 42);
                String name = cd + 46 + nl <= n ? new String(zip, cd + 46, nl, "UTF-8") : null;
                cd += 46 + nl + xl + cl;
                if (name == null || name.startsWith("__MACOSX") || !name.toLowerCase().endsWith(ext)) continue;
                if (lho + 30 > n || u32(zip, lho) != 0x04034b50) continue;
                int data = lho + 30 + u16(zip, lho + 26) + u16(zip, lho + 28);
                if (data + csize > n) continue;
                if (method == 0) { byte[] out = new byte[csize]; System.arraycopy(zip, data, out, 0, csize); return out; }
                if (method == 8) {
                    Inflater inf = new Inflater(true);
                    inf.setInput(zip, data, csize);
                    byte[] out = new byte[usize > 0 ? usize : csize * 8 + 1024];
                    int got = inf.inflate(out);
                    inf.end();
                    if (usize > 0 && got == usize) return out;
                    byte[] trimmed = new byte[got];
                    System.arraycopy(out, 0, trimmed, 0, got);
                    return trimmed;
                }
            }
        } catch (Throwable ignored) {}
        return null;
    }

    static int u16(byte[] b, int o) { return (b[o] & 0xff) | (b[o + 1] & 0xff) << 8; }
    static long u32(byte[] b, int o) { return (b[o] & 0xffL) | (b[o + 1] & 0xffL) << 8 | (b[o + 2] & 0xffL) << 16 | (b[o + 3] & 0xffL) << 24; }
}
