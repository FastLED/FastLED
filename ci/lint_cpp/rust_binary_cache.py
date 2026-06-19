"""Build-on-change cache for the Rust ``fastled-lint`` binary.

Rebuilds the binary only when (a) it does not yet exist or (b) the Rust
crate's sources have changed since the last successful build. Otherwise the
cached binary is returned untouched, removing the per-invocation soldr/cargo
fingerprint-scan tax from the lint hot path.

Implementation notes
--------------------
The crate is small (~20 ``.rs`` files plus ``Cargo.toml``/``Cargo.lock``), so
the fingerprint is computed natively in Python with ``os.stat`` (mtime+size)
+ blake2b hashing — typically under 2ms on Windows and with no IPC/daemon
dependency. We deliberately avoid the ``zccache-fingerprint`` CLI here: that
infra is overkill for ~30 files and adds 300–800ms of broker latency on
Windows even on the cache-hit path.
"""

from __future__ import annotations

import hashlib
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

from ci.util.paths import PROJECT_ROOT


RUST_CRATE_DIR = PROJECT_ROOT / "ci" / "lint_cpp_rs"
RUST_MANIFEST = RUST_CRATE_DIR / "Cargo.toml"
RUST_BINARY_NAME = "fastled-lint"

_FINGERPRINT_FILE = PROJECT_ROOT / ".cache" / "rust_lint_binary.fingerprint"
_FINGERPRINT_INPUTS = (
    # Files (relative to PROJECT_ROOT) and recursive source globs that, when
    # changed, require a rebuild. Order is irrelevant — sorted for determinism.
    "ci/lint_cpp_rs/Cargo.toml",
    "ci/lint_cpp_rs/Cargo.lock",
    "ci/lint_cpp_rs/rust-toolchain.toml",
)
_SRC_DIR = RUST_CRATE_DIR / "src"


def _guess_host_triple() -> str | None:
    """Best-effort guess of the host's Rust target triple.

    Used to prioritize the matching ``target/<triple>/release/`` directory
    when multiple cross-compile outputs exist. A miss is harmless — the
    function only influences ordering, never correctness.
    """
    machine = platform.machine().lower()
    arch = {
        "amd64": "x86_64",
        "x86_64": "x86_64",
        "x64": "x86_64",
        "arm64": "aarch64",
        "aarch64": "aarch64",
        "armv7l": "armv7",
    }.get(machine, machine)
    system = sys.platform
    if system == "win32":
        return f"{arch}-pc-windows-msvc"
    if system == "darwin":
        return f"{arch}-apple-darwin"
    if system.startswith("linux"):
        return f"{arch}-unknown-linux-gnu"
    return None


def _binary_candidate_paths() -> list[Path]:
    """Return candidate locations of the built fastled-lint binary.

    soldr drives cargo with ``CARGO_BUILD_TARGET`` pinned to the host triple,
    so the binary lands under ``target/<triple>/release/`` rather than the
    plain ``target/release/``. Ordering matters because the first existing
    candidate wins: prefer the explicitly-requested triple
    (``CARGO_BUILD_TARGET``), then the host triple, then the plain
    ``target/release/``, then any other triple discovered on disk (sorted
    for determinism — never let directory-iteration order pick a binary
    silently on a multi-triple checkout).
    """
    suffix = ".exe" if os.name == "nt" else ""
    name = f"{RUST_BINARY_NAME}{suffix}"
    target_root = RUST_CRATE_DIR / "target"
    plain_release = target_root / "release" / name

    ordered: list[Path] = []
    seen: set[Path] = set()

    def _add(path: Path) -> None:
        if path not in seen:
            ordered.append(path)
            seen.add(path)

    env_target = os.environ.get("CARGO_BUILD_TARGET")
    if env_target:
        _add(target_root / env_target / "release" / name)

    host_triple = _guess_host_triple()
    if host_triple:
        _add(target_root / host_triple / "release" / name)

    _add(plain_release)

    if target_root.is_dir():
        for triple_dir in sorted(target_root.iterdir(), key=lambda p: p.name):
            if not triple_dir.is_dir() or triple_dir.name == "release":
                continue
            _add(triple_dir / "release" / name)

    return ordered


def _binary_path() -> Path:
    """Return the path to an existing binary, or the default if none exists."""
    candidates = _binary_candidate_paths()
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0]


