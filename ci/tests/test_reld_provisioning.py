"""Pinned reld provisioning: host mapping, safe extraction, checksum gate."""

import io
import tarfile
import zipfile
from pathlib import Path

import pytest

from ci.tools import reld


@pytest.mark.parametrize(
    ("system", "machine", "triple"),
    [
        ("Linux", "x86_64", "x86_64-unknown-linux-gnu"),
        ("Linux", "aarch64", "aarch64-unknown-linux-gnu"),
        ("Darwin", "arm64", "aarch64-apple-darwin"),
        ("Darwin", "x86_64", "x86_64-apple-darwin"),
        ("Windows", "AMD64", "x86_64-pc-windows-msvc"),
        ("Windows", "ARM64", "aarch64-pc-windows-msvc"),
    ],
)
def test_every_ci_host_has_a_pinned_release(
    system: str, machine: str, triple: str
) -> None:
    assert reld.host_triple(system, machine) == triple
    assert len(reld.SHA256[triple]) == 64


@pytest.mark.parametrize(
    ("system", "machine"), [("FreeBSD", "x86_64"), ("Linux", "riscv64")]
)
def test_unsupported_hosts_fail_loudly(system: str, machine: str) -> None:
    with pytest.raises(RuntimeError, match="no release"):
        reld.host_triple(system, machine)


def test_archive_names_follow_the_release_contract() -> None:
    assert reld.archive_name("x86_64-pc-windows-msvc").endswith(".zip")
    assert reld.archive_name("x86_64-unknown-linux-gnu") == (
        f"reld-v{reld.VERSION}-x86_64-unknown-linux-gnu.tar.gz"
    )


@pytest.mark.parametrize(
    "name", ["../reld", "/tmp/reld", "other/reld", "root\\reld", "C:reld"]
)
def test_unsafe_archive_paths_are_rejected(name: str) -> None:
    with pytest.raises(ValueError, match="Unsafe"):
        reld.safe_member(name, "root")


def _tar(path: Path, root: str) -> None:
    with tarfile.open(path, "w:gz") as out:
        data = b"#!/bin/sh\n"
        info = tarfile.TarInfo(f"{root}/reld")
        info.size = len(data)
        info.mode = 0o755
        out.addfile(info, io.BytesIO(data))


def test_extracts_tar_and_zip_layouts(tmp_path: Path) -> None:
    root = "reld-v0-x"
    tar_path = tmp_path / "a.tar.gz"
    _tar(tar_path, root)
    reld.extract(tar_path, tmp_path / "t", root)
    assert (tmp_path / "t" / root / "reld").is_file()
    zip_path = tmp_path / "a.zip"
    with zipfile.ZipFile(zip_path, "w") as out:
        out.writestr(f"{root}/reld.exe", b"MZ")
    reld.extract(zip_path, tmp_path / "z", root)
    assert (tmp_path / "z" / root / "reld.exe").read_bytes() == b"MZ"


def test_checksum_mismatch_never_installs(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    triple = reld.host_triple("Linux", "x86_64")
    monkeypatch.setattr(reld.platform, "system", lambda: "Linux")
    monkeypatch.setattr(reld.platform, "machine", lambda: "x86_64")
    fake = tmp_path / "fake.tar.gz"
    _tar(fake, f"reld-v{reld.VERSION}-{triple}")
    monkeypatch.setattr(reld, "urlopen", lambda url, timeout: fake.open("rb"))
    with pytest.raises(ValueError, match="SHA-256 mismatch"):
        reld.ensure_reld(tmp_path / "cache")
    assert not (tmp_path / "cache" / f"reld-v{reld.VERSION}-{triple}").exists()


def test_installed_reld_is_reused(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setattr(reld.platform, "system", lambda: "Linux")
    monkeypatch.setattr(reld.platform, "machine", lambda: "x86_64")
    installed = tmp_path / f"reld-v{reld.VERSION}-x86_64-unknown-linux-gnu" / "reld"
    installed.parent.mkdir(parents=True)
    installed.write_bytes(b"x")

    def _no_download(url: str, timeout: int) -> None:
        raise AssertionError("must not download when already installed")

    monkeypatch.setattr(reld, "urlopen", _no_download)
    assert reld.ensure_reld(tmp_path) == installed
