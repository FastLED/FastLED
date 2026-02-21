import json
import os
from pathlib import Path
from typing import Callable, Dict, Optional

from ci.meson.cache_utils import (
    _SKIP_DIR_NAMES,
    _SKIP_DIR_PREFIXES,
    get_max_dir_mtime,
)
from ci.util.test_types import (
    FingerprintResult,
    TestArgs,
    calculate_cpp_test_fingerprint,
    calculate_examples_fingerprint,
    calculate_fingerprint,
    calculate_python_test_fingerprint,
    calculate_wasm_fingerprint,
)
from ci.util.timestamp_print import ts_print


def _get_max_source_file_mtime(root: Path) -> float:
    """Return max mtime of any source file under root, skipping build directories.

    Uses os.scandir() for efficiency. Detects BOTH structural changes (file
    add/remove) AND content modifications (file writes), unlike
    get_max_dir_mtime() which only detects structural changes.

    Scans C++ source files (.cpp, .h, .hpp, .c, .ino) by default.
    Returns 0.0 on missing root or any OS error.
    """
    _SOURCE_EXTS = frozenset([".cpp", ".h", ".hpp", ".c", ".ino"])
    max_mtime = 0.0
    stack = [str(root)]
    while stack:
        current = stack.pop()
        try:
            with os.scandir(current) as it:
                for entry in it:
                    try:
                        name = entry.name
                        if entry.is_dir(follow_symlinks=False):
                            if name not in _SKIP_DIR_NAMES and not any(
                                name.startswith(p) for p in _SKIP_DIR_PREFIXES
                            ):
                                stack.append(entry.path)
                        elif entry.is_file(follow_symlinks=False):
                            _, ext = os.path.splitext(name)
                            if ext.lower() in _SOURCE_EXTS:
                                mtime = entry.stat(follow_symlinks=False).st_mtime
                                if mtime > max_mtime:
                                    max_mtime = mtime
                    except OSError:
                        pass
        except OSError:
            pass
    return max_mtime


