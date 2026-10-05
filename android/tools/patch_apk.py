#!/usr/bin/env python3
"""
Patch Bowling by Jason Belmonte with BowlingPlus (Android).

What it does, given the game's APK(s) and the freshly built BowlingPlus library + dex:
  1. Merge the split APKs into one universal APK (APKEditor), so there is a single file to sign and install.
  2. Rename the game's lib/arm64-v8a/libmain.so to libmain_orig.so and drop our libmain.so in its place.
     Unity's Java loads "main", so our JNI_OnLoad runs; it dlopen()s libmain_orig.so and forwards to Unity.
  3. Add a tiny transparent <activity> (com.bowlingplus.Pickers$Relay) and a <provider> (FileProvider) to the
     manifest, which the menu uses for the photo / file pickers and for sharing QR codes and templates.
  4. zipalign and sign.

Nothing of the game's own code, assets or other libraries is touched. The Java side rides along inside
libmain.so as an embedded dex (see bin2header.py / Jni.cpp), so no classes.dex is modified.

Tools expected on PATH or given by flag: java, apksigner, zipalign, aapt2; APKEditor.jar via --apkeditor.
"""
import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile

ANDROID_NS = "http://schemas.android.com/apk/res/android"
ET.register_namespace("android", ANDROID_NS)


def run(cmd, **kw):
    print("+ " + " ".join(cmd))
    subprocess.run(cmd, check=True, **kw)


def find_tool(name, override=None):
    if override:
        return override
    p = shutil.which(name)
    if not p:
        sys.exit("error: couldn't find '%s' on PATH (install the Android SDK build-tools, or pass the flag)" % name)
    return p


def merge_splits(apkeditor, inputs, out_apk):
    """Produce one universal APK from either a single apk, several split apks, or an .apks/.xapk/.apkm bundle."""
    tmp = tempfile.mkdtemp(prefix="bp_merge_")
    if len(inputs) == 1 and inputs[0].lower().endswith((".apks", ".xapk", ".apkm", ".zip")):
        # a bundle of splits: unzip, then merge the directory
        bundle_dir = os.path.join(tmp, "bundle")
        os.makedirs(bundle_dir)
        with zipfile.ZipFile(inputs[0]) as z:
            z.extractall(bundle_dir)
        run(["java", "-jar", apkeditor, "m", "-i", bundle_dir, "-o", out_apk, "-f"])
    elif len(inputs) == 1 and _has_all_libs(inputs[0]):
        # already a single complete apk
        shutil.copy(inputs[0], out_apk)
    else:
        # loose split files: put them in a directory and merge
        split_dir = os.path.join(tmp, "splits")
        os.makedirs(split_dir)
        for p in inputs:
            shutil.copy(p, split_dir)
        run(["java", "-jar", apkeditor, "m", "-i", split_dir, "-o", out_apk, "-f"])
    return out_apk


def _has_all_libs(apk):
    try:
        with zipfile.ZipFile(apk) as z:
            names = z.namelist()
            return any(n.endswith("/libmain.so") for n in names) and any(n.endswith("/libil2cpp.so") for n in names)
    except Exception:
        return False


def read_zip(apk):
    with zipfile.ZipFile(apk) as z:
        return {i.filename: (z.read(i.filename), i) for i in z.infolist()}


def swap_library(entries, our_lib):
    """Rename every libmain.so to libmain_orig.so and insert ours. arm64 is what the bundle ships."""
    our = open(our_lib, "rb").read()
    found = False
    for name in list(entries):
        if name.endswith("/libmain.so"):
            data, info = entries.pop(name)
            orig = name[: -len("libmain.so")] + "libmain_orig.so"
            entries[orig] = (data, _clone_info(info, orig))
            entries[name] = (our, _clone_info(info, name))   # keep STORED + alignment
            found = True
            print("  swapped %s (game -> libmain_orig.so, BowlingPlus -> libmain.so)" % name)
    if not found:
        sys.exit("error: no lib/*/libmain.so in the APK - is this the right game, and arm64?")
    return entries


def _clone_info(src, name):
    zi = zipfile.ZipInfo(name, date_time=src.date_time)
    zi.compress_type = zipfile.ZIP_STORED            # native libs: stored so they can be mmap'd (extractNativeLibs=false)
    zi.external_attr = src.external_attr
    zi.create_system = src.create_system
    return zi


