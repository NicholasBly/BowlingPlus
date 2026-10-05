package com.bowlingplus;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.graphics.Color;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

// The oil pattern editor (OilUI.mm BFOilEditor): name, machine settings, forward / reverse steps with big
// steppers, a live thickness preview, and "Start from" a real pattern. The preview and distances come from
// the native Kegel engine (N.call "oilCompute").
final class Editor {
    interface Done { void run(JSONObject saved); }

    Activity act;
    Context c;
    JSONObject pattern;
    List<JSONArray> fwd = new ArrayList<>(), rev = new ArrayList<>();
    Done onDone;
    View holder;
    EditText nameField;
    ImageView preview;
    TextView stats, baseButton;
    Stepper dropStepper;
    LinearLayout fwdStack, revStack;
    List<TextView> fwdEnds = new ArrayList<>(), revEnds = new ArrayList<>();
    android.os.Handler debounce = new android.os.Handler(android.os.Looper.getMainLooper());

    // Everything that asks the game (oilCompute / oilBuiltins can each wait up to 4 s for Unity) runs on this
    // one worker thread, never on the UI thread. One thread keeps the requests in order; the generation
    // counters drop answers that a newer edit has already made stale.
    static final java.util.concurrent.ExecutorService WORK = java.util.concurrent.Executors.newSingleThreadExecutor(r -> {
        Thread t = new Thread(r, "BowlingPlus-editor"); t.setDaemon(true); return t;
    });
    int previewGen, startGen;
    List<JSONObject> builtinsCache;   // null until loaded

    static void edit(Activity a, JSONObject p, Done done) {
        Editor e = new Editor();
        e.act = a; e.c = a; e.onDone = done;
        e.pattern = p != null ? Oil.clone(p) : new JSONObject();
        try { if (!e.pattern.has("id")) e.pattern.put("id", java.util.UUID.randomUUID().toString()); } catch (Throwable ignored) {}
        for (JSONArray s : toList(Oil.cleanSteps(p != null ? p.optJSONArray("fwd") : null))) e.fwd.add(s);
        for (JSONArray s : toList(Oil.cleanSteps(p != null ? p.optJSONArray("rev") : null))) e.rev.add(s);
        e.build();
        e.loadBuiltins(null);
        if (p == null) e.startFrom(0, true);   // fills in the steps when the game answers
    }

    static List<JSONArray> toList(JSONArray a) { List<JSONArray> o = new ArrayList<>(); for (int i = 0; i < a.length(); i++) o.add(a.optJSONArray(i)); return o; }

    void startFrom(int index, boolean announce) {
        put("base", index);
        final String arg = Oil.computeArg(jWith("base", index)).toString();
        final int gen = ++startGen;
        if (stats != null) stats.setText("Loading the pattern\u2026");
        WORK.execute(() -> {
            String cr = N.call("oilCompute", arg);
            BP.UI.post(() -> { if (gen == startGen) applyStart(cr, announce); });
        });
    }

    void applyStart(String cr, boolean announce) {
        JSONObject r = null;
        try { if (cr != null && !cr.equals("null")) r = new JSONObject(cr); } catch (Throwable ignored) {}
        put("exact", r != null); put("precise", r != null);
        if (r != null && r.optInt("feet") > 0) put("feet", r.optInt("feet")); else pattern.remove("feet");
        int tdrop = r != null ? r.optInt("tdrop") : 0;
        if (tdrop > 0) put("drop", tdrop);
        if (dropStepper != null) dropStepper.setValue(pattern.optInt("drop"));
        fwd.clear(); rev.clear();
        if (r != null) {
            for (JSONArray s : toList(Oil.cleanSteps(r.optJSONArray("fwd")))) fwd.add(s);
            for (JSONArray s : toList(Oil.cleanSteps(r.optJSONArray("rev")))) rev.add(s);
        }
        if (fwd.isEmpty()) {   // game not loaded: a simple house-style start
            fwd.add(mk(2, 38, 2, 14, 0)); fwd.add(mk(4, 36, 3, 16, 0)); fwd.add(mk(7, 33, 4, 18, 0)); fwd.add(mk(10, 30, 3, 20, 0)); fwd.add(mk(2, 38, 0, 26, 40));
            rev.add(mk(8, 32, 3, 20, 0)); rev.add(mk(5, 35, 2, 18, 0));
        }
        if (announce) { rebuildSteps(); schedulePreview(); }
    }