class FingerprintManager:
    def __init__(self, cache_dir: Path, build_mode: str = "quick"):
        self.cache_dir = cache_dir
        self.build_mode = build_mode
        self.fingerprint_dir = cache_dir / "fingerprint"
        self.cache_dir.mkdir(exist_ok=True)
        self.fingerprint_dir.mkdir(exist_ok=True)
        self._fingerprints: dict[str, FingerprintResult] = {}
        self._prev_fingerprints: dict[str, Optional[FingerprintResult]] = {}

    def _get_fingerprint_file(self, name: str) -> Path:
        # For cpp_test and examples, include build mode in filename to separate caches per build mode
        if name in ("cpp_test", "examples"):
            return self.fingerprint_dir / f"{name}_{self.build_mode}.json"
        return self.fingerprint_dir / f"{name}.json"

    def read(self, name: str) -> Optional[FingerprintResult]:
        fingerprint_file = self._get_fingerprint_file(name)
        if fingerprint_file.exists():
            with open(fingerprint_file, "r") as f:
                try:
                    data = json.load(f)
                    return FingerprintResult(
                        hash=data.get("hash", ""),
                        elapsed_seconds=data.get("elapsed_seconds"),
                        status=data.get("status"),
                        num_tests_run=data.get("num_tests_run"),
                        num_tests_passed=data.get("num_tests_passed"),
                        duration_seconds=data.get("duration_seconds"),
                        test_name=data.get("test_name"),
                    )
                except json.JSONDecodeError:
                    ts_print(f"Invalid {name} fingerprint file. Recalculating...")
        return None

    def write(self, name: str, fingerprint: FingerprintResult) -> None:
        fingerprint_file = self._get_fingerprint_file(name)
        fingerprint_dict: dict[str, Optional[str | int | float]] = {
            "hash": fingerprint.hash,
            "elapsed_seconds": fingerprint.elapsed_seconds,
            "status": fingerprint.status,
        }
        # Include test metadata if available
        if fingerprint.num_tests_run is not None:
            fingerprint_dict["num_tests_run"] = fingerprint.num_tests_run
        if fingerprint.num_tests_passed is not None:
            fingerprint_dict["num_tests_passed"] = fingerprint.num_tests_passed
        if fingerprint.duration_seconds is not None:
            fingerprint_dict["duration_seconds"] = fingerprint.duration_seconds
        if fingerprint.test_name is not None:
            fingerprint_dict["test_name"] = fingerprint.test_name
        with open(fingerprint_file, "w") as f:
            json.dump(fingerprint_dict, f, indent=2)

    def check(self, name: str, calculator: Callable[[], FingerprintResult]) -> bool:
        prev_fingerprint = self.read(name)
        self._prev_fingerprints[name] = prev_fingerprint

        fingerprint_data = calculator()
        self._fingerprints[name] = fingerprint_data

        if prev_fingerprint is None:
            return True
        return not prev_fingerprint.should_skip(fingerprint_data)

    def save_all(self, status: str) -> None:
        for name, fingerprint in self._fingerprints.items():
            fingerprint.status = status
            # Preserve test metadata from previous fingerprint if current one doesn't have it
            # This happens when tests are skipped (cache hit) - we want to keep the old metadata
            if fingerprint.num_tests_run is None:
                prev_fp = self._prev_fingerprints.get(name)
                if prev_fp is not None:
                    fingerprint.num_tests_run = prev_fp.num_tests_run
                    fingerprint.num_tests_passed = prev_fp.num_tests_passed
                    fingerprint.duration_seconds = prev_fp.duration_seconds
                    fingerprint.test_name = prev_fp.test_name
            self.write(name, fingerprint)

    def update_test_metadata(
        self,
        name: str,
        num_tests_run: int,
        num_tests_passed: int,
        duration_seconds: float,
        test_name: Optional[str] = None,
    ) -> None:
        """Update the test metadata for a fingerprint before saving"""
        if name in self._fingerprints:
            self._fingerprints[name].num_tests_run = num_tests_run
            self._fingerprints[name].num_tests_passed = num_tests_passed
            self._fingerprints[name].duration_seconds = duration_seconds
            if test_name:
                self._fingerprints[name].test_name = test_name

    def get_prev_fingerprint(self, name: str) -> Optional[FingerprintResult]:
        """Get the previous fingerprint data (from last run) for display"""
        return self._prev_fingerprints.get(name)

    def _mtime_fast_path(self, name: str, *dirs: Path) -> bool:
        """
        Mtime-based fast-path for fingerprint checks.

        If the fingerprint file for *name* is NEWER than all source file mtimes in
        *dirs*, AND the stored status is "success", we can skip the expensive
        rglob + SHA-256 hash computation entirely.

        Returns True if the fast-path fired (fingerprint up-to-date, no change),
        False if the full computation is needed.

        Side effects on True: populates _prev_fingerprints[name] and
        _fingerprints[name] so that save_all() behaves correctly.

        Uses file-level mtime scanning (not directory-level) to correctly detect
        content-only modifications. On NTFS/ext4, directory mtimes do NOT update
        when a file's content changes (only on file create/delete), so the previous
        directory-only approach could produce false "no change" results.

        Overhead: ~20-70ms per call (file-level scanning of source files).
        Savings: ~200-400ms vs full SHA-256 computation when no changes detected.
        """
        fp_file = self._get_fingerprint_file(name)
        if not fp_file.exists():
            return False
        try:
            fp_mtime = fp_file.stat().st_mtime
            max_file_mtime = max(
                (_get_max_source_file_mtime(d) for d in dirs), default=0.0
            )
            if max_file_mtime > fp_mtime:
                return False  # a source file was modified after fingerprint write
            prev = self.read(name)
            if prev is None or prev.status != "success":
                return False  # no previous result or previous run failed
            # Fast-path fires: record the cached result without running the calculator
            self._prev_fingerprints[name] = prev
            self._fingerprints[name] = FingerprintResult(hash=prev.hash)
            return True
        except OSError:
            return False

    def check_cpp(self, args: TestArgs) -> bool:
        cwd = Path.cwd()
        if self._mtime_fast_path("cpp_test", cwd / "src", cwd / "tests"):
            return False  # no change detected via mtime fast-path
        return self.check("cpp_test", lambda: calculate_cpp_test_fingerprint(args))

    def check_examples(self, args: TestArgs) -> bool:
        cwd = Path.cwd()
        if self._mtime_fast_path("examples", cwd / "src", cwd / "examples"):
            return False  # no change detected via mtime fast-path
        return self.check("examples", lambda: calculate_examples_fingerprint(args))

    def check_python(self) -> bool:
        cwd = Path.cwd()
        # Fast-path: if ci/ has no structural changes since last write, skip 135ms hash.
        # Python tests depend on ci/ Python modules and ci/tests/ test files.
        # Limitation: in-place file content edits (no add/remove) are not detected.
        if self._mtime_fast_path("python_test", cwd / "ci"):
            return False  # no change detected via mtime fast-path
        return self.check("python_test", calculate_python_test_fingerprint)

    def check_wasm(self) -> bool:
        cwd = Path.cwd()
        # Fast-path: if src/ and examples/ have no structural changes since last write,
        # skip the expensive 500ms+ rglob + SHA-256 computation.
        # WASM tests depend on src/ C++ files and examples/wasm/ source files.
        # Limitation: in-place file content edits (no add/remove) are not detected.
        if self._mtime_fast_path("wasm", cwd / "src", cwd / "examples"):
            return False  # no change detected via mtime fast-path
        return self.check("wasm", calculate_wasm_fingerprint)

    def check_all(self) -> bool:
        cwd = Path.cwd()
        if self._mtime_fast_path("all", cwd / "src"):
            return False  # no change detected via mtime fast-path
        return self.check("all", calculate_fingerprint)
