package com.bowlingplus;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.text.InputType;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Switch;
import android.widget.TextView;

// Shared look (BowlingPlus's dark panels, orange accent) and view helpers, so Menu.java and Oil.java read
// like the iOS UIKit code. All overlays are added to the activity's content FrameLayout and sit on top of
// the game; none of the game's own views are touched.
final class UiKit {
    static final int ACCENT = Color.rgb(255, 123, 26);
    static final int YELLOW = Color.rgb(255, 204, 61);
    static final int CARD_BG = Color.argb(247, 20, 23, 28);
    static final int PANEL_BG = Color.argb(14, 255, 255, 255);

    private UiKit() {}

    static int dim(float a) { return Color.argb((int) (a * 255), 255, 255, 255); }
    static int dp(Context c, float v) { return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, c.getResources().getDisplayMetrics())); }

    static FrameLayout content(Activity act) { return act == null ? null : act.findViewById(android.R.id.content); }

    static GradientDrawable round(int color, float radius, Context c) {
        GradientDrawable g = new GradientDrawable();
        g.setColor(color);
        g.setCornerRadius(dp(c, radius));
        return g;
    }

    static GradientDrawable roundStroke(int color, float radius, int strokeColor, float strokeW, Context c) {
        GradientDrawable g = round(color, radius, c);
        g.setStroke(Math.max(1, dp(c, strokeW)), strokeColor);
        return g;
    }

    static TextView label(Context c, String text, float sp, int color, boolean bold) {
        TextView t = new TextView(c);
        t.setText(text);
        t.setTextSize(sp);
        t.setTextColor(color);
        t.setTypeface(Typeface.DEFAULT, bold ? Typeface.BOLD : Typeface.NORMAL);
        return t;
    }

    static Switch sw(Context c, boolean on) {
        Switch s = new Switch(c);
        s.setChecked(on);
        s.setThumbTintList(android.content.res.ColorStateList.valueOf(Color.WHITE));
        s.setTrackTintList(new android.content.res.ColorStateList(
                new int[][]{ new int[]{ android.R.attr.state_checked }, new int[]{} },
                new int[]{ ACCENT, dim(0.3f) }));
        return s;
    }

    static Button button(Context c, String title, boolean filled, boolean small) {
        Button b = new Button(c);
        b.setAllCaps(false);
        b.setText(title);
        b.setTextSize(small ? 13 : 15);
        b.setTypeface(Typeface.DEFAULT_BOLD);
        b.setTextColor(filled ? Color.rgb(20, 20, 20) : Color.WHITE);
        b.setBackground(round(filled ? ACCENT : dim(0.12f), 10, c));
        b.setStateListAnimator(null);
        int padV = dp(c, small ? 8 : 10), padH = dp(c, small ? 8 : 14);
        b.setPadding(padH, padV, padH, padV);
        b.setMinHeight(0);
        b.setMinimumHeight(0);
        return b;
    }

    static LinearLayout row(Context c, boolean horizontal) {
        LinearLayout l = new LinearLayout(c);
        l.setOrientation(horizontal ? LinearLayout.HORIZONTAL : LinearLayout.VERTICAL);
        if (horizontal) l.setGravity(Gravity.CENTER_VERTICAL);
        return l;
    }

    static LinearLayout.LayoutParams lp(int w, int h) { return new LinearLayout.LayoutParams(w, h); }
    static LinearLayout.LayoutParams lpWeight(float weight) {
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT);
        p.weight = weight;
        return p;
    }

    static EditText field(Context c, String hint) {
        EditText f = new EditText(c);
        f.setHint(hint);
        f.setHintTextColor(dim(0.35f));
        f.setTextColor(Color.WHITE);
        f.setTextSize(15);
        f.setSingleLine(true);
        f.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        f.setBackground(round(dim(0.08f), 10, c));
        f.setPadding(dp(c, 12), dp(c, 10), dp(c, 12), dp(c, 10));
        return f;
    }

    static ImageView image(Context c, byte[] png, float size) {
        ImageView iv = new ImageView(c);
        if (png != null) iv.setImageBitmap(android.graphics.BitmapFactory.decodeByteArray(png, 0, png.length));
        iv.setAdjustViewBounds(true);
        iv.setLayoutParams(new LinearLayout.LayoutParams(dp(c, size), dp(c, size)));
        return iv;
    }

    // A dimmed full-screen overlay holding a centered card; tap outside to dismiss.
    interface OnDismiss { void run(); }

    // How many BowlingPlus panels are on screen right now. Counted by the panel itself when it is attached to /
    // removed from a window, so it is right however a panel gets closed. The native side caps the game's frame
    // rate while this is above 0 (the lane behind a panel isn't being played), and the on-screen menu button
    // steps out of the way.
    private static int overlayCount;
    static boolean overlayOpen() { return overlayCount > 0; }

    private static void overlayChanged(int delta) {
        boolean before = overlayCount > 0;
        overlayCount = Math.max(0, overlayCount + delta);
        if (before != (overlayCount > 0)) {
            try { N.call("overlay", overlayCount > 0 ? "1" : "0"); } catch (Throwable ignored) {}
        }
        BP.UI.post(() -> { try { MenuButton.refresh(); } catch (Throwable ignored) {} });   // posted: we may be inside removeView
    }

    private static final class Overlay extends FrameLayout {
        private boolean counted;
        Overlay(Context c) { super(c); }
        @Override protected void onAttachedToWindow() { super.onAttachedToWindow(); if (!counted) { counted = true; overlayChanged(+1); } }
        @Override protected void onDetachedFromWindow() { super.onDetachedFromWindow(); if (counted) { counted = false; overlayChanged(-1); } }
    }

    static FrameLayout overlay(Activity act, final OnDismiss onOutside) {
        final FrameLayout ov = new Overlay(act);
        ov.setBackgroundColor(Color.argb(115, 0, 0, 0));
        ov.setLayoutParams(new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        ov.setClickable(true);
        if (onOutside != null) ov.setOnClickListener(v -> onOutside.run());
        return ov;
    }

    // Build a dimmed overlay with the card inside it, tap-outside dismisses, and add it to the activity.
    // Returns the overlay (remove it to dismiss). The card scrolls and is height-capped.
    static FrameLayout present(Activity act, LinearLayout content, int gravity, int maxWidthDp, OnDismiss onOutside) {
        final FrameLayout[] ovh = new FrameLayout[1];
        FrameLayout ov = overlay(act, () -> {
            FrameLayout root = content(act);
            if (root != null && ovh[0] != null) root.removeView(ovh[0]);
            if (onOutside != null) onOutside.run();
        });
        ovh[0] = ov;
        ov.addView(cardScroll(act, content, gravity, maxWidthDp));
        FrameLayout root = content(act);
        if (root != null) root.addView(ov);
        return ov;
    }

    static ScrollView cardScroll(Activity act, LinearLayout content, int gravity, int maxWidthDp) {
        Context c = act;
        // callers give only a vertical position (TOP / CENTER / BOTTOM); with no horizontal part Android puts the
        // card against the left edge, so every card is centered across the screen unless asked otherwise
        if ((gravity & Gravity.HORIZONTAL_GRAVITY_MASK) == 0) gravity |= Gravity.CENTER_HORIZONTAL;
        LinearLayout card = new LinearLayout(c);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setBackground(roundStroke(CARD_BG, 20, dim(0.08f), 1, c));
        card.setClickable(true);   // taps on the card don't dismiss
        // The card used to hold a ScrollView of its own inside the height-capped ScrollView below. The inner one
        // never scrolled (it was given unlimited height) but still sat in the middle of every touch and layout
        // pass. The outer one does all the scrolling, so the content goes straight into the card.
        card.addView(content, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        int pad = dp(c, 18);
        content.setPadding(pad, pad, pad, pad);
        int w = Math.min(dp(c, maxWidthDp), act.getResources().getDisplayMetrics().widthPixels - dp(c, 24));
        FrameLayout.LayoutParams clp = new FrameLayout.LayoutParams(w, ViewGroup.LayoutParams.WRAP_CONTENT);
        clp.gravity = gravity;
        clp.topMargin = clp.bottomMargin = dp(c, 12);
        int maxH = act.getResources().getDisplayMetrics().heightPixels - dp(c, 40);
        card.setLayoutParams(clp);
        card.setTag(maxH);
        return wrapMaxHeight(card, maxH);
    }

    // card whose height is capped (so a long menu scrolls)
    private static ScrollView wrapMaxHeight(final LinearLayout card, final int maxH) {
        ScrollView holder = new ScrollView(card.getContext()) {
            @Override protected void onMeasure(int w, int h) {
                super.onMeasure(w, View.MeasureSpec.makeMeasureSpec(maxH, View.MeasureSpec.AT_MOST));
            }
        };
        holder.setLayoutParams(card.getLayoutParams());
        card.setLayoutParams(new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        holder.addView(card);
        return holder;
    }

    static void toast(Activity act, String text) {
        if (act == null) return;
        android.widget.Toast.makeText(act, text, android.widget.Toast.LENGTH_SHORT).show();
    }

    static void openUrl(Activity act, String url) {
        try { act.startActivity(new android.content.Intent(android.content.Intent.ACTION_VIEW, android.net.Uri.parse(url))); } catch (Throwable ignored) {}
    }
}