    JSONObject jWith(String k, int v) { JSONObject j = Oil.clone(pattern); try { j.put(k, v); j.put("fwd", new JSONArray()); j.put("rev", new JSONArray()); } catch (Throwable ignored) {} return j; }

    void build() {
        LinearLayout stack = UiKit.row(c, false);

        LinearLayout head = UiKit.row(c, true);
        Button cancel = UiKit.button(c, "Cancel", false, false), save = UiKit.button(c, "Save", true, false);
        cancel.setOnClickListener(v -> cancel());
        save.setOnClickListener(v -> save());
        head.addView(cancel);
        TextView title = UiKit.label(c, "Oil pattern", 18, Color.WHITE, true);
        title.setGravity(Gravity.CENTER);
        head.addView(title, UiKit.lpWeight(1));
        head.addView(save);
        stack.addView(head);

        nameField = UiKit.field(c, "My pattern");
        nameField.setText(pattern.optString("name", "My pattern"));
        nameField.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);
        stack.addView(nameField, Menu.mt(c, 8));

        baseButton = UiKit.label(c, "", 14, Color.WHITE, true);
        baseButton.setBackground(UiKit.round(UiKit.dim(0.12f), 10, c));
        baseButton.setPadding(UiKit.dp(c, 14), UiKit.dp(c, 12), UiKit.dp(c, 14), UiKit.dp(c, 12));
        baseButton.setOnClickListener(v -> pickStart());
        refreshBaseMenu();
        stack.addView(baseButton, Menu.mt(c, 8));

        dropStepper = new Stepper(c, "REVERSE BRUSH DROP", 0, 60, v -> v > 0 ? v + " ft" : "from start pattern");
        dropStepper.setValue(pattern.optInt("drop"));
        dropStepper.changed = v -> { put("drop", v); schedulePreview(); };
        stack.addView(dropStepper, Menu.mt(c, 8));

        preview = new ImageView(c);
        preview.setBackgroundColor(Color.rgb(219, 179, 128));
        preview.setLayoutParams(UiKit.lp(ViewGroup.LayoutParams.MATCH_PARENT, UiKit.dp(c, 96)));
        preview.setScaleType(ImageView.ScaleType.FIT_XY);
        stack.addView(preview, Menu.mt(c, 8));
        LinearLayout axis = UiKit.row(c, true);
        axis.addView(UiKit.label(c, "Foul line", 10, UiKit.dim(0.45f), true), UiKit.lpWeight(1));
        axis.addView(UiKit.label(c, "Arrows", 10, UiKit.dim(0.45f), true));
        axis.addView(UiKit.label(c, "Pins \u2192", 10, UiKit.dim(0.45f), true), UiKit.lpWeight(1));
        ((TextView) axis.getChildAt(2)).setGravity(Gravity.END);
        stack.addView(axis, Menu.mt(c, 4));
        stack.addView(Oil.legend(c), Menu.mt(c, 6));
        stats = UiKit.label(c, "", 13, UiKit.YELLOW, true);
        stack.addView(stats, Menu.mt(c, 6));

