package com.bowlingplus;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;

import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

// The shake menu (Menu.mm). Plain Android views over the game. One card per topic (collapsible), rows with a
// switch; descriptions stay hidden until you tap a row's title or the (i) in the header. The pin picker and
// the Skip tutorial button live here too.
public final class Menu {
    static FrameLayout overlay;
    static Activity act;
    static final List<TextView> helpLabels = new ArrayList<>();
    static TextView statusLabel, ballLabel, arsenalLabel, fpsLabel, oilLabel, autoLabel, pinImageLabel, speedLabel, spinLabel;
    static Switch spareSwitch, autoSwitch;
    static Button resumeButton, updateButton, skipButton;
    static SeekBar speedSlider, spinSlider;
    static EditText searchField;
    static Button helpButton, oilColorButton;
    static String updateUrl;
    static final android.os.Handler refresh = new android.os.Handler(android.os.Looper.getMainLooper());

    private Menu() {}

    static boolean visible() { return overlay != null && overlay.getParent() != null; }

    public static void toggle(Activity a) {
        if (pickerVisible()) return;              // finish picking pins first
        if (visible()) hide();
        else show(a);
    }

    static void show(Activity a) {
        act = a;
        FrameLayout root = UiKit.content(a);
        if (root == null) return;
        overlay = build(a);
        root.addView(overlay);
        MenuButton.refresh();                      // hide the floating button while the menu itself is open
        N.call("menuVisible", "1");
        BP.poke();                                 // so state() reflects the latest game data
        syncAll();
        tickRefresh();
        refresh.removeCallbacksAndMessages(null);
        refresh.postDelayed(refreshRunner, 500);
    }

    static void hide() {
        if (overlay == null) return;
        FrameLayout root = UiKit.content(act);
        if (root != null) root.removeView(overlay);
        overlay = null;
        refresh.removeCallbacksAndMessages(null);
        N.call("menuVisible", "0");
        MenuButton.refresh();                       // bring the floating button back, if it's turned on
    }

    static Runnable refreshRunner = new Runnable() {
        public void run() { if (visible()) { BP.poke(); tickRefresh(); refresh.postDelayed(this, 500); } }
    };

    // tickRefresh() used to call N.call("state") directly - a round-trip to the game's thread, up to 1.5 s -
    // right on the Android main thread, every 500 ms while the menu is open. That's what made scrolling the
    // menu feel laggy: every redraw competed with a call that could block the whole UI thread. Now the native
    // call runs on its own thread and only the (cheap) "apply it to the views" step touches the UI thread.
    static final java.util.concurrent.atomic.AtomicBoolean stateFetchInFlight = new java.util.concurrent.atomic.AtomicBoolean(false);
    // one long-lived worker for the menu's calls into the game (instead of a new thread every 500 ms)
    static final java.util.concurrent.ExecutorService IO = java.util.concurrent.Executors.newSingleThreadExecutor(r -> {
        Thread t = new Thread(r, "BowlingPlus-menu"); t.setDaemon(true); return t;
    });