def patch_manifest(entries, aapt2, pkg):
    """Add the Relay activity and the FileProvider. Done on the binary manifest via aapt2 by rebuilding just
    that entry is hard; instead we decode/encode with APKEditor-style XML is overkill, so we edit the binary
    AXML directly using aapt2's manifest from a tiny overlay is not available. We instead inject at install
    time is not possible either - so we patch the binary XML with a minimal editor."""
    # This is handled by manifest_patch.patch_axml (kept separate for clarity/testing).
    from manifest_patch import add_components
    raw, info = entries["AndroidManifest.xml"]
    new = add_components(raw, pkg)
    entries["AndroidManifest.xml"] = (new, info)
    return entries


def write_zip(entries, out_apk):
    order = sorted(entries, key=lambda n: (0 if n.endswith(".so") else 1, n))
    with zipfile.ZipFile(out_apk, "w") as z:
        for name in order:
            data, info = entries[name]
            zi = zipfile.ZipInfo(name, date_time=info.date_time if hasattr(info, "date_time") else (1981, 1, 1, 1, 1, 1))
            zi.compress_type = zipfile.ZIP_STORED if name.endswith(".so") or name == "resources.arsc" else zipfile.ZIP_DEFLATED
            zi.external_attr = getattr(info, "external_attr", 0)
            z.writestr(zi, data)


def main():
    ap = argparse.ArgumentParser(description="Patch Bowling by Jason Belmonte with BowlingPlus.")
    ap.add_argument("inputs", nargs="+", help="the game: one .apk, several split .apk files, or an .apks/.xapk bundle")
    ap.add_argument("--lib", required=True, help="the built BowlingPlus libmain.so (arm64-v8a)")
    ap.add_argument("--out", required=True, help="output patched APK")
    ap.add_argument("--keystore", help="keystore to sign with (a debug keystore is made if omitted)")
    ap.add_argument("--ks-pass", default="android")
    ap.add_argument("--ks-alias", default="bowlingplus")
    ap.add_argument("--apkeditor", default=os.environ.get("APKEDITOR"), help="path to APKEditor.jar")
    ap.add_argument("--apksigner", default=None)
    ap.add_argument("--zipalign", default=None)
    ap.add_argument("--aapt2", default=None)
    args = ap.parse_args()

    if not args.apkeditor and len(args.inputs) == 1 and _has_all_libs(args.inputs[0]):
        args.apkeditor = "unused"
    if not args.apkeditor:
        sys.exit("error: --apkeditor APKEditor.jar is required to merge split APKs")

    pkg = "studio.wannaplay.bowlingjb"
    work = tempfile.mkdtemp(prefix="bp_patch_")
    merged = os.path.join(work, "merged.apk")
    merge_splits(args.apkeditor, args.inputs, merged)

    print("reading merged APK...")
    entries = read_zip(merged)
    entries = swap_library(entries, args.lib)
    entries = patch_manifest(entries, find_tool("aapt2", args.aapt2), pkg)

    unsigned = os.path.join(work, "unsigned.apk")
    write_zip(entries, unsigned)

    aligned = os.path.join(work, "aligned.apk")
    zipalign = find_tool("zipalign", args.zipalign)
    run([zipalign, "-p", "-f", "4", unsigned, aligned])

    # sign
    ks = args.keystore
    if not ks:
        ks = os.path.join(work, "debug.keystore")
        keytool = os.path.join(os.path.dirname(find_tool("apksigner", args.apksigner)), "..", "..", "bin", "keytool")
        keytool = shutil.which("keytool") or "keytool"
        run([keytool, "-genkeypair", "-keystore", ks, "-storepass", args.ks_pass, "-keypass", args.ks_pass,
             "-alias", args.ks_alias, "-keyalg", "RSA", "-keysize", "2048", "-validity", "10000",
             "-dname", "CN=BowlingPlus"])

    apksigner = find_tool("apksigner", args.apksigner)
    run([apksigner, "sign", "--ks", ks, "--ks-pass", "pass:" + args.ks_pass, "--ks-key-alias", args.ks_alias,
         "--key-pass", "pass:" + args.ks_pass, "--out", args.out, aligned])
    run([apksigner, "verify", "--verbose", args.out])
    print("\ndone: %s" % args.out)
    print("Install: uninstall the Play Store copy first (different signature), then install this APK.")


if __name__ == "__main__":
    main()
