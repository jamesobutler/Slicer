from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import SlicerSignBundleMacOS as signer


ROOT = Path(__file__).resolve().parents[2]


class ReleaseSigningTests(unittest.TestCase):
    @unittest.skipUnless(sys.platform == "darwin", "requires macOS CMake options")
    def test_application_signing_options_do_not_require_a_package_format(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "SlicerReadApplicationProperties.cmake").write_text(
                "function(SlicerReadApplicationProperties)\n"
                "  set(application_properties \"\" PARENT_SCOPE)\n"
                "endfunction()\n")
            entitlements = root / "entitlements.plist"
            entitlements.write_text("<plist/>")
            script = root / "options.cmake"
            script.write_text(f"""
cmake_minimum_required(VERSION 3.28)
set(CMAKE_MODULE_PATH "{root}")
set(Slicer_SOURCE_DIR "{ROOT}")
set(Slicer_MAIN_PROJECT_APPLICATION_NAME Custom)
function(mark_as_superbuild)
endfunction()
include("{ROOT}/CMake/SlicerApplicationOptions.cmake")
""")
            def configure(*args):
                return subprocess.run(["cmake", *args, "-P", str(script)], text=True, capture_output=True)

            self.assertEqual(configure().returncode, 0)
            result = configure("-DSlicer_CPACK_MACOS_APP_SIGNING_IDENTITY=Developer ID")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("requires an application identity", result.stderr)
            result = configure("-DSlicer_CPACK_MACOS_APP_SIGNING_IDENTITY=Developer ID",
                               f"-DSlicer_CPACK_MACOS_APP_ENTITLEMENTS={entitlements}")
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_rejects_ignored_code_in_nested_bundle(self):
        with tempfile.TemporaryDirectory() as directory:
            app = Path(directory) / "App.app"
            framework = app / "Contents" / "Frameworks" / "Vendor.framework"
            framework.mkdir(parents=True)
            (framework / "Vendor").write_bytes(b"\xcf\xfa\xed\xfe")
            with patch.object(signer, "sign") as sign:
                self.assertFalse(signer.sign_release_bundle(str(app), "identity", "entitlements.plist",
                                                            [re.compile("Vendor")]))
            sign.assert_not_called()

    def test_nested_failure_does_not_sign_outer_bundle(self):
        with tempfile.TemporaryDirectory() as directory:
            app = Path(directory) / "App.app"
            helper = app / "Contents" / "Helper.app"
            helper.mkdir(parents=True)
            calls = []

            def sign(path, identity, extra_args):
                calls.append(path)
                return path, 1, "failed"

            with patch.object(signer, "sign", side_effect=sign):
                self.assertFalse(signer.sign_release_bundle(str(app), "identity", "entitlements.plist"))
            self.assertEqual(calls, [str(helper)])

    @unittest.skipUnless(sys.platform == "darwin", "requires macOS code signing tools")
    def test_signs_nested_bundle_inside_out(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "main.c"
            source.write_text("int main(void) { return 0; }\n")
            binary = root / "binary"
            subprocess.run(["clang", str(source), "-o", str(binary)], check=True, capture_output=True)
            app = root / "Outer.app"
            helper = app / "Contents" / "Helpers" / "Helper.app"
            for bundle, name in ((app, "Outer"), (helper, "Helper")):
                contents = bundle / "Contents"
                (contents / "MacOS").mkdir(parents=True)
                shutil.copy(binary, contents / "MacOS" / name)
                with (contents / "Info.plist").open("wb") as stream:
                    plistlib.dump({"CFBundleIdentifier": f"org.example.{name}",
                                   "CFBundleName": name, "CFBundleExecutable": name,
                                   "CFBundlePackageType": "APPL", "CFBundleVersion": "1"}, stream)
            framework = app / "Contents" / "Frameworks" / "Example.framework"
            version = framework / "Versions" / "A"
            (version / "Resources").mkdir(parents=True)
            subprocess.run(["clang", "-dynamiclib", str(source), "-o", str(version / "Example")],
                           check=True, capture_output=True)
            with (version / "Resources" / "Info.plist").open("wb") as stream:
                plistlib.dump({"CFBundleIdentifier": "org.example.framework",
                               "CFBundleName": "Example", "CFBundleExecutable": "Example",
                               "CFBundlePackageType": "FMWK", "CFBundleVersion": "1"}, stream)
            (framework / "Versions" / "Current").symlink_to("A")
            (framework / "Example").symlink_to("Versions/Current/Example")
            (framework / "Resources").symlink_to("Versions/Current/Resources")
            entitlements = root / "entitlements.plist"
            with entitlements.open("wb") as stream:
                plistlib.dump({}, stream)
            self.assertTrue(signer.sign_release_bundle(str(app), "-", str(entitlements)))


if __name__ == "__main__":
    unittest.main()
