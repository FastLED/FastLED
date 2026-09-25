"""Provision reld, the single native linker for every host build (#4610 follow-up).

reld (https://github.com/zackees/reld) is FastLED's one host-native linker on
Linux, macOS and Windows. It links ELF natively and routes what its native
engine cannot honour -- ThinLTO/plugin links on Linux, and COFF/Mach-O until
its own backends land -- to an lld "bridge". FastLED pins that bridge to the
lld shipped with clang-tool-chain so a link never depends on whichever lld or
rust-lld happens to be on the machine.

``ensure_reld()`` downloads the pinned release once into ``.cached/reld``,
verifies its SHA-256 against the table below (copied from the release's
``SHA256SUMS``), and returns the executable. Other linkers are banned from the
host build surface by ``ci/lint/banned_linkers.py``.
"""

from __future__ import annotations

import hashlib
import os
import platform
import shutil
import tarfile
import tempfile
import zipfile
from pathlib import Path, PurePosixPath
from urllib.request import urlopen


VERSION = "0.2.1"
# reld v0.2.1 SHA256SUMS, one entry per supported host triple.
SHA256: dict[str, str] = {
    "x86_64-unknown-linux-gnu": "99cfe82b285514fd8c1ea7a709da328300bf8693b153efa7129e6cf846a8e33e",
    "aarch64-unknown-linux-gnu": "fc392f3f60c53020f13d815e61b0bc4469c0ebabf5943bfe1c2125bd27707945",
    "x86_64-apple-darwin": "fc537376148d60c638fd7a9ceb87cd1fb837f1e6dae60db8a9a2c110801fa18e",
    "aarch64-apple-darwin": "c5941c264a6e23212a018b06883922621eb3e80f15553fb292948d62faeb63c0",
    "x86_64-pc-windows-msvc": "9f8a1c348636c5e29c507c720173521aea936d420a518ca440e73c998aa83670",
    "aarch64-pc-windows-msvc": "9dbd909b4c7bc4abe97936aa12a0fe1009faf48040f6da3630e313ece7043ad8",
}

PROJECT_ROOT = Path(__file__).resolve().parents[2]
CACHE_ROOT = PROJECT_ROOT / ".cached" / "reld"

# The lld flavour reld bridges to, by host OS. FastLED's Windows host target
# is MinGW (x86_64-windows-gnu), which reld bridges through `ld.lld -m i386pep`.
_BRIDGE_TOOL = {"Linux": "ld.lld", "Darwin": "ld64.lld", "Windows": "ld.lld"}


def host_triple(system: str, machine: str) -> str:
    arch = {
        "x86_64": "x86_64",
        "amd64": "x86_64",
        "arm64": "aarch64",
        "aarch64": "aarch64",
    }.get(machine.lower())
    os_part = {
        "Linux": "unknown-linux-gnu",
        "Darwin": "apple-darwin",
        "Windows": "pc-windows-msvc",
    }.get(system)
    triple = f"{arch}-{os_part}"
    if arch is None or os_part is None or triple not in SHA256:
        raise RuntimeError(f"reld {VERSION} has no release for {system} {machine}")
    return triple


def archive_name(triple: str) -> str:
    suffix = ".zip" if "windows" in triple else ".tar.gz"
    return f"reld-v{VERSION}-{triple}{suffix}"


def safe_member(name: str, root: str) -> Path:
    """Reject archive paths that could escape the install directory."""
    path = PurePosixPath(name)
    if (
        not name
        or "\\" in name
        or path.is_absolute()
        or ".." in path.parts
        or path.parts[0] != root
        or ":" in name
    ):
        raise ValueError(f"Unsafe reld archive path: {name}")
    return Path(*path.parts)


def extract(archive: Path, destination: Path, root: str) -> None:
    """Extract a reld release archive (tar.gz or zip) with path checks."""
    if archive.suffix == ".zip":
        with zipfile.ZipFile(archive) as source:
            for info in source.infolist():
                relative = safe_member(info.filename.rstrip("/"), root)
                output = destination / relative
                if info.is_dir():
                    output.mkdir(parents=True, exist_ok=True)
                    continue
                output.parent.mkdir(parents=True, exist_ok=True)
                with source.open(info) as data, output.open("wb") as out:
                    shutil.copyfileobj(data, out)
        return
    with tarfile.open(archive, "r:gz") as source:
        for member in source:
            relative = safe_member(member.name.rstrip("/"), root)
            if not (member.isdir() or member.isfile()):
                raise ValueError(f"Archive special file is forbidden: {member.name}")
            output = destination / relative
            if member.isdir():
                output.mkdir(parents=True, exist_ok=True)
                continue
            output.parent.mkdir(parents=True, exist_ok=True)
            data = source.extractfile(member)
            if data is None:
                raise ValueError(f"Cannot read archive file: {member.name}")
            with data, output.open("wb") as out:
                shutil.copyfileobj(data, out)
            output.chmod(member.mode & 0o777)


def _sha256(path: Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def executable_name(triple: str) -> str:
    return "reld.exe" if "windows" in triple else "reld"


def ensure_reld(cache_root: Path = CACHE_ROOT) -> Path:
    """Return the pinned reld executable, downloading and verifying it once."""
    triple = host_triple(platform.system(), platform.machine())
    root = f"reld-v{VERSION}-{triple}"
    install_dir = cache_root / root
    linker = install_dir / executable_name(triple)
    if linker.is_file():
        return linker
    cache_root.mkdir(parents=True, exist_ok=True)
    name = archive_name(triple)
    url = f"https://github.com/zackees/reld/releases/download/v{VERSION}/{name}"
    staging = Path(tempfile.mkdtemp(prefix=f".{root}-", dir=cache_root))
    try:
        archive = staging / name
        with urlopen(url, timeout=300) as response, archive.open("wb") as out:
            shutil.copyfileobj(response, out)
        digest = _sha256(archive)
        if digest != SHA256[triple]:
            raise ValueError(f"SHA-256 mismatch for {name}: {digest}")
        extract(archive, staging, root)
        if not (staging / root / executable_name(triple)).is_file():
            raise FileNotFoundError(f"reld executable missing from {name}")
        try:
            # Atomic publish; a concurrent installer that won the race is fine.
            os.replace(staging / root, install_dir)
        except OSError:
            if not linker.is_file():
                raise
    finally:
        shutil.rmtree(staging, ignore_errors=True)
    return linker


def bridge_linker() -> Path:
    """The clang-tool-chain lld that reld delegates bridged links to.

    Required on every host: ThinLTO links on Linux and all COFF/Mach-O links
    on Windows/macOS go through the bridge, and without an explicit bridge
    reld would silently pick whatever rust-lld is on the machine. clang-tool-
    chain is a hard dependency of native builds, so a failed lookup is an
    error, not a fallback (CodeRabbit on #4632).
    """
    system = platform.system()
    tool = _BRIDGE_TOOL.get(system)
    if tool is None:
        raise RuntimeError(f"reld has no bridge linker mapping for host {system}")
    from clang_tool_chain.wrapper import (  # noqa: PLC0415 - heavy import, deferred
        find_tool_binary,
    )

    found = Path(find_tool_binary(tool))
    if not found.is_file():
        raise FileNotFoundError(
            f"reld bridge linker {tool} not found in clang-tool-chain ({found}); "
            "run ./install to provision the toolchain"
        )
    return found


def main() -> None:
    linker = ensure_reld()
    print(linker)
    print(f"bridge: {bridge_linker()}")


if __name__ == "__main__":
    main()
