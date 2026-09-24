"""Focused checks for the Linux-only upstream Wild installer."""

import tarfile
import tempfile
import unittest
from pathlib import Path

from ci.tools.install_native_linker import extract, host_target, safe_member


class NativeLinkerInstallerTests(unittest.TestCase):
    def test_host_mapping(self) -> None:
        self.assertEqual(host_target("Linux", "x86_64"), "x86_64-unknown-linux-gnu")
        self.assertEqual(host_target("Linux", "aarch64"), "aarch64-unknown-linux-gnu")
        for system, machine in (
            ("Darwin", "arm64"),
            ("Windows", "AMD64"),
            ("FreeBSD", "x86_64"),
            ("Linux", "riscv64"),
        ):
            with self.subTest(system=system, machine=machine):
                with self.assertRaises(RuntimeError):
                    host_target(system, machine)

    def test_archive_paths(self) -> None:
        self.assertEqual(
            safe_member("wild-linker-0.10.0-x/wild", "wild-linker-0.10.0-x"),
            Path("wild-linker-0.10.0-x/wild"),
        )
        for path in (
            "../escape",
            "/absolute",
            "wild-linker-0.10.0-x/../escape",
            "wild-linker-0.10.0-x\\escape",
            "C:/escape",
        ):
            with self.subTest(path=path), self.assertRaises(ValueError):
                safe_member(path, "wild-linker-0.10.0-x")

    def test_tar_link_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            archive = Path(temp) / "sample.tar.gz"
            with tarfile.open(archive, "w:gz") as output:
                member = tarfile.TarInfo("wild-linker-0.10.0-x/wild")
                member.type = tarfile.SYMTYPE
                member.linkname = "../../escape"
                output.addfile(member)
            with self.assertRaises(ValueError):
                extract(archive, Path(temp), "wild-linker-0.10.0-x")


if __name__ == "__main__":
    unittest.main()
