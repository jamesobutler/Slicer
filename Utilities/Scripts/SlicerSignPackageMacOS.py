"""Sign and optionally notarize a macOS installer package or disk image."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def notarize_package(package, profile):
    result = subprocess.run([
        "xcrun", "notarytool", "submit", str(package), "--keychain-profile", profile,
        "--wait", "--output-format", "json",
    ], check=False, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"Notarization submission failed:\n{result.stdout}\n{result.stderr}")
    response = json.loads(result.stdout)
    if response.get("status") != "Accepted":
        raise RuntimeError(f"Notarization failed: {result.stdout}")
    subprocess.run(["xcrun", "stapler", "staple", str(package)], check=True)
    subprocess.run(["xcrun", "stapler", "validate", str(package)], check=True)


def sign_package(package, output, identity, notary_profile=None):
    package = Path(package)
    output = Path(output)
    kind = package.suffix.lower()
    if kind not in (".pkg", ".dmg") or output.suffix.lower() != kind:
        raise ValueError("Input and output must have the same .pkg or .dmg extension")
    if not package.is_file():
        raise ValueError(f"Package does not exist: {package}")
    if output.exists() or output.is_symlink():
        raise FileExistsError(f"Output already exists: {output}")
    if not identity or identity == "-":
        raise ValueError("A Developer ID signing identity is required")

    with tempfile.TemporaryDirectory(dir=output.parent, prefix="slicer-sign-") as directory:
        signed = Path(directory) / output.name
        if kind == ".pkg":
            subprocess.run(["productsign", "--sign", identity, "--timestamp", str(package), str(signed)], check=True)
        else:
            shutil.copy2(package, signed)
            subprocess.run(["codesign", "--force", "--sign", identity, "--timestamp", str(signed)], check=True)
        if notary_profile:
            notarize_package(signed, notary_profile)
        os.link(signed, output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", required=True, type=Path, help="Input .pkg or .dmg")
    parser.add_argument("--output", required=True, type=Path, help="New signed output file")
    parser.add_argument("--identity", required=True, help="Developer ID Installer for PKG; Developer ID Application for DMG")
    parser.add_argument("--notary-profile", help="notarytool keychain profile")
    options = parser.parse_args()
    sign_package(options.package, options.output, options.identity, options.notary_profile)


if __name__ == "__main__":
    main()