        fwdStack = UiKit.row(c, false);
        revStack = UiKit.row(c, false);
        stack.addView(tab("Forward oil"), Menu.mt(c, 8));
        stack.addView(fwdStack, Menu.mt(c, 8));
        Button addF = UiKit.button(c, "+ Add forward step", false, false);
        addF.setOnClickListener(v -> addStep(true));
        stack.addView(addF, Menu.mt(c, 8));
        stack.addView(tab("Reverse oil"), Menu.mt(c, 8));
        stack.addView(revStack, Menu.mt(c, 8));
        Button addR = UiKit.button(c, "+ Add reverse step", false, false);
        addR.setOnClickListener(v -> addStep(false));
        stack.addView(addR, Menu.mt(c, 8));
        stack.addView(UiKit.label(c, "Boards: 2L is the 2nd board from the left, 2R the 2nd from the right. Like a real Kegel machine, each step's distance comes from loads \u00D7 speed (0 loads = travel without oil), and colors show how thick the oil is.", 12, UiKit.dim(0.5f), false), Menu.mt(c, 8));

        rebuildSteps();
        holder = UiKit.present(act, stack, Gravity.CENTER, 400, this::cancelCleanup);
        schedulePreview();
    }

    View tab(String text) {
        TextView l = UiKit.label(c, text.toUpperCase(), 13, Color.rgb(25, 25, 25), true);
        l.setGravity(Gravity.CENTER);
        l.setBackground(UiKit.round(UiKit.YELLOW, 9, c));
        l.setLayoutParams(UiKit.lp(ViewGroup.LayoutParams.MATCH_PARENT, UiKit.dp(c, 30)));
        l.setGravity(Gravity.CENTER);
        return l;
    }

    void refreshBaseMenu() {
        String from = pattern.optString("from", "");
        if (from.isEmpty() && builtinsCache != null) {
            int base = pattern.optInt("base");
            for (JSONObject b : builtinsCache) if (b.optInt("index") == base) from = b.optString("name");
        }
        baseButton.setText("Start from: " + (from.isEmpty() ? "a game pattern" : from) + "  \u25BE");
    }

    // Ask the game for its patterns on the worker thread; then run `then` on the UI thread.
    void loadBuiltins(final Runnable then) {
        WORK.execute(() -> {
            String r = N.call("oilBuiltins");
            List<JSONObject> out = new ArrayList<>();
            try { if (r != null) { JSONArray a = new JSONArray(r); for (int i = 0; i < a.length(); i++) out.add(a.optJSONObject(i)); } } catch (Throwable ignored) {}
            BP.UI.post(() -> {
                if (r != null) builtinsCache = out;   // no answer (game busy): ask again next time
                if (baseButton != null) refreshBaseMenu();
                if (then != null) then.run();
            });
        });
    }

    List<JSONObject> builtins() { return builtinsCache != null ? builtinsCache : new ArrayList<>(); }

    void pickStart() {
        if (builtinsCache == null) { loadBuiltins(this::showStartMenu); return; }
        showStartMenu();
    }

    void showStartMenu() {
        List<String> titles = new ArrayList<>();
        List<Runnable> acts = new ArrayList<>();
        for (JSONObject cc : Oil.collection()) { titles.add("\u2605 " + cc.optString("name") + " \u00B7 " + cc.optString("feet") + " ft"); acts.add(() -> startFromPattern(cc)); }
        for (JSONObject b : builtins()) { final int idx = b.optInt("index"); final String name = b.optString("name"); titles.add(name + " \u00B7 " + b.optString("feet") + " ft"); acts.add(() -> { startFrom(idx, true); put("from", name); refreshBaseMenu(); }); }
        if (titles.isEmpty()) { UiKit.toast(act, "Open a game first so the patterns are loaded"); return; }
        final List<Runnable> fActs = acts;
        new AlertDialog.Builder(act).setTitle("Start from").setItems(titles.toArray(new String[0]), (d, w) -> fActs.get(w).run()).setNegativeButton("Cancel", null).show();
    }

    void startFromPattern(JSONObject cc) {
        put("base", cc.optInt("base")); put("exact", cc.optBoolean("exact")); put("precise", cc.optBoolean("precise"));
        if (cc.has("feet")) put("feet", cc.optInt("feet")); else pattern.remove("feet");
        put("drop", cc.optInt("drop")); dropStepper.setValue(cc.optInt("drop"));
        put("from", cc.optString("name"));
        fwd.clear(); rev.clear();
        for (JSONArray s : toList(Oil.cleanSteps(cc.optJSONArray("fwd")))) fwd.add(s);
        for (JSONArray s : toList(Oil.cleanSteps(cc.optJSONArray("rev")))) rev.add(s);
        rebuildSteps(); schedulePreview(); refreshBaseMenu();
    }

    void rebuildSteps() {
        fwdEnds.clear(); revEnds.clear();
        fwdStack.removeAllViews(); revStack.removeAllViews();
        for (int i = 0; i < fwd.size(); i++) fwdStack.addView(stepRow(fwd, i, fwdEnds), Menu.mt(c, 8));
        for (int i = 0; i < rev.size(); i++) revStack.addView(stepRow(rev, i, revEnds), Menu.mt(c, 8));
    }

    View stepRow(final List<JSONArray> list, final int i, List<TextView> ends) {
        final JSONArray s = list.get(i);
        while (s.length() < 5) s.put(0);
        if (s.length() < 6) s.put(50);
        LinearLayout box = UiKit.row(c, false);
        box.setBackground(UiKit.round(UiKit.dim(0.04f), 12, c));
        int p = UiKit.dp(c, 10);
        box.setPadding(p, p, p, p);
        LinearLayout top = UiKit.row(c, true);
        top.addView(UiKit.label(c, "Step " + (i + 1), 13, Color.WHITE, true));
        TextView end = UiKit.label(c, "", 12, UiKit.dim(0.55f), true);
        ends.add(end);
        LinearLayout.LayoutParams elp = Menu.mlH(c, 8); elp.weight = 1;
        top.addView(end, elp);
        final boolean forward = list == fwd;
        Button del = UiKit.button(c, "\u2715", false, false);
        del.setBackgroundColor(Color.TRANSPARENT);
        del.setTextColor(UiKit.dim(0.5f));
        del.setOnClickListener(v -> { list.remove(i); put("exact", false); put("precise", false); pattern.remove("feet"); rebuildSteps(); schedulePreview(); });
        top.addView(del);
        box.addView(top);

        String[] titles = { "START", "STOP", "LOADS", "SPEED in/s", "TRAVEL TO (no oil)" };
        int[] lo = { 1, 1, 0, 6, 0 }, hi = { 39, 39, 99, 30, 70 };
        final Stepper[] st = new Stepper[5];
        final Stepper travel;
        for (int k = 0; k < 5; k++) {
            final int fk = k;
            Stepper step = new Stepper(c, titles[k], lo[k], hi[k], k < 2 ? Editor::boardLabel : k == 4 ? (v -> v + " ft") : null);
            step.setValue(k == 4 ? Math.round((float) s.optDouble(4, 0)) : s.optInt(k));
            st[k] = step;
        }
        travel = st[4];
        travel.setVisibility(s.optInt(2) != 0 ? View.GONE : View.VISIBLE);
        for (int k = 0; k < 5; k++) {
            final int fk = k;
            st[k].changed = v -> {
                try { s.put(fk, v); } catch (Throwable ignored) {}
                if (fk == 2 || fk == 3) { put("exact", false); put("precise", false); pattern.remove("feet"); }
                if (fk == 2) { travel.setVisibility(v != 0 ? View.GONE : View.VISIBLE); if (v == 0 && forward && s.optDouble(4, 0) < 1) { try { s.put(4, 40); } catch (Throwable ignored) {} travel.setValue(40); } }
                schedulePreview();
            };
        }
        Stepper mics = new Stepper(c, "MICS (\u00B5L)", 5, 150, null);
        mics.setValue(s.optInt(5, 50));
        mics.changed = v -> { try { s.put(5, v); } catch (Throwable ignored) {} schedulePreview(); };

        LinearLayout r1 = UiKit.row(c, true);
        r1.addView(st[0], UiKit.lpWeight(1)); r1.addView(Menu.space(c, 8)); r1.addView(st[1], UiKit.lpWeight(1));
        LinearLayout r2 = UiKit.row(c, true);
        r2.addView(st[2], UiKit.lpWeight(1)); r2.addView(Menu.space(c, 8)); r2.addView(st[3], UiKit.lpWeight(1));
        box.addView(r1, Menu.mt(c, 8));
        box.addView(r2, Menu.mt(c, 8));
        box.addView(mics, Menu.mt(c, 8));
        box.addView(travel, Menu.mt(c, 8));
        return box;
    }

    void addStep(boolean forward) {
        put("precise", false); put("exact", false); pattern.remove("feet");
        List<JSONArray> list = forward ? fwd : rev;
        JSONArray last = list.isEmpty() ? (forward ? mk(10, 30, 2, 16, 0) : mk(8, 32, 2, 18, 0)) : listTail(list);
        if (list.size() < 40) list.add(last);
        rebuildSteps(); schedulePreview();
    }
    JSONArray listTail(List<JSONArray> l) { try { return new JSONArray(l.get(l.size() - 1).toString()); } catch (Throwable t) { return new JSONArray(); } }

    // Every edit comes through here. It also cancels a "Start from" answer still on its way, so a late answer
    // can never wipe out steps you've already changed.
    void schedulePreview() { startGen++; debounce.removeCallbacksAndMessages(null); debounce.postDelayed(this::updatePreview, 150); }

    void updatePreview() {
        final String arg = computeArgNow().toString();
        final int w = preview.getWidth() > 10 ? preview.getWidth() : UiKit.dp(c, 340), h = preview.getHeight() > 10 ? preview.getHeight() : UiKit.dp(c, 96);
        final int gen = ++previewGen;
        WORK.execute(() -> {
            if (gen != previewGen) return;   // a newer edit is already queued
            String cr = N.call("oilCompute", arg);
            JSONObject r = null;
            android.graphics.Bitmap bmp = null;
            try {
                if (cr != null && !cr.equals("null")) { r = new JSONObject(cr); if (r.length() > 0) bmp = Oil.renderPreview(r, w, h); }
            } catch (Throwable ignored) {}
            final String fcr = cr; final JSONObject fr = r; final android.graphics.Bitmap fb = bmp;
            BP.UI.post(() -> { if (gen == previewGen) applyPreview(fcr, fr, fb); });
        });
    }

    void applyPreview(String cr, JSONObject r, android.graphics.Bitmap bmp) {
        if (cr == null || cr.equals("null")) { stats.setText("Preview shows up once the game has loaded a lane."); return; }
        if (r == null) { stats.setText("Preview error."); return; }
        try {
            if (r.length() == 0) { stats.setText("The game couldn't build this pattern. Try fewer or smaller steps."); return; }
            if (bmp != null) preview.setImageBitmap(bmp);
            float far = 0;
            JSONArray fe = r.optJSONArray("fwd"), re = r.optJSONArray("rev");
            for (int i = 0; i < fwdEnds.size() && fe != null && i < fe.length(); i++) { float ft = (float) fe.optJSONArray(i).optDouble(4); far = Math.max(far, ft); fwdEnds.get(i).setText("to " + String.format("%.1f", ft) + " ft"); }
            for (int i = 0; i < revEnds.size() && re != null && i < re.length(); i++) revEnds.get(i).setText("back to " + String.format("%.1f", re.optJSONArray(i).optDouble(4)) + " ft");
            int loads = 0;
            for (JSONArray s : fwd) loads += s.optInt(2);
            for (JSONArray s : rev) loads += s.optInt(2);
            stats.setText(String.format("Oil out to %.0f ft \u00B7 %d loads \u00B7 %d forward, %d reverse steps", far, loads, fwd.size(), rev.size()));
        } catch (Throwable t) { stats.setText("Preview error."); }
    }

    JSONObject computeArgNow() {
        JSONObject a = new JSONObject();
        try {
            a.put("base", pattern.optInt("base"));
            a.put("fwd", new JSONArray(fwd.toString())); a.put("rev", new JSONArray(rev.toString()));
            a.put("drop", pattern.optInt("drop")); a.put("exact", pattern.optBoolean("exact"));
            a.put("feet", pattern.optInt("feet")); a.put("precise", pattern.optBoolean("precise"));
        } catch (Throwable ignored) {}
        return a;
    }

    void cancel() { cancelCleanup(); FrameLayout root = UiKit.content(act); if (root != null) root.removeView(holder); }
    void cancelCleanup() { debounce.removeCallbacksAndMessages(null); previewGen++; startGen++; }   // tapped outside: stop the timer, drop pending answers

    void save() {
        String name = nameField.getText().toString().trim();
        put("name", name.isEmpty() ? "My pattern" : name.substring(0, Math.min(40, name.length())));
        try { pattern.put("fwd", Oil.cleanSteps(new JSONArray(fwd.toString()))); pattern.put("rev", Oil.cleanSteps(new JSONArray(rev.toString()))); } catch (Throwable ignored) {}
        JSONObject saved = Oil.clone(pattern);
        Oil.upsert(saved);
        cancel();
        if (onDone != null) onDone.run(saved);
    }

    void put(String k, Object v) { try { pattern.put(k, v); } catch (Throwable ignored) {} }
    static JSONArray mk(int a, int b, int l, int sp, int ft) { JSONArray s = new JSONArray(); s.put(a); s.put(b); s.put(l); s.put(sp); s.put(ft); s.put(50); return s; }
    static String boardLabel(int b) { if (b < 20) return b + "L"; if (b == 20) return "20"; return (40 - b) + "R"; }

    // ---- a big, thumb-friendly stepper (BFOilStepper) ----
    static final class Stepper extends LinearLayout {
        interface Fmt { String run(int v); }
        interface Changed { void run(int v); }
        int value, min, max;
        TextView valueLabel;
        Fmt format;
        Changed changed;

        Stepper(Context c, String title, int lo, int hi, Fmt fmt) {
            super(c);
            min = lo; max = hi; format = fmt;
            setBackground(UiKit.round(UiKit.dim(0.07f), 10, c));
            setGravity(Gravity.CENTER_VERTICAL);
            Button minus = pad(c, "\u2212"), plus = pad(c, "+");
            TextView t = UiKit.label(c, title, 10, UiKit.dim(0.5f), true);
            t.setGravity(Gravity.CENTER);
            valueLabel = UiKit.label(c, "", 16, Color.WHITE, true);
            valueLabel.setGravity(Gravity.CENTER);
            LinearLayout mid = UiKit.row(c, false);
            mid.addView(t); mid.addView(valueLabel);
            addView(minus);
            addView(mid, UiKit.lpWeight(1));
            addView(plus);
            minus.setOnClickListener(v -> step(-1));
            plus.setOnClickListener(v -> step(1));
        }
        Button pad(Context c, String s) {
            Button b = new Button(c);
            b.setText(s); b.setAllCaps(false);
            b.setTextColor(UiKit.YELLOW);
            b.setTextSize(22);
            b.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);
            b.setBackgroundColor(Color.TRANSPARENT);
            b.setStateListAnimator(null);
            b.setLayoutParams(UiKit.lp(UiKit.dp(c, 40), UiKit.dp(c, 44)));
            b.setPadding(0, 0, 0, 0);
            return b;
        }
        void setValue(int v) { value = Math.max(min, Math.min(max, v)); valueLabel.setText(format != null ? format.run(value) : String.valueOf(value)); }
        void step(int d) { int v = Math.max(min, Math.min(max, value + d)); if (v == value) return; setValue(v); if (changed != null) changed.run(v); }
    }
}
