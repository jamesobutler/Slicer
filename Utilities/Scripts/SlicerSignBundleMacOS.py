"""Ad-hoc (or identity) code-sign a macOS application bundle, inside-out.

``install_name_tool`` invalidates the code signature of every Mach-O file it
rewrites during bundle fixup. On Apple Silicon the kernel refuses to map a page
whose signature does not match the binary, so an unsigned or stale-signed file
is killed at load time with EXC_BAD_ACCESS ("Code Signature Invalid").

``codesign --deep`` is unreliable on a bundle this large and deeply nested (the
application's CFBundleExecutable is a small bootstrap, so the real application
binary and every module, plugin and framework are *nested* code): it can leave
individual binaries with an invalid signature, which then fault at load time.

Signing inside-out -- every Mach-O signed on its own, deepest path first, and
the enclosing .app signed last -- avoids that: each file is sealed
independently and the umbrella signature seals the directory tree on top.

Only the ad-hoc identity ("-") is used by default, which is enough for the
binary to load on the machine it was built for and for internal testing (users
still clear the quarantine attribute manually). Use --identity together with
--hardened-runtime and --entitlements for distribution/notarization.
"""

import argparse
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

# Mach-O magic numbers (thin and fat, both byte orders).
_MACHO_MAGIC = {
    b"\xcf\xfa\xed\xfe",  # 64-bit LE
    b"\xce\xfa\xed\xfe",  # 32-bit LE
    b"\xfe\xed\xfa\xcf",  # 64-bit BE
    b"\xfe\xed\xfa\xce",  # 32-bit BE
    b"\xca\xfe\xba\xbe",  # fat BE
    b"\xbe\xba\xfe\xca",  # fat LE
}


def is_macho(path):
    try:
        if os.path.islink(path) or not os.path.isfile(path):
            return False
        with open(path, "rb") as handle:
            return handle.read(4) in _MACHO_MAGIC
    except OSError:
        return False


def find_machos(root, ignored=()):
    """Return (paths, skipped): every Mach-O under root, split by whether its
    basename matches one of the ``ignored`` regexes -- e.g. a vendor library
    that ships with its own valid signature and must not be re-signed.
    """
    paths = []
    skipped = []
    for directory, _dirs, files in os.walk(root):
        for name in files:
            path = os.path.join(directory, name)
            if not is_macho(path):
                continue
            if any(pattern.search(name) for pattern in ignored):
                skipped.append(path)
            else:
                paths.append(path)
    return paths, skipped


def sign(path, identity, extra_args=()):
    result = subprocess.run(
        ["codesign", "--force", "--sign", identity, *extra_args, path],
        capture_output=True,
        text=True,
        check=False,
    )
    return path, result.returncode, result.stderr.strip()


def sign_bundle(app, identity, ignored=()):
    machos, skipped = find_machos(app, ignored)
    # Deepest paths first so nested code is sealed before its container.
    machos.sort(key=lambda path: path.count(os.sep), reverse=True)

    failures = []
    with ThreadPoolExecutor(max_workers=os.cpu_count()) as executor:
        for path, code, error in executor.map(lambda p: sign(p, identity), machos):
            if code != 0:
                failures.append((path, error))
    print("SlicerSignBundleMacOS: signed %d Mach-O files (%d failures)"
          % (len(machos), len(failures)))
    for path, error in failures[:20]:
        print(f"  failed: {path}: {error}")
    if skipped:
        print("SlicerSignBundleMacOS: ignored %d Mach-O file(s), left unsigned:" % len(skipped))
        for path in skipped:
            print("  ignored:", path)

    # Seal the application bundle itself last.
    result = subprocess.run(
        ["codesign", "--force", "--sign", identity, app],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        print("SlicerSignBundleMacOS: signing the application bundle failed:\n"
              + result.stderr, file=sys.stderr)
    return len(failures) == 0 and result.returncode == 0


def sign_release_bundle(app, identity, entitlements, ignored=()):
    machos, skipped = find_machos(app, ignored)
    if skipped:
        print("SlicerSignBundleMacOS: preserving %d previously signed Mach-O file(s)" % len(skipped))
    nested_bundles = []
    for directory, dirs, _files in os.walk(app):
        for name in dirs:
            path = os.path.join(directory, name)
            if not os.path.islink(path) and name.endswith((".app", ".framework", ".xpc", ".appex", ".bundle", ".plugin")):
                nested_bundles.append(path)

    for path in skipped:
        if any(os.path.commonpath((bundle, path)) == bundle for bundle in nested_bundles):
            print(f"SlicerSignBundleMacOS: cannot preserve ignored code inside a nested bundle: {path}",
                  file=sys.stderr)
            return False

    paths = machos + nested_bundles
    extra_args = ("--timestamp", "--options", "runtime", "--entitlements", entitlements)
    depths = sorted({path.count(os.sep) for path in paths}, reverse=True)
    with ThreadPoolExecutor(max_workers=os.cpu_count()) as executor:
        for depth in depths:
            current = [path for path in paths if path.count(os.sep) == depth]
            failures = [(path, error) for path, code, error in
                        executor.map(lambda path: sign(path, identity, extra_args), current) if code != 0]
            if failures:
                for path, error in failures:
                    print(f"SlicerSignBundleMacOS: failed to sign {path}: {error}", file=sys.stderr)
                return False
    path, code, error = sign(app, identity, extra_args)
    if code != 0:
        print(f"SlicerSignBundleMacOS: failed to sign {path}: {error}", file=sys.stderr)
        return False
    result = subprocess.run(["codesign", "--verify", "--deep", "--strict", "--verbose=2", app], check=False)
    return result.returncode == 0


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", required=True, help="Path to the .app bundle")
    parser.add_argument("--identity", default="-",
                        help="codesign identity (default: '-', ad-hoc)")
    parser.add_argument("--hardened-runtime", action="store_true",
                        help="Sign nested code for distribution with a timestamp and hardened runtime")
    parser.add_argument("--entitlements", help="Entitlements plist used for hardened runtime signing")
    parser.add_argument("--ignore-library", action="append", default=[], metavar="REGEX",
                        help="Regex (unanchored search against basename) of Mach-O files to "
                             "leave unsigned -- e.g. a vendor library that ships "
                             "with its own valid signature (repeatable).")
    options = parser.parse_args(argv)
    if not os.path.isdir(options.app):
        print("SlicerSignBundleMacOS: no such bundle: %s" % options.app,
              file=sys.stderr)
        return 1
    if options.hardened_runtime:
        if options.identity == "-" or not options.entitlements or not os.path.isfile(options.entitlements):
            parser.error("--hardened-runtime requires a signing identity and an existing entitlements file")
    elif options.entitlements:
        parser.error("--entitlements requires --hardened-runtime")
    ignored = []
    for pattern in options.ignore_library:
        try:
            ignored.append(re.compile(pattern))
        except re.error as error:
            print("SlicerSignBundleMacOS: invalid --ignore-library regex %r: %s" %
                  (pattern, error), file=sys.stderr)
            return 1
    if options.hardened_runtime:
        return 0 if sign_release_bundle(options.app, options.identity, options.entitlements, ignored) else 1
    return 0 if sign_bundle(options.app, options.identity, ignored) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