    // ---- building ----
    static FrameLayout build(final Activity a) {
        Context c = a;
        FrameLayout ov = UiKit.overlay(a, Menu::hide);
        LinearLayout stack = UiKit.row(c, false);
        helpLabels.clear();

        // header
        LinearLayout header = UiKit.row(c, true);
        TextView title = UiKit.label(c, "\uD83C\uDFB3 BowlingPlus", 22, Color.WHITE, true);
        header.addView(title, UiKit.lpWeight(1));
        helpButton = UiKit.button(c, "\u24D8", false, true);
        helpButton.setBackground(UiKit.round(UiKit.dim(0.08f), 17, c));
        helpButton.setOnClickListener(v -> toggleHelp());
        header.addView(helpButton);
        Button close = UiKit.button(c, "\u2715", false, false);
        close.setBackgroundColor(Color.TRANSPARENT);
        close.setTextColor(UiKit.dim(0.7f));
        close.setOnClickListener(v -> hide());
        header.addView(close);
        stack.addView(header);

        statusLabel = UiKit.label(c, "", 13, UiKit.ACCENT, true);
        stack.addView(statusLabel);
        resumeButton = UiKit.button(c, "Turn BowlingPlus back on", true, false);
        resumeButton.setOnClickListener(v -> { N.call("exitSafe"); BP.poke(); tickRefresh(); });
        stack.addView(resumeButton, mt(c, 8));

        // ---- Arsenal search
        LinearLayout searchRow = UiKit.row(c, true);
        searchField = UiKit.field(c, "Ball name, e.g. match up");
        searchRow.addView(searchField, UiKit.lpWeight(1));
        Button go = UiKit.button(c, "Search", true, false);
        Button clear = UiKit.button(c, "Clear", false, false);
        go.setOnClickListener(v -> { N.call("arsenal", searchField.getText().toString()); hide(); });
        clear.setOnClickListener(v -> { searchField.setText(""); N.call("arsenal", ""); tickRefresh(); });
        searchRow.addView(go, mlH(c, 8)); searchRow.addView(clear, mlH(c, 8));
        searchField.setOnEditorActionListener((tv, actionId, ev) -> {
            if (actionId == EditorInfo.IME_ACTION_SEARCH || actionId == EditorInfo.IME_ACTION_DONE) { go.performClick(); return true; }
            return false;
        });
        arsenalLabel = UiKit.label(c, "", 12, UiKit.dim(0.55f), false);
        LinearLayout searchCell = UiKit.row(c, false);
        searchCell.addView(searchRow);
        searchCell.addView(arsenalLabel, mt(c, 8));
        stack.addView(group(c, "\uD83D\uDD0E", "Arsenal search", "search", true, new View[]{ cell(c, searchCell) }));

        // ---- Practice fun
        speedLabel = UiKit.label(c, "", 16, UiKit.ACCENT, true);
        speedSlider = slider(c, (int) ((BF.MAX_SPEED - 1) * 10));   // 1.0..5.0 step 0.1
        speedSlider.setOnSeekBarChangeListener(sliderListener(() -> {
            Config.set("speed", 1.0 + speedSlider.getProgress() / 10.0);
            updateSpeedLabels();
        }));
        View speedCell = sliderCell(c, "Ball speed", speedLabel, speedSlider, "Multiplies the ball's speed right after you let go of it. 1x to 5x.",
                () -> { Config.set("speed", 1.0); speedSlider.setProgress(0); updateSpeedLabels(); });

        spinLabel = UiKit.label(c, "", 16, UiKit.ACCENT, true);
        spinSlider = slider(c, (int) ((BF.MAX_SPIN - 1) * 2));      // 1..17 step 0.5
        spinSlider.setOnSeekBarChangeListener(sliderListener(() -> {
            Config.set("spin", 1.0 + spinSlider.getProgress() / 2.0);
            updateSpeedLabels();
        }));
        View spinCell = sliderCell(c, "Ball spin (RPM)", spinLabel, spinSlider,
                "Multiplies your throw's spin right after release (the game caps it near 600 rpm). 17x takes a 600 rpm throw to about 10,000. The extra revs also add grip on the lane, so the ball hooks more. Very fast spin can look slow or backwards on screen (like car wheels in videos).",
                () -> { Config.set("spin", 1.0); spinSlider.setProgress(0); updateSpeedLabels(); });

        spareSwitch = toggle(c, "spare", null);
        autoSwitch = toggle(c, "auto", () -> tickRefresh());
        autoLabel = UiKit.label(c, "", 12, UiKit.ACCENT, true);
        View spareCell = cell(c, rowView(c, "Spare shooting mode",
                "Pick which pins stand at the start of every frame. Tip: you don't need this on to pick pins for one shot. Tap the pin layout (top right while you hold the ball, or the little screen under the ball return in the overhead view) and the pin picker opens just for that shot. Scores in this mode are just for fun.",
                spareSwitch));
        LinearLayout autoStack = UiKit.row(c, false);
        autoStack.addView(rowView(c, "Auto-rack", "Sets up the same pins every frame without asking. Tip: pick your pins once and tap Auto in the pin picker.", autoSwitch));
        autoStack.addView(autoLabel, mt(c, 6));
        View laneCell = cell(c, rowView(c, "Bowl on the other lane (experimental)", "Practice only. Moves you to the game's other lane while you're at the ball rack, like dragging your shoes over (which the game snaps back). If the game keeps moving you back, BowlingPlus stops trying and Copy debug info says so. Turn it off to go back. Not checked yet: how the other lane plays (its oil and pins).", toggle(c, "laneOther", null)));
        stack.addView(group(c, "\uD83C\uDFAF", "Practice fun", "fun", true, new View[]{ speedCell, spinCell, spareCell, cell(c, autoStack), laneCell }));

        // ---- Oil
        oilColorButton = UiKit.button(c, "", false, true);
        oilColorButton.setLayoutParams(UiKit.lp(UiKit.dp(c, 44), UiKit.dp(c, 30)));
        oilColorButton.setOnClickListener(v -> { hide(); Oil.showColorPicker(act); });
        oilLabel = UiKit.label(c, "", 12, UiKit.ACCENT, true);
        TextView always = UiKit.label(c, "ALWAYS ON", 10, UiKit.ACCENT, true);
        LinearLayout libStack = UiKit.row(c, false);
        Button libBtn = UiKit.button(c, "\uD83D\uDEE2  Custom oil patterns", false, false);
        libBtn.setOnClickListener(v -> { hide(); Oil.showLibrary(act); });
        libStack.addView(libBtn);
        libStack.addView(oilLabel, mt(c, 8));
        stack.addView(group(c, "\uD83D\uDEE2", "Oil (practice)", "oil", true, new View[]{
                cell(c, rowView(c, "Real-life oil", "In Practice, the game's own patterns and yours are drawn from their real Kegel data: exact distances, microliters per step, reverse oil adding on top, the brush carrying oil back to the foul line, and Kegel's brushed film over the whole lane, with left on the left. Online matches, tournaments and the tutorial always use the game's own oil.", always)),
                cell(c, rowView(c, "Oil color", "Pick the color the lane shows oil in, or keep the game's.", oilColorButton)),
                cell(c, rowView(c, "Show oil thickness", "Stronger shading by oil thickness (darker = more oil) instead of the game's look. Looks only, the ball feels the same.", toggle(c, "oilThick2", null))),
                cell(c, rowView(c, "Show oil breakdown", "Redraws the lane oil after every shot so you can watch it break down over the game.", toggle(c, "oilBreak", null))),
                cell(c, rowView(c, "Invisible oil", "Hides the oil and plays a random unlocked game pattern each game. Read the lane like the real thing.", toggle(c, "oilInvis", () -> tickRefresh()))),
                cell(c, rowView(c, "Fix oil display side", "The game drew the oil mirrored, so breakdown and carrydown showed up on the wrong side. Now they show where your ball went.", toggle(c, "oilMirror", null))),
                cell(c, libStack) }));

        // ---- Fixes
        ballLabel = UiKit.label(c, "", 12, UiKit.dim(0.5f), false);
        LinearLayout skinStack = UiKit.row(c, false);
        skinStack.addView(rowView(c, "Match Up skins", "Fixes the Match Up Pearl/BP ball textures.", toggle(c, "tex", () -> tickRefresh())));
        skinStack.addView(ballLabel, mt(c, 6));
        stack.addView(group(c, "\uD83D\uDEE0", "Fixes", "fixes", true, new View[]{
                cell(c, skinStack),
                cell(c, rowView(c, "Pin physics fix", "Fast pins can't fly through other pins, and pins clipped low at the base can tip over properly. Practice only.", toggle(c, "pin", null))),
                cell(c, rowView(c, "Improve spinning pin collision (experimental)", "Uses Unity's speculative collisions, which also predict spin.", toggle(c, "pinSpec2", null))),
                cell(c, rowView(c, "Fix connection", "If loading sits on \"connecting\" for 30 s, shows the game's gray offline button. A loading circle stuck for 30 s gets hidden so you can try again.", toggle(c, "unstick", null))),
                cell(c, rowView(c, "Game server over IPv4", "The game always picks IPv6 when some DNS servers offer it, but its servers don't answer on IPv6, so it hangs indefinitely. This forces IPv4.", toggle(c, "ipv4", null))) }));

        // ---- Pins & display
        fpsLabel = UiKit.label(c, "", 12, UiKit.dim(0.5f), false);
        LinearLayout fpsStack = UiKit.row(c, false);
        fpsStack.addView(rowView(c, "120 FPS mode", "Runs menus and gameplay at 120 FPS on 120 Hz screens: smoother, with faster touch response. The game normally uses 30 FPS menus / 60 FPS play. Uses more battery.", toggle(c, "fps120", () -> tickRefresh())));
        fpsStack.addView(fpsLabel, mt(c, 6));
        pinImageLabel = UiKit.label(c, "", 12, UiKit.ACCENT, true);
        Switch pinImageSwitch = toggle(c, "pinImage", () -> {
            if (Config.b("pinImage", false)) Oil.pickPinImage(act, false, Menu::onPinPicked);
            tickRefresh();
        });
        Button photos = UiKit.button(c, "From Photos", false, true);
        Button files = UiKit.button(c, "From Files", false, true);
        photos.setOnClickListener(v -> Oil.pickPinImage(act, false, Menu::onPinPicked));
        files.setOnClickListener(v -> Oil.pickPinImage(act, true, Menu::onPinPicked));
        LinearLayout pickRow = UiKit.row(c, true);
        pickRow.addView(photos, UiKit.lpWeight(1)); pickRow.addView(space(c, 8)); pickRow.addView(files, UiKit.lpWeight(1));
        Button guide = UiKit.button(c, "Get the wrap template + guide", false, true);
        guide.setOnClickListener(v -> Oil.sharePinGuide(act));
        LinearLayout pinStack = UiKit.row(c, false);
        pinStack.addView(rowView(c, "Use my own pin image", "Puts your own picture on the pins (all lanes). Draw on the wrap template: one sheet that wraps around the pin like paper, so there are no seams. Looks only.", pinImageSwitch));
        pinStack.addView(pickRow, mt(c, 8)); pinStack.addView(guide, mt(c, 8)); pinStack.addView(pinImageLabel, mt(c, 8));
        stack.addView(group(c, "\uD83C\uDFA8", "Pins & display", "look", true, new View[]{ cell(c, fpsStack), cell(c, pinStack) }));

        // ---- Help & diagnostics
        Button debug = UiKit.button(c, "Copy debug info", false, false);
        Button log = UiKit.button(c, "Copy log", false, false);
        Button net = UiKit.button(c, "Run connection test", false, false);
        // "debug" waits for Unity (up to 4 s): ask on a worker thread so the UI never freezes
        debug.setOnClickListener(v -> copyAsync(debug, "Copy debug info", false));
        log.setOnClickListener(v -> copyAsync(log, "Copy log", true));
        net.setOnClickListener(v -> { N.call("netTest"); net.setText("Testing (~20 s)..."); BP.UI.postDelayed(() -> net.setText("Run connection test"), 20000); });
        TextView dh = UiKit.label(c, "If something looks off, tap Copy debug info and paste it in a GitHub issue. The log records loading, the connection and network checks from the moment the game starts: run the connection test, wait ~20 s, then Copy log.", 12, UiKit.dim(0.6f), false);
        dh.setVisibility(Config.b("menuHelp", false) ? View.VISIBLE : View.GONE);
        helpLabels.add(dh);
        LinearLayout diag = UiKit.row(c, false);
        diag.addView(debug);
        LinearLayout logRow = UiKit.row(c, true);
        logRow.addView(log, UiKit.lpWeight(1)); logRow.addView(space(c, 8)); logRow.addView(net, UiKit.lpWeight(1));
        diag.addView(logRow, mt(c, 8)); diag.addView(dh, mt(c, 8));
        stack.addView(group(c, "\uD83E\uDE7A", "Help & diagnostics", "help", false, new View[]{ cell(c, diag) }));

        // ---- Menu button (an alternative/addition to shaking)
        LinearLayout btnStack = UiKit.row(c, false);
        btnStack.addView(rowView(c, "On-screen menu button", "A small round button you can tap to open the menu, instead of shaking. Press and drag it to move it; it remembers where you put it. Shake and the three-finger tap keep working too.", toggle(c, "menuButton", () -> MenuButton.refresh())));
        stack.addView(group(c, "\uD83D\uDD18", "Menu button", "menubtn", false, new View[]{ cell(c, btnStack) }));

        // ---- Facebook login + diagnostics
        LinearLayout fb = UiKit.row(c, false);
        fb.addView(rowView(c, "Browser Facebook login", "The normal login hands off to the Facebook app, which rejects a re-signed app (\"invalid key hash\"). This makes login open Facebook in a browser tab instead, which logs in by App ID, not the app's signature. Uses the game's own Facebook app, and you log in on Facebook's real page. On: the only login that can work on a patched build. Change needs a restart to take full effect.", toggle(c, "fbWebLogin", null)));
        fb.addView(cell(c, rowView(c, "Log all server hosts", "Writes every server name the game looks up to the log (once each). After a login, Copy log shows whether your profile is fetched from Facebook or from the game's own servers. Leave off unless you're checking.", toggle(c, "logHosts", null))), mt(c, 8));
        stack.addView(group(c, "\uD83D\uDD11", "Account & login", "login", false, new View[]{ cell(c, fb) }));

        // ---- Back up my data
        Button backup = UiKit.button(c, "\uD83D\uDCE6  Back up my data", true, false);
        backup.setOnClickListener(v -> Backup.run(act));
        TextView backupHelp = UiKit.label(c, "Saves everything the game keeps on this phone - settings, local save data, and a cached Facebook session if you've logged in - to one file you can save to Drive, email to yourself, etc. A safety net if the game or its Facebook login ever stop working. Doesn't include anything that only lives on the game's servers. Keep the file private, like a password: the cached Facebook session in it can be used to get into your account.", 12, UiKit.dim(0.6f), false);
        stack.addView(group(c, "\uD83D\uDCBE", "Backup", "backup", false, new View[]{ cell(c, backup), cell(c, backupHelp) }));

        stack.addView(footer(c), mt(c, 12));

        ScrollView holder = UiKit.cardScroll(a, stack, Gravity.CENTER, 380);
        ov.addView(holder);
        return ov;
    }