def _iter_source_files() -> list[Path]:
    """Enumerate every Rust source file plus the discrete top-level inputs."""
    files: list[Path] = []
    if _SRC_DIR.is_dir():
        for path in _SRC_DIR.rglob("*.rs"):
            if path.is_file():
                files.append(path)
    for relative in _FINGERPRINT_INPUTS:
        candidate = PROJECT_ROOT / relative
        if candidate.is_file():
            files.append(candidate)
    files.sort()
    return files


def _compute_fingerprint() -> str:
    """Hash the (path, size, mtime_ns) of every fingerprint input.

    This is content-agnostic but very fast: ~20 ``os.stat`` calls. If any
    file is added, removed, or rewritten the digest changes.
    """
    hasher = hashlib.blake2b(digest_size=16)
    for path in _iter_source_files():
        st = path.stat()
        # Use the project-relative path so the fingerprint is stable across
        # checkouts in different locations.
        relative = path.resolve().relative_to(PROJECT_ROOT)
        hasher.update(str(relative).replace("\\", "/").encode("utf-8"))
        hasher.update(b"\0")
        hasher.update(str(st.st_size).encode("ascii"))
        hasher.update(b"\0")
        hasher.update(str(st.st_mtime_ns).encode("ascii"))
        hasher.update(b"\0")
    return hasher.hexdigest()


def _load_cached_fingerprint() -> str | None:
    try:
        return _FINGERPRINT_FILE.read_text(encoding="utf-8").strip() or None
    except FileNotFoundError:
        return None
    except OSError:
        return None


def _store_fingerprint(value: str) -> None:
    try:
        _FINGERPRINT_FILE.parent.mkdir(parents=True, exist_ok=True)
        _FINGERPRINT_FILE.write_text(value, encoding="utf-8")
    except OSError as exc:
        print(
            f"warning: failed to persist Rust lint fingerprint to {_FINGERPRINT_FILE}: {exc}",
            file=sys.stderr,
        )


def _resolve_soldr() -> str:
    """Return the soldr executable path or raise if it is not installed."""
    soldr = shutil.which("soldr")
    if soldr is None:
        raise RuntimeError(
            "soldr not found on PATH — install via `uv sync` (soldr is pinned in "
            "pyproject.toml). soldr is required to build the Rust C++ linter."
        )
    return soldr


def _run_build(verbose: bool = True) -> None:
    """Invoke ``soldr cargo build --release`` for the fastled-lint binary."""
    soldr = _resolve_soldr()
    cmd = [
        soldr,
        "cargo",
        "build",
        "--release",
        "--manifest-path",
        str(RUST_MANIFEST),
        "--bin",
        RUST_BINARY_NAME,
    ]
    if verbose:
        print(
            f"🔨 Building Rust C++ linter ({RUST_BINARY_NAME}) — this runs once "
            "until the crate sources change.",
            file=sys.stderr,
        )
    result = subprocess.run(
        cmd,
        cwd=str(PROJECT_ROOT),
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"Rust C++ linter build failed (exit {result.returncode}); see "
            "soldr/cargo output above."
        )


def ensure_rust_lint_binary(verbose: bool = True) -> Path:
    """Return a path to a fresh ``fastled-lint`` binary.

    Rebuilds the binary if it does not yet exist or if the cached fingerprint
    over the crate's sources is stale. Otherwise the cached binary is returned
    without invoking soldr/cargo at all.
    """
    binary = _binary_path()
    current_fingerprint = _compute_fingerprint()
    cached_fingerprint = _load_cached_fingerprint()

    needs_rebuild = not binary.exists() or cached_fingerprint != current_fingerprint
    if needs_rebuild:
        _run_build(verbose=verbose)
        binary = _binary_path()
        # Recompute after the build so the persisted fingerprint reflects any
        # generated/touched files that participate in the input set.
        _store_fingerprint(_compute_fingerprint())

    if not binary.exists():
        raise RuntimeError(
            f"Rust C++ linter binary missing after successful build (looked in: "
            f"{[str(p) for p in _binary_candidate_paths()]})"
        )
    return binary


def main() -> int:
    """CLI: print the path to the (built-if-needed) binary."""
    try:
        path = ensure_rust_lint_binary(verbose=True)
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        return 1
    print(str(path))
    return 0


if __name__ == "__main__":
    sys.exit(main())
