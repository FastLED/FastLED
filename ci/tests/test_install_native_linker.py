"""Focused checks for the cross-platform reld installer."""

import tarfile
import tempfile
import unittest
from pathlib import Path

from ci.tools.install_native_linker import extract, host_target, safe_member


class NativeLinkerInstallerTests(unittest.TestCase):
    def test_host_mapping(self) -> None:
        self.assertEqual(host_target("Linux", "x86_64"), "x86_64-unknown-linux-gnu")
        self.assertEqual(host_target("Darwin", "arm64"), "aarch64-apple-darwin")
        self.assertEqual(host_target("Windows", "AMD64"), "x86_64-pc-windows-msvc")
        self.assertEqual(host_target("Windows", "ARM64"), "aarch64-pc-windows-msvc")
        self.assertEqual(host_target("Windows", "aarch64"), "aarch64-pc-windows-msvc")
        with self.assertRaises(RuntimeError):
            host_target("FreeBSD", "x86_64")

    def test_archive_paths(self) -> None:
        self.assertEqual(
            safe_member("reld-v0.2.0-x/reld", "reld-v0.2.0-x"),
            Path("reld-v0.2.0-x/reld"),
        )
        for path in (
            "../escape",
            "/absolute",
            "reld-v0.2.0-x/../escape",
            "reld-v0.2.0-x\\escape",
            "C:/escape",
        ):
            with self.subTest(path=path), self.assertRaises(ValueError):
                safe_member(path, "reld-v0.2.0-x")

    def test_tar_link_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            archive = Path(temp) / "sample.tar.gz"
            with tarfile.open(archive, "w:gz") as output:
                member = tarfile.TarInfo("reld-v0.2.0-x/reld")
                member.type = tarfile.SYMTYPE
                member.linkname = "../../escape"
                output.addfile(member)
            with self.assertRaises(ValueError):
                extract(archive, Path(temp), "reld-v0.2.0-x")


if __name__ == "__main__":
    unittest.main()