    // ---- helpers ----
    static LinearLayout.LayoutParams mt(Context c, int topDp) { LinearLayout.LayoutParams p = UiKit.lp(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT); p.topMargin = UiKit.dp(c, topDp); return p; }
    static LinearLayout.LayoutParams mlH(Context c, int leftDp) { LinearLayout.LayoutParams p = UiKit.lp(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT); p.leftMargin = UiKit.dp(c, leftDp); return p; }
    static View space(Context c, int w) { View v = new View(c); v.setLayoutParams(UiKit.lp(UiKit.dp(c, w), 1)); return v; }

    static Switch toggle(Context c, final String key, final Runnable after) {
        final Switch s = UiKit.sw(c, Config.b(key, false));
        s.setOnCheckedChangeListener((bv, on) -> { Config.set(key, on); if (after != null) after.run(); });
        s.setTag(key);
        return s;
    }

    // one row: title (+ (i) and a hidden description), the control on the right
    static View rowView(Context c, String title, String help, View control) {
        LinearLayout top = UiKit.row(c, true);
        final TextView t = UiKit.label(c, title + (help != null ? "  \u24D8" : ""), 16, Color.WHITE, true);
        top.addView(t, UiKit.lpWeight(1));
        if (control != null) top.addView(control);
        LinearLayout col = UiKit.row(c, false);
        col.addView(top);
        if (help != null) {
            final TextView h = UiKit.label(c, help, 12, UiKit.dim(0.6f), false);
            h.setVisibility(Config.b("menuHelp", false) ? View.VISIBLE : View.GONE);
            helpLabels.add(h);
            col.addView(h, mt(c, 6));
            t.setOnClickListener(v -> h.setVisibility(h.getVisibility() == View.VISIBLE ? View.GONE : View.VISIBLE));
        }
        return col;
    }

