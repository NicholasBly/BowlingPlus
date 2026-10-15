#!/usr/bin/env python3
"""
Upload a signed-ready IPA to Signulous for signing, from CI (the iOS workflow, .github/workflows/build.yml).

Taken from GlowRedux's scripts/signulous_upload.py (the same upload the TweakDash desktop app does): a multipart
POST to the Signulous sign-app endpoint with the UDID, the IPA, and the "Customize Uploaded App" fields. Signulous
signs the app against the UDID and the app then appears in your Signulous account to install on the device.

The UDID is read from an environment variable (never argv), and is masked in
all output, so it is not exposed in CI logs.

Usage:
  SIGNULOUS_UDID=... signulous_upload.py --ipa BowlingPlus.ipa --name Bowling \
      --version 1.907 --bundle studio.wannaplay.bowlingjb [--push] [--duplicate] \
      [--match-provisioning] [--exclude-provisioning] [--liquid-glass]

Exit code 0 on success, non-zero otherwise.
"""
import argparse
import json
import os
import re
import sys
import urllib.error
import urllib.request

SIGN_URL = "https://api2.signulous.com/sign-app"


def mask_udid(udid: str) -> str:
    return udid if len(udid) <= 4 else "..." + udid[-4:]


def normalize_udid(text: str):
    text = text.strip()
    new_style = (len(text) == 25 and text[8] == "-"
                 and re.fullmatch(r"[0-9A-Fa-f]{8}", text[:8])
                 and re.fullmatch(r"[0-9A-Fa-f]{16}", text[9:]))
    old_style = len(text) == 40 and re.fullmatch(r"[0-9A-Fa-f]{40}", text)
    if not (new_style or old_style):
        return None
    return text.lower()


def build_multipart(udid, ipa_bytes, ipa_name, opts):
    boundary = "----BowlingPlusFormBoundary" + os.urandom(12).hex()
    b = boundary.encode()
    CRLF = b"\r\n"
    out = bytearray()

    def field(name, value):
        out.extend(b"--" + b + CRLF)
        out.extend(b'Content-Disposition: form-data; name="' + name.encode() + b'"' + CRLF + CRLF)
        out.extend(value.encode() + CRLF)

    def file_field(name, filename, content, ctype=b"application/octet-stream"):
        out.extend(b"--" + b + CRLF)
        out.extend(b'Content-Disposition: form-data; name="' + name.encode()
                   + b'"; filename="' + filename.encode() + b'"' + CRLF)
        out.extend(b"Content-Type: " + ctype + CRLF + CRLF)
        out.extend(content + CRLF)

    # Exact order the Signulous upload form sends (from signulous.cpp).
    field("udid", udid)
    file_field("app", ipa_name, ipa_bytes)
    field("app", "")                      # the (empty) direct-URL box
    field("customName", opts["name"])
    field("customVersion", opts["version"])
    file_field("customIcon", "", b"")     # no custom icon
    field("customBundleIdentifier", opts["bundle"])
    if opts["duplicate"]:            field("useUniqueIdentifiers", "on")
    if opts["push"]:                 field("enablePushNotifications", "on")
    if opts["match_provisioning"]:   field("matchProvisioningIdentifier", "on")
    if opts["exclude_provisioning"]: field("excludeProvisioning", "on")
    if opts["liquid_glass"]:         field("enableLiquidGlass", "on")
    out.extend(b"--" + b + b"--" + CRLF)

    return bytes(out), "multipart/form-data; boundary=" + boundary


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--ipa", required=True)
    p.add_argument("--name", required=True)
    p.add_argument("--version", required=True)
    p.add_argument("--bundle", required=True)
    p.add_argument("--push", action="store_true")
    p.add_argument("--duplicate", action="store_true")
    p.add_argument("--match-provisioning", action="store_true")
    p.add_argument("--exclude-provisioning", action="store_true")
    p.add_argument("--liquid-glass", action="store_true")
    args = p.parse_args()

    raw = os.environ.get("SIGNULOUS_UDID", "")
    udid = normalize_udid(raw)
    if not udid:
        print("::error::SIGNULOUS_UDID is missing or not a valid UDID.")
        return 2
    masked = mask_udid(udid)

    try:
        with open(args.ipa, "rb") as f:
            ipa_bytes = f.read()
    except OSError as e:
        print(f"::error::Can't read IPA: {e}")
        return 1

    opts = {
        "name": args.name, "version": args.version, "bundle": args.bundle,
        "push": args.push, "duplicate": args.duplicate,
        "match_provisioning": args.match_provisioning,
        "exclude_provisioning": args.exclude_provisioning,
        "liquid_glass": args.liquid_glass,
    }
    body, content_type = build_multipart(udid, ipa_bytes, os.path.basename(args.ipa), opts)

    print(f"Uploading {len(ipa_bytes)//1024//1024} MB to Signulous for UDID {masked} "
          f"(name={args.name}, version={args.version}, push={args.push}) ...")

    req = urllib.request.Request(SIGN_URL, data=body, method="POST", headers={
        "Content-Type": content_type,
        "Accept": "*/*",
        "Origin": "https://www.signulous.com",
        "Referer": "https://www.signulous.com/",
    })

    def clean(text: str) -> str:
        # Never let the UDID appear in logs, even if echoed back.
        return text.replace(udid, masked).replace(udid.upper(), masked)

    try:
        # Signing can take a while for a big app; give it 15 minutes.
        with urllib.request.urlopen(req, timeout=15 * 60) as resp:
            status = resp.status
            text = resp.read().decode("utf-8", "replace")
    except urllib.error.HTTPError as e:
        text = e.read().decode("utf-8", "replace")
        if e.code == 413:
            print("::error::Signulous says the IPA is too big (HTTP 413).")
            return 1
        print(f"::error::Signulous returned HTTP {e.code}: {clean(text)[:200]}")
        return 1
    except Exception as e:
        print(f"::error::Couldn't reach Signulous: {clean(str(e))}")
        return 1

    try:
        j = json.loads(text)
    except json.JSONDecodeError:
        print(f"::error::Signulous sent an unexpected reply: {clean(text)[:200]}")
        return 1

    if isinstance(j, dict) and j.get("status") == "success" and j.get("message"):
        print(f"Signulous signed it: {clean(str(j['message']))}")
        print("Open the Signulous app on your device to install it.")
        return 0
    msg = j.get("message") if isinstance(j, dict) else None
    print(f"::error::Signulous couldn't sign the app: {clean(str(msg)) if msg else clean(text)[:200]}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
