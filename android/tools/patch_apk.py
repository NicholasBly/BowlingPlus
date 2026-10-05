#!/usr/bin/env python3
"""
Patch Bowling by Jason Belmonte with BowlingPlus (Android).

Given the game (one .apk, several split .apk files, or an .apks / .xapk / .apkm bundle) and the two build
outputs (libmain.so and bowlingplus.dex), this makes one installable APK:

  1. Merges the split APKs into one. The game's Play bundle only splits off the native libraries
     (split_config.arm64_v8a.apk), which this script merges itself. Bundles that also split resources
     (density / language splits) need APKEditor (--apkeditor).
  2. Renames the game's lib/arm64-v8a/libmain.so to libmain_orig.so and puts BowlingPlus's libmain.so in its
     place. Unity's Java loads "main", so our JNI_OnLoad runs; it dlopen()s libmain_orig.so and hands over.
  3. Adds BowlingPlus's Java side as the next classesN.dex, so the app's own class loader finds it.
  4. Edits the manifest (tools/axml.py): removes the "this app needs its splits" markers, adds the picker relay
     activity and the small file provider the menu uses.
  5. Keeps every other entry exactly as it was (stored entries stay stored: Unity reads its asset bundles
     straight out of the APK), drops the old signature, zipaligns and signs.

Tools: zipalign and apksigner from the Android build-tools, keytool from the JDK (java for APKEditor only).
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import axml  # noqa: E402

PKG = "studio.wannaplay.bowlingjb"
ABI = "arm64-v8a"

# framework attribute ids (android.R.attr) and the one theme we use (android.R.style)
R_THEME, R_NAME, R_EXPORTED = 0x01010000, 0x01010003, 0x01010010
R_EXCLUDE_FROM_RECENTS, R_AUTHORITIES, R_GRANT_URI = 0x01010017, 0x01010018, 0x0101001b
R_CONFIG_CHANGES = 0x0101001f
THEME_TRANSLUCENT_NOTITLE = 0x01030010   # @android:style/Theme.Translucent.NoTitleBar

SIGNATURE_FILE = re.compile(r"^META-INF/([^/]+\.(SF|RSA|DSA|EC)|MANIFEST\.MF)$", re.I)
DROP = {"stamp-cert-sha256"}   # Play's source stamp: it would no longer match


def run(cmd, **kw):
    print("+ " + " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True, **kw)


def find_tool(name, override=None):
    p = override or shutil.which(name)
    if not p:
        sys.exit("error: couldn't find '%s' (install the Android SDK build-tools, or pass its path)" % name)
    return p


# ---------------------------------------------------------------------------
# reading the input
# ---------------------------------------------------------------------------
def collect_apks(inputs, work):
    """Every .apk from the inputs, as paths on disk. A zip with an AndroidManifest.xml is an APK; any other
    zip (.apks / .xapk / .apkm, whatever it's named) is a bundle, and the .apk files inside it are used."""
    apks = []
    for p in inputs:
        try:
            z = zipfile.ZipFile(p)
        except zipfile.BadZipFile:
            sys.exit("error: %s isn't an APK or an APK bundle (not a zip file - a download page instead of the file?)" % p)
        with z:
            names = z.namelist()
            if "AndroidManifest.xml" in names:
                apks.append(p)
                continue
            d = tempfile.mkdtemp(prefix="bundle_", dir=work)
            for n in names:
                if n.lower().endswith(".apk") and "/" not in n.strip("/"):
                    z.extract(n, d)
                    apks.append(os.path.join(d, n))
    if not apks:
        sys.exit("error: no .apk found in the input")
    return apks


def manifest_info(apk):
    """(split name or None for the base APK, package name)"""
    with zipfile.ZipFile(apk) as z:
        doc = axml.decode(z.read("AndroidManifest.xml"))
    m = doc.elements("manifest")[0]
    sp, pkg = m.attr("split", ns=None), m.attr("package", ns=None)
    return (sp.value if sp else None), (pkg.value if pkg else None)


def only_libs(apk):
    with zipfile.ZipFile(apk) as z:
        return all(n.startswith(("lib/", "META-INF/")) or n in ("AndroidManifest.xml", "stamp-cert-sha256")
                   for n in z.namelist())


def read_entries(apk):
    """name -> (bytes, ZipInfo), in the APK's order."""
    out = {}
    with zipfile.ZipFile(apk) as z:
        for i in z.infolist():
            if not i.is_dir():
                out[i.filename] = (z.read(i), i)
    return out


def merged_entries(apks, apkeditor, work):
    base, splits = None, []
    for a in apks:
        sp, pkg = manifest_info(a)
        if pkg and pkg != PKG:
            print("warning: %s is package %s, not %s" % (os.path.basename(a), pkg, PKG))
        if sp is None:
            base = a
        else:
            splits.append((sp, a))
    if base is None:
        sys.exit("error: no base APK in the input (only splits)")
    print("base: %s | splits: %s" % (os.path.basename(base), ", ".join(s for s, _ in splits) or "none"))

    if any(not only_libs(a) for _, a in splits):
        # resource splits: only APKEditor merges resources.arsc properly
        if not apkeditor:
            sys.exit("error: this bundle splits resources too; pass --apkeditor APKEditor.jar to merge it")
        d = tempfile.mkdtemp(prefix="splits_", dir=work)
        for a in apks:
            shutil.copy(a, d)
        merged = os.path.join(work, "merged.apk")
        run(["java", "-jar", apkeditor, "m", "-i", d, "-o", merged, "-f"])
        return read_entries(merged)

    entries = read_entries(base)
    for _, a in splits:
        for name, v in read_entries(a).items():
            if name.startswith("lib/"):
                entries[name] = v
    return entries


# ---------------------------------------------------------------------------
# the changes
# ---------------------------------------------------------------------------
def swap_library(entries, our_lib):
    orig = "lib/%s/libmain.so" % ABI
    renamed = "lib/%s/libmain_orig.so" % ABI
    if renamed in entries:
        sys.exit("error: this APK is already patched (it has libmain_orig.so). Start from the original game.")
    if orig not in entries:
        abis = sorted({n.split("/")[1] for n in entries if n.startswith("lib/") and n.count("/") >= 2})
        sys.exit("error: no %s in the APK (it has: %s). BowlingPlus is arm64 only." % (orig, ", ".join(abis) or "no libraries"))
    data, info = entries.pop(orig)
    entries[renamed] = (data, info)
    entries[orig] = (open(our_lib, "rb").read(), info)
    for n in entries:
        if n.startswith("lib/") and n.endswith("/libmain.so") and n != orig:
            print("note: %s left as is (BowlingPlus only runs on arm64)" % n)
    print("  libmain.so -> libmain_orig.so, BowlingPlus's libmain.so added")


def add_dex(entries, dex_path):
    if "classes.dex" not in entries:
        sys.exit("error: the APK has no classes.dex")
    n = 2
    while "classes%d.dex" % n in entries:
        n += 1
    name = "classes%d.dex" % n
    entries[name] = (open(dex_path, "rb").read(), entries["classes.dex"][1])
    print("  BowlingPlus's Java side added as %s" % name)


def A(name, resid, type_, value):
    return axml.Attr(name, type_, value, ns=axml.ANDROID_NS, resid=resid)


def element(name, attrs):
    attrs = sorted(attrs, key=lambda a: a.resid)   # Android reads attributes assuming they're sorted by id
    return [axml.Node("start", name=name, attrs=attrs), axml.Node("end", name=name)]


def patch_manifest(raw):
    doc = axml.decode(raw)
    man = doc.elements("manifest")[0]
    app = doc.elements("application")[0]

    # 1) it's one APK now: drop the split markers (else Android 12+ refuses it as "missing splits", and the
    #    Play libraries may stop the game for missing splits)
    man.attrs = [a for a in man.attrs if a.name not in ("requiredSplitTypes", "splitTypes", "isSplitRequired")]
    app.attrs = [a for a in app.attrs if a.name != "isSplitRequired"]
    drop_meta = {"com.android.vending.splits.required", "com.android.vending.splits", "com.android.stamp.source",
                 "com.android.stamp.type", "com.android.vending.derived.apk.id"}
    for m in list(doc.elements("meta-data")):
        n = m.attr("name")
        if n is not None and n.type == axml.TYPE_STRING and n.value in drop_meta:
            doc.remove_element(m)

    # 2) our two components (skipped if already there)
    names = set()
    for e in doc.nodes:
        a = e.attr("name") if e.kind == "start" else None
        if a is not None and a.type == axml.TYPE_STRING:
            names.add(a.value)
    add = []
    if "com.bowlingplus.Pickers$Relay" not in names:
        game = [e for e in doc.elements("activity") if e.attr("configChanges") is not None]
        cfg = game[0].attr("configChanges").value if game else 0x40003FFF
        add += element("activity", [
            A("name", R_NAME, axml.TYPE_STRING, "com.bowlingplus.Pickers$Relay"),
            A("theme", R_THEME, axml.TYPE_REFERENCE, THEME_TRANSLUCENT_NOTITLE),
            A("exported", R_EXPORTED, axml.TYPE_INT_BOOLEAN, 0),
            A("excludeFromRecents", R_EXCLUDE_FROM_RECENTS, axml.TYPE_INT_BOOLEAN, 0xFFFFFFFF),
            A("configChanges", R_CONFIG_CHANGES, axml.TYPE_INT_HEX, cfg),
        ])
    if "com.bowlingplus.BpFileProvider" not in names:
        add += element("provider", [
            A("name", R_NAME, axml.TYPE_STRING, "com.bowlingplus.BpFileProvider"),
            A("authorities", R_AUTHORITIES, axml.TYPE_STRING, PKG + ".bowlingplus.fileprovider"),
            A("exported", R_EXPORTED, axml.TYPE_INT_BOOLEAN, 0),
            A("grantUriPermissions", R_GRANT_URI, axml.TYPE_INT_BOOLEAN, 0xFFFFFFFF),
        ])
    doc.insert_before_end(app, add)
    out = doc.encode()
    if not axml.same(doc, axml.decode(out)):   # what we wrote must read back the same
        sys.exit("error: the patched manifest doesn't read back the same (axml.py bug)")
    return out


def write_zip(entries, out_apk):
    with zipfile.ZipFile(out_apk, "w") as z:
        for name, (data, info) in entries.items():
            if name in DROP or SIGNATURE_FILE.match(name):
                continue
            zi = zipfile.ZipInfo(name, date_time=info.date_time)
            zi.external_attr = info.external_attr
            # keep how the game stored each entry: Unity reads the stored ones (asset bundles, data files)
            # straight out of the APK. Native libraries and resources.arsc are always stored.
            stored = info.compress_type == zipfile.ZIP_STORED or name.endswith(".so") or name == "resources.arsc"
            zi.compress_type = zipfile.ZIP_STORED if stored else zipfile.ZIP_DEFLATED
            z.writestr(zi, data)


def sign(aligned, out, args, work):
    apksigner = find_tool("apksigner", args.apksigner)
    ks_pass = os.environ.get("BP_KS_PASS") or args.ks_pass
    key_pass = os.environ.get("BP_KEY_PASS") or ks_pass
    ks = args.keystore
    if not ks:
        ks = os.path.join(work, "throwaway.keystore")
        run([find_tool("keytool"), "-genkeypair", "-keystore", ks, "-storepass", ks_pass, "-keypass", key_pass,
             "-alias", args.ks_alias, "-keyalg", "RSA", "-keysize", "2048", "-validity", "10000",
             "-dname", "CN=BowlingPlus"], stdout=subprocess.DEVNULL)
        print("note: signed with a throwaway key. The next build can't install over this one without "
              "uninstalling (which erases the game's data). See android/README.md to keep one key.")
    run([apksigner, "sign", "--ks", ks, "--ks-pass", "pass:" + ks_pass, "--ks-key-alias", args.ks_alias,
         "--key-pass", "pass:" + key_pass, "--out", out, aligned])
    run([apksigner, "verify", out])


def main():
    ap = argparse.ArgumentParser(description="Patch Bowling by Jason Belmonte with BowlingPlus.")
    ap.add_argument("inputs", nargs="+", help="the game: one .apk, split .apk files, or an .apks/.xapk/.apkm bundle")
    ap.add_argument("--lib", required=True, help="BowlingPlus's libmain.so (arm64-v8a)")
    ap.add_argument("--dex", required=True, help="BowlingPlus's Java side (bowlingplus.dex)")
    ap.add_argument("--out", required=True, help="the patched APK to write")
    ap.add_argument("--apkeditor", default=os.environ.get("APKEDITOR"), help="APKEditor.jar (only for bundles with resource splits)")
    ap.add_argument("--keystore", default=os.environ.get("BP_KEYSTORE"), help="keystore to sign with (default: a throwaway key)")
    ap.add_argument("--ks-pass", default="android", help="keystore password (or env BP_KS_PASS)")
    ap.add_argument("--ks-alias", default=os.environ.get("BP_KEY_ALIAS") or "bowlingplus", help="key alias (or env BP_KEY_ALIAS)")
    ap.add_argument("--apksigner")
    ap.add_argument("--zipalign")
    ap.add_argument("--no-sign", action="store_true", help="stop after writing the unsigned, unaligned APK (for testing)")
    args = ap.parse_args()

    work = tempfile.mkdtemp(prefix="bp_patch_")
    try:
        entries = merged_entries(collect_apks(args.inputs, work), args.apkeditor, work)
        print("patching...")
        swap_library(entries, args.lib)
        add_dex(entries, args.dex)
        raw, info = entries["AndroidManifest.xml"]
        entries["AndroidManifest.xml"] = (patch_manifest(raw), info)
        print("  manifest: split markers removed, picker relay + file provider added")

        unsigned = args.out if args.no_sign else os.path.join(work, "unsigned.apk")
        write_zip(entries, unsigned)
        if args.no_sign:
            print("\nwrote %s (unsigned, not aligned)" % unsigned)
            return
        aligned = os.path.join(work, "aligned.apk")
        zipalign = find_tool("zipalign", args.zipalign)
        try:   # -P 16: 16 KB page alignment for the .so files (build-tools 35+); older zipalign only has -p
            run([zipalign, "-P", "16", "-f", "4", unsigned, aligned])
        except subprocess.CalledProcessError:
            run([zipalign, "-p", "-f", "4", unsigned, aligned])
        sign(aligned, args.out, args, work)
        print("\ndone: %s" % args.out)
        print("Install: uninstall the Play Store copy first (different signature), then install this APK.")
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
