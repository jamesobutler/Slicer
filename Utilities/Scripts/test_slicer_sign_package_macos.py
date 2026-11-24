from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import Mock, patch

import SlicerSignPackageMacOS as signer


class PackageSigningTests(unittest.TestCase):
    def test_signing_precedes_notarization_for_each_package_type(self):
        for extension, command in ((".pkg", "productsign"), (".dmg", "codesign")):
            with self.subTest(extension=extension), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                package = root / ("input" + extension)
                package.write_bytes(b"original")
                output = root / ("output" + extension)
                calls = []

                def run(args, **kwargs):
                    calls.append(args)
                    if args[0] == "productsign":
                        Path(args[-1]).write_bytes(b"signed")
                    return Mock(returncode=0, stdout='{"status":"Accepted"}')

                with patch.object(signer.subprocess, "run", side_effect=run):
                    signer.sign_package(package, output, "Developer ID", "profile")
                self.assertEqual([args[0:2] for args in calls],
                                 [[command, "--sign" if extension == ".pkg" else "--force"],
                                  ["xcrun", "notarytool"], ["xcrun", "stapler"], ["xcrun", "stapler"]])
                self.assertEqual(calls[1][2], "submit")
                self.assertEqual(calls[2][2], "staple")
                self.assertEqual(calls[3][2], "validate")
                self.assertEqual(package.read_bytes(), b"original")
                self.assertTrue(output.is_file())

    def test_notarization_failure_does_not_publish_or_staple(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package = root / "input.pkg"
            package.write_bytes(b"original")
            output = root / "output.pkg"

            def run(args, **kwargs):
                if args[0] == "productsign":
                    Path(args[-1]).write_bytes(b"signed")
                return Mock(returncode=0, stdout='{"status":"Invalid"}')

            with patch.object(signer.subprocess, "run", side_effect=run) as command:
                with self.assertRaisesRegex(RuntimeError, "Invalid"):
                    signer.sign_package(package, output, "Developer ID", "profile")
            self.assertEqual(command.call_count, 2)
            self.assertFalse(output.exists())
            self.assertEqual(package.read_bytes(), b"original")

    def test_rejects_existing_output_and_invalid_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package = root / "input.pkg"
            package.write_bytes(b"original")
            output = root / "output.pkg"
            output.write_bytes(b"keep")
            with self.assertRaises(FileExistsError):
                signer.sign_package(package, output, "Developer ID")
            self.assertEqual(output.read_bytes(), b"keep")
            with self.assertRaises(ValueError):
                signer.sign_package(package, root / "output.dmg", "Developer ID")
            with self.assertRaises(ValueError):
                signer.sign_package(package, root / "new.pkg", "-")

    def test_notary_submission_error_is_reported(self):
        result = Mock(returncode=1, stdout="failure output", stderr="failure details")
        with patch.object(signer.subprocess, "run", return_value=result) as run:
            with self.assertRaisesRegex(RuntimeError, "failure details"):
                signer.notarize_package(Path("app.pkg"), "profile")
            self.assertEqual(run.call_count, 1)

    def test_signing_failure_does_not_publish(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            package = root / "input.pkg"
            package.write_bytes(b"original")
            output = root / "output.pkg"
            with patch.object(signer.subprocess, "run", side_effect=subprocess.CalledProcessError(1, "productsign")):
                with self.assertRaises(subprocess.CalledProcessError):
                    signer.sign_package(package, output, "Developer ID")
            self.assertFalse(output.exists())
            self.assertEqual(package.read_bytes(), b"original")


if __name__ == "__main__":
    unittest.main()