    static View cell(Context c, View content) {
        LinearLayout cl = UiKit.row(c, false);
        int p = UiKit.dp(c, 12), ph = UiKit.dp(c, 14);
        cl.setPadding(ph, p, ph, p);
        cl.addView(content);
        return cl;
    }

    static View group(Context c, String icon, String title, final String key, boolean defOpen, View[] cells) {
        final boolean[] open = { prefBool("menuOpen." + key, defOpen) };
        LinearLayout g = UiKit.row(c, false);
        LinearLayout headRow = UiKit.row(c, true);
        TextView head = UiKit.label(c, icon + "  " + title.toUpperCase(), 12, UiKit.dim(0.6f), true);
        final TextView chev = UiKit.label(c, "\u203A", 22, UiKit.dim(0.45f), true);
        headRow.addView(head, UiKit.lpWeight(1));
        headRow.addView(chev);
        headRow.setPadding(UiKit.dp(c, 4), UiKit.dp(c, 4), UiKit.dp(c, 4), UiKit.dp(c, 4));

        final LinearLayout panel = UiKit.row(c, false);
        panel.setBackground(UiKit.roundStroke(UiKit.PANEL_BG, 14, UiKit.dim(0.06f), 1, c));
        for (int i = 0; i < cells.length; i++) {
            if (i > 0) { View sep = new View(c); sep.setBackgroundColor(UiKit.dim(0.07f)); sep.setLayoutParams(UiKit.lp(ViewGroup.LayoutParams.MATCH_PARENT, 1)); panel.addView(sep); }
            panel.addView(cells[i]);
        }
        chev.setRotation(open[0] ? 90 : 0);
        panel.setVisibility(open[0] ? View.VISIBLE : View.GONE);
        headRow.setOnClickListener(v -> {
            open[0] = !open[0];
            setPref("menuOpen." + key, open[0]);
            chev.setRotation(open[0] ? 90 : 0);
            panel.setVisibility(open[0] ? View.VISIBLE : View.GONE);
        });
        g.addView(headRow, mt(c, 12));
        g.addView(panel, mt(c, 6));
        return g;
    }

