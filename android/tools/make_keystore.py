#!/usr/bin/env python3
"""
Makes the signing key for BowlingPlus builds, and writes the four values GitHub needs, ready to copy.

Why: Android only installs an update over an app signed with the SAME key. Without your own key every build
gets a new random one, so you'd have to uninstall first (which erases the game's data on the phone).
Make this key ONCE and keep using it.

    python3 make_keystore.py            (Windows: py make_keystore.py)

Run it in a folder OUTSIDE the repo (like your Desktop). It needs Java's `keytool` (comes with any JDK).
It writes two files in the current folder:
    bowlingplus.keystore       the key itself. BACK THIS UP. Lose it and you can't update an installed copy.
    bowlingplus-secrets.txt    the four GitHub secrets (open it in Notepad and copy each one)
Never commit or share either file.
"""
import base64
import os
import secrets
import shutil
import subprocess
import sys

ALIAS = "bowlingplus"


def main():
    keytool = shutil.which("keytool")
    if not keytool:
        sys.exit("keytool not found. Install a JDK (https://adoptium.net, any version 17 or newer), open a new\n"
                 "terminal and run this again. No JDK on this machine? Use GitHub Codespaces instead: see\n"
                 "android/README.md, 'Keeping one signing key'.")
    ks = os.path.abspath("bowlingplus.keystore")
    out = os.path.abspath("bowlingplus-secrets.txt")
    if os.path.exists(ks) or os.path.exists(out):
        sys.exit("bowlingplus.keystore / bowlingplus-secrets.txt already exist here. Not overwriting them: "
                 "that would replace your key. Move them away first, or use the ones you have.")
    password = secrets.token_urlsafe(18)
    subprocess.run([keytool, "-genkeypair", "-keystore", ks, "-storetype", "PKCS12", "-storepass", password,
                    "-alias", ALIAS, "-keyalg", "RSA", "-keysize", "2048", "-validity", "10000",
                    "-dname", "CN=BowlingPlus"], check=True)
    b64 = base64.b64encode(open(ks, "rb").read()).decode()
    with open(out, "w") as f:
        f.write("Add these four as repository secrets: GitHub repo > Settings > Secrets and variables > Actions >\n"
                "New repository secret. The NAME is the line starting with ###, the VALUE is the line under it.\n\n")
        for name, value in (("ANDROID_KEYSTORE_B64", b64), ("ANDROID_KEYSTORE_PASSWORD", password),
                            ("ANDROID_KEY_ALIAS", ALIAS), ("ANDROID_KEY_PASSWORD", password)):
            f.write("### %s\n%s\n\n" % (name, value))
    print("\nDone. Two files were written:\n  %s   <- back this up somewhere safe\n  %s   <- copy the 4 secrets from it\n" % (ks, out))
    print("Then add the four secrets to GitHub (android/README.md has the click-by-click steps).")


if __name__ == "__main__":
    main()