    static SeekBar slider(Context c, int max) {
        SeekBar s = new SeekBar(c);
        s.setMax(max);
        s.getProgressDrawable().setTint(UiKit.ACCENT);
        s.getThumb().setTint(UiKit.ACCENT);
        return s;
    }

    interface Run { void run(); }
    static SeekBar.OnSeekBarChangeListener sliderListener(final Run onChange) {
        return new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar sb, int p, boolean user) { if (user) onChange.run(); }
            public void onStartTrackingTouch(SeekBar sb) {}
            public void onStopTrackingTouch(SeekBar sb) {}
        };
    }

    static View sliderCell(Context c, String title, TextView value, SeekBar slider, String help, final Run reset) {
        View head = rowView(c, title, help, null);
        LinearLayout top = (LinearLayout) ((LinearLayout) head).getChildAt(0);
        top.addView(value);
        Button r = UiKit.button(c, "Reset", false, true);
        r.setOnClickListener(v -> reset.run());
        top.addView(r, mlH(c, 8));
        LinearLayout col = UiKit.row(c, false);
        col.addView(head);
        col.addView(slider, mt(c, 8));
        return cell(c, col);
    }

    static View footer(Context c) {
        LinearLayout col = UiKit.row(c, false);
        col.setGravity(Gravity.CENTER_HORIZONTAL);
        LinearLayout row1 = UiKit.row(c, true);
        row1.setGravity(Gravity.CENTER);   // this row is full width; without this its items sat at the left
        row1.addView(UiKit.image(c, N.bytes("logo"), 25));
        Button gh = linkButton(c, "BowlingPlus", "https://github.com/NicholasBly/BowlingPlus");
        Button donate = linkButton(c, "\u2665 Donate", "https://github.com/sponsors/NicholasBly");
        row1.addView(gh, mlH(c, 8));
        row1.addView(UiKit.label(c, "\u00B7", 15, UiKit.dim(0.35f), true), mlH(c, 8));
        row1.addView(donate, mlH(c, 8));
        col.addView(row1);
        Button updates = linkButton(c, "Check for updates", null);
        updates.setOnClickListener(v -> checkForUpdates());
        col.addView(updates, mt(c, 4));
        updateButton = UiKit.button(c, "", false, true);
        updateButton.setBackgroundColor(Color.TRANSPARENT);
        updateButton.setVisibility(View.GONE);
        updateButton.setOnClickListener(v -> { if (updateUrl != null) UiKit.openUrl(act, updateUrl); });
        col.addView(updateButton);
        TextView ver = UiKit.label(c, "v" + BF.VERSION + " \u00B7 Shake again or tap outside to close", 11, UiKit.dim(0.35f), false);
        ver.setGravity(Gravity.CENTER);
        col.addView(ver, mt(c, 4));
        return col;
    }

    static Button linkButton(Context c, String title, final String url) {
        Button b = UiKit.button(c, title, false, true);
        b.setBackgroundColor(Color.TRANSPARENT);
        b.setTextColor(UiKit.ACCENT);
        b.setPaintFlags(b.getPaintFlags() | android.graphics.Paint.UNDERLINE_TEXT_FLAG);
        if (url != null) b.setOnClickListener(v -> UiKit.openUrl(act, url));
        return b;
    }

    // ---- live refresh ----
    static JSONObject lastState = new JSONObject();

    static void tickRefresh() {
        if (!stateFetchInFlight.compareAndSet(false, true)) return;   // one in flight is enough; this tick's data will arrive moments later
        IO.execute(() -> {
            String s = null;
            try { s = N.call("state"); } finally { stateFetchInFlight.set(false); }
            final String fs = s;
            if (fs != null) BP.UI.post(() -> applyState(fs));
        });
    }

    static void applyState(String s) {
        try { lastState = new JSONObject(s); Config.load(lastState.optJSONObject("cfg").toString()); } catch (Throwable ignored) {}
        if (!visible()) return;
        setText(statusLabel, lastState.optString("status"));
        setTextOrHide(ballLabel, lastState.optString("ball"));
        setText(arsenalLabel, lastState.optString("arsenal"));
        setTextOrHide(fpsLabel, lastState.optString("fps"));
        setTextOrHide(oilLabel, lastState.optString("oil"));
        setText(pinImageLabel, lastState.optString("pinImage"));
        boolean safe = lastState.optBoolean("safe", false);
        int safeVis = safe ? View.VISIBLE : View.GONE;
        if (resumeButton.getVisibility() != safeVis) resumeButton.setVisibility(safeVis);
        boolean auto = Config.b("auto", false);
        setText(autoLabel, !auto ? "" : Config.b("spare", false)
                ? "Auto-racking " + pinsText(Config.i("mask", BF.ALL_PINS)) + " every frame"
                : "Turn on Spare shooting mode to use Auto-rack");
        int autoVis = auto ? View.VISIBLE : View.GONE;
        if (autoLabel.getVisibility() != autoVis) autoLabel.setVisibility(autoVis);
    }

    static void syncAll() {
        tickRefresh();
        syncHelpButton();
        for (TextView h : helpLabels) h.setVisibility(Config.b("menuHelp", false) ? View.VISIBLE : View.GONE);
        if (overlay == null) return;
        syncSwitches(overlay);
        speedSlider.setProgress((int) Math.round((Config.d("speed", 1) - 1) * 10));
        spinSlider.setProgress((int) Math.round((Config.d("spin", 1) - 1) * 2));
        updateSpeedLabels();
        syncColorButton();
    }

    static void syncSwitches(View v) {
        if (v instanceof Switch && v.getTag() instanceof String) {
            Switch s = (Switch) v;
            s.setChecked(Config.b((String) v.getTag(), s.isChecked()));
        } else if (v instanceof ViewGroup) {
            ViewGroup g = (ViewGroup) v;
            for (int i = 0; i < g.getChildCount(); i++) syncSwitches(g.getChildAt(i));
        }
    }

    static void updateSpeedLabels() {
        double spin = Config.d("spin", 1), speed = Config.d("speed", 1);
        if (spinLabel != null) spinLabel.setText(spin < 1.01 ? "1x" : String.format("%.1fx (600 \u2192 %d)", spin, Math.round(600 * spin)));
        if (speedLabel != null) speedLabel.setText(String.format("%.1fx", speed));
    }

    static void syncColorButton() {
        double hue = Config.d("oilHue", -1);
        if (oilColorButton == null) return;
        int color = hue < 0 ? Color.rgb(219, 179, 128) : Color.HSVToColor(new float[]{ (float) (hue * 360), 0.85f, 0.95f });
        oilColorButton.setBackground(UiKit.round(color, 15, act));
        oilColorButton.setText(hue < 0 ? "game" : "");
        oilColorButton.setTextSize(10);
        oilColorButton.setTextColor(Color.rgb(50, 50, 50));
    }

    static void toggleHelp() {
        boolean on = !Config.b("menuHelp", false);
        Config.set("menuHelp", on);
        syncHelpButton();
        for (TextView h : helpLabels) h.setVisibility(on ? View.VISIBLE : View.GONE);
    }

    static void syncHelpButton() {
        if (helpButton == null) return;
        boolean on = Config.b("menuHelp", false);
        helpButton.setText("\u24D8");
        helpButton.setTextColor(on ? UiKit.ACCENT : UiKit.dim(0.7f));
        helpButton.setBackground(UiKit.round(on ? Color.argb(40, 255, 123, 26) : UiKit.dim(0.08f), 17, act));
    }

    static void onPinPicked(String msg) { if (pinImageLabel != null) pinImageLabel.setText(msg); tickRefresh(); }

    static void copyAsync(final Button b, final String reset, final boolean withLog) {
        b.setText("Collecting\u2026");
        IO.execute(() -> {
            String text = N.call("debug");
            if (withLog) text = text + "\n" + N.call("log");
            final String t = text;
            BP.UI.post(() -> { try { copy(b, reset, t); } catch (Throwable ignored) {} });
        });
    }

    static void copy(Button b, String reset, String text) {
        android.content.ClipboardManager cm = (android.content.ClipboardManager) act.getSystemService(Context.CLIPBOARD_SERVICE);
        cm.setPrimaryClip(android.content.ClipData.newPlainText("BowlingPlus", text == null ? "" : text));
        b.setText("Copied!");
        BP.UI.postDelayed(() -> b.setText(reset), 2500);
    }

    static void checkForUpdates() {
        updateButton.setVisibility(View.VISIBLE);
        updateButton.setText("Checking GitHub\u2026");
        updateButton.setTextColor(UiKit.dim(0.55f));
        new Thread(() -> {
            String latest = null, page = "https://github.com/NicholasBly/BowlingPlus/releases/latest";
            int status = 0;
            try {
                java.net.HttpURLConnection c = (java.net.HttpURLConnection) new java.net.URL("https://api.github.com/repos/NicholasBly/BowlingPlus/releases/latest").openConnection();
                c.setRequestProperty("Accept", "application/vnd.github+json");
                c.setRequestProperty("User-Agent", "BowlingPlus/" + BF.VERSION);
                c.setConnectTimeout(15000); c.setReadTimeout(15000);
                status = c.getResponseCode();   // 404 = the repo has no published release yet (not a network problem)
                java.io.BufferedReader r = new java.io.BufferedReader(new java.io.InputStreamReader(c.getInputStream()));
                StringBuilder sb = new StringBuilder(); String ln; while ((ln = r.readLine()) != null) sb.append(ln);
                r.close();
                JSONObject j = new JSONObject(sb.toString());
                latest = j.optString("tag_name", "").replaceAll("[vV ]", "");
                if (j.has("html_url")) page = j.optString("html_url");
            } catch (Throwable ignored) {}
            final String fl = latest, fp = page;
            final int fs = status;
            BP.UI.post(() -> {
                if (fs == 404) { updateButton.setText("No release has been published on GitHub yet."); updateButton.setTextColor(UiKit.dim(0.55f)); }
                else if (fl == null || fl.isEmpty()) { updateButton.setText("Couldn't reach GitHub. Check your connection and try again."); updateButton.setTextColor(UiKit.dim(0.55f)); }
                else if (BF.compareVersions(fl, BF.VERSION) > 0) { updateUrl = fp; updateButton.setText("BowlingPlus " + fl + " is out (you have " + BF.VERSION + "). Tap to download."); updateButton.setTextColor(UiKit.ACCENT); }
                else { updateButton.setText("You're up to date (" + BF.VERSION + ")."); updateButton.setTextColor(UiKit.dim(0.55f)); }
            });
        }).start();
    }

    // The menu refreshes twice a second; re-setting identical text still costs a layout pass of the whole menu,
    // which is felt while scrolling. Only touch a label whose text actually changed.
    static void setText(TextView t, String s) {
        if (t == null) return;
        String v = s == null ? "" : s;
        if (!v.contentEquals(t.getText())) t.setText(v);
    }
    static void setTextOrHide(TextView t, String s) {
        if (t == null) return;
        setText(t, s);
        int vis = s == null || s.isEmpty() ? View.GONE : View.VISIBLE;
        if (t.getVisibility() != vis) t.setVisibility(vis);
    }

    static boolean prefBool(String k, boolean def) { return act.getSharedPreferences("BowlingPlus", Context.MODE_PRIVATE).getBoolean(k, def); }
    static void setPref(String k, boolean v) { act.getSharedPreferences("BowlingPlus", Context.MODE_PRIVATE).edit().putBoolean(k, v).apply(); }

    static String pinsText(int mask) {
        mask &= BF.ALL_PINS;
        if (mask == BF.ALL_PINS) return "a full rack";
        List<String> pins = new ArrayList<>();
        for (int i = 0; i < 10; i++) if ((mask & (1 << i)) != 0) pins.add(String.valueOf(i + 1));
        return pins.isEmpty() ? "no pins" : android.text.TextUtils.join("-", pins);
    }

    // ===================== pin picker =====================
    static Picker picker;

    static boolean pickerVisible() { return picker != null && picker.overlay.getParent() != null; }

    public static void showPicker(Activity a, int mask, boolean oneShot) {
        act = a;
        hidePickerView();
        picker = new Picker(a, mask & BF.ALL_PINS, oneShot);
        FrameLayout root = UiKit.content(a);
        if (root != null) root.addView(picker.overlay);
    }

    public static void hidePicker() { hidePickerView(); N.call("pickerHidden"); }
    static void hidePickerView() { if (picker != null) { FrameLayout root = UiKit.content(act); if (root != null) root.removeView(picker.overlay); picker = null; } }

    static final class Picker {
        FrameLayout overlay;
        int mask;
        boolean oneShot;
        Button[] pins = new Button[10];
        Button rack, auto;

        Picker(final Activity a, int startMask, boolean one) {
            Context c = a;
            this.oneShot = one;
            this.mask = startMask != 0 ? startMask : BF.ALL_PINS;
            overlay = UiKit.overlay(a, null);
            LinearLayout stack = UiKit.row(c, false);

            LinearLayout header = UiKit.row(c, true);
            header.addView(UiKit.label(c, one ? "Pick your pins" : "Spare mode: pick your pins", 18, Color.WHITE, true), UiKit.lpWeight(1));
            Button x = UiKit.button(c, "\u2715", false, false);
            x.setBackgroundColor(Color.TRANSPARENT);
            x.setTextColor(UiKit.dim(0.7f));
            x.setOnClickListener(v -> { hidePicker(); N.call("spareDismissed"); });
            header.addView(x);
            stack.addView(header);
            stack.addView(UiKit.label(c, one
                    ? "Tap pins to add or remove them, then hit Rack 'em. This is just for this shot: next frame is a full rack again. Turn on Spare shooting mode in the menu to be asked every frame. Scores are just for fun."
                    : "Tap pins to add or remove them, then hit Rack 'em. Auto racks the same pins every frame until you turn it off (shake for the menu). The X leaves everything as it is. Scores in this mode are just for fun.", 12, UiKit.dim(0.55f), false), mt(c, 6));

            // the rack as seen from the foul line: back row on top, head pin at the bottom
            LinearLayout tri = UiKit.row(c, false);
            tri.setGravity(Gravity.CENTER_HORIZONTAL);
            int[][] rows = { { 7, 8, 9, 10 }, { 4, 5, 6 }, { 2, 3 }, { 1 } };
            for (int i = 0; i < 10; i++) pins[i] = pinButton(c, i + 1);
            for (int[] r : rows) {
                LinearLayout rr = UiKit.row(c, true);
                rr.setGravity(Gravity.CENTER);
                for (int p : r) { rr.addView(pins[p - 1]); rr.addView(space(c, 14)); }
                tri.addView(rr, mt(c, 10));
            }
            stack.addView(tri, mt(c, 10));

            String[] presetTitles = { "10 pin", "7 pin", "7-10", "Bucket" };
            int[] presets = { 1 << 9, 1 << 6, (1 << 6) | (1 << 9), (1 << 1) | (1 << 3) | (1 << 4) | (1 << 7) };
            LinearLayout presetRow = UiKit.row(c, true);
            for (int i = 0; i < presetTitles.length; i++) {
                final int m = presets[i];
                Button b = UiKit.button(c, presetTitles[i], false, true);
                b.setOnClickListener(v -> { mask = m; updatePins(); });
                presetRow.addView(b, UiKit.lpWeight(1));
                if (i < presetTitles.length - 1) presetRow.addView(space(c, 8));
            }
            stack.addView(presetRow, mt(c, 10));

            LinearLayout quick = UiKit.row(c, true);
            Button all = UiKit.button(c, "All", false, true), none = UiKit.button(c, "None", false, true), full = UiKit.button(c, "Full rack", false, true);
            all.setOnClickListener(v -> { mask = BF.ALL_PINS; updatePins(); });
            none.setOnClickListener(v -> { mask = 0; updatePins(); });
            full.setOnClickListener(v -> { hidePicker(); N.call(one ? "spareNow" : "spare", String.valueOf(BF.ALL_PINS)); BP.poke(); });
            quick.addView(all, UiKit.lpWeight(1)); quick.addView(space(c, 8)); quick.addView(none, UiKit.lpWeight(1)); quick.addView(space(c, 8)); quick.addView(full, UiKit.lpWeight(1));
            stack.addView(quick, mt(c, 8));

            rack = UiKit.button(c, "Rack 'em", true, false);
            auto = UiKit.button(c, "Auto (every frame)", false, false);
            rack.setOnClickListener(v -> { if (mask == 0) return; hidePicker(); N.call(one ? "spareNow" : "spare", String.valueOf(mask)); BP.poke(); });
            auto.setOnClickListener(v -> { if (mask == 0 || one) return; Config.set("auto", true); hidePicker(); N.call("spare", String.valueOf(mask)); BP.poke(); });
            auto.setVisibility(one ? View.GONE : View.VISIBLE);
            LinearLayout go = UiKit.row(c, true);
            go.addView(rack, UiKit.lpWeight(1));
            if (!one) { go.addView(space(c, 8)); go.addView(auto, UiKit.lpWeight(1)); }
            stack.addView(go, mt(c, 10));

            ScrollView holder = UiKit.cardScroll(a, stack, Gravity.BOTTOM, 360);
            overlay.addView(holder);
            updatePins();
        }

        Button pinButton(Context c, final int pin) {
            Button b = new Button(c);
            b.setAllCaps(false);
            b.setText(String.valueOf(pin));
            b.setTextSize(17);
            b.setTypeface(Typeface.DEFAULT_BOLD);
            b.setStateListAnimator(null);
            b.setLayoutParams(UiKit.lp(UiKit.dp(c, 46), UiKit.dp(c, 46)));
            b.setPadding(0, 0, 0, 0);
            b.setOnClickListener(v -> { mask ^= (1 << (pin - 1)); updatePins(); });
            return b;
        }

        void updatePins() {
            for (int i = 0; i < 10; i++) {
                boolean up = (mask & (1 << i)) != 0;
                pins[i].setBackground(UiKit.roundStroke(up ? Color.WHITE : Color.TRANSPARENT, 23, up ? Color.WHITE : UiKit.dim(0.3f), 2, pins[i].getContext()));
                pins[i].setTextColor(up ? Color.rgb(199, 31, 31) : UiKit.dim(0.4f));
            }
            rack.setEnabled(mask != 0);
            rack.setAlpha(mask != 0 ? 1f : 0.4f);
            if (auto != null) { auto.setEnabled(mask != 0); auto.setAlpha(mask != 0 ? 1f : 0.4f); }
        }
    }

    // ===================== skip tutorial button =====================
    static Button skip;
    public static void setSkipVisible(final Activity a, boolean vis) {
        act = a;
        FrameLayout root = UiKit.content(a);
        if (root == null) return;
        if (!vis) { if (skip != null) root.removeView(skip); return; }
        if (skip == null) {
            skip = UiKit.button(a, "\u23ED Skip tutorial", true, false);
            skip.setOnClickListener(v -> {
                new AlertDialog.Builder(act)
                        .setTitle("Skip the tutorial?")
                        .setMessage("This uses the game's own skip function: the tutorial gets marked as done and you go to the main screen, where you can log in to your account.")
                        .setNegativeButton("Cancel", null)
                        .setPositiveButton("Skip", (d, w) -> { setSkipVisible(act, false); N.call("skipTutorial"); BP.poke(); })
                        .show();
            });
        }
        if (skip.getParent() != root) {
            if (skip.getParent() != null) ((ViewGroup) skip.getParent()).removeView(skip);
            FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            lp.gravity = Gravity.TOP | Gravity.END;
            lp.topMargin = UiKit.dp(a, 8); lp.rightMargin = UiKit.dp(a, 12);
            root.addView(skip, lp);
        }
        skip.bringToFront();
    }
}
