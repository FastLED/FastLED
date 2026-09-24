import json
import os
from pathlib import Path
from typing import Callable, Dict, Optional

from ci.early_exit_cache import FINGERPRINT_VALIDATION_VERSION, fingerprint_aux_hash
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


_CPP_SOURCE_EXTS = frozenset([".cpp", ".h", ".hpp", ".c", ".ino"])
_PY_SOURCE_EXTS = frozenset([".py"])
_WASM_SOURCE_EXTS = _CPP_SOURCE_EXTS | frozenset([".js", ".html"])


def _get_max_source_file_mtime(root: Path, exts: frozenset[str] | None = None) -> float:
    """Return max mtime of any source file under root, skipping build directories.

    Uses os.scandir() for efficiency. Detects BOTH structural changes (file
    add/remove) AND content modifications (file writes), unlike
    get_max_dir_mtime() which only detects structural changes.

    Args:
        root: Root directory to scan
        exts: Set of file extensions to check (default: _CPP_SOURCE_EXTS).
              Pass _PY_SOURCE_EXTS to scan Python files instead.

    Returns 0.0 on missing root or any OS error.
    """
    if exts is None:
        exts = _CPP_SOURCE_EXTS
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
                            if ext.lower() in exts:
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
        self._needs_run: dict[str, bool] = {}
        # name -> max source mtime seen at the START of this invocation's check
        self._observed_max_mtime: dict[str, float] = {}
        self._observed_aux_hash: dict[str, str | None] = {}

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
                        status=(
                            data.get("status")
                            if data.get("validation_version")
                            == FINGERPRINT_VALIDATION_VERSION
                            and data.get("aux_hash") is not None
                            else None
                        ),
                        num_tests_run=data.get("num_tests_run"),
                        num_tests_passed=data.get("num_tests_passed"),
                        duration_seconds=data.get("duration_seconds"),
                        test_name=data.get("test_name"),
                        source_max_mtime=data.get("source_max_mtime"),
                        aux_hash=data.get("aux_hash"),
                        scope=data.get("scope"),
                        num_examples_run=data.get("num_examples_run"),
                        num_examples_passed=data.get("num_examples_passed"),
                        examples_included=data.get("examples_included"),
                    )
                except json.JSONDecodeError:
                    ts_print(f"Invalid {name} fingerprint file. Recalculating...")
        return None

    def write(self, name: str, fingerprint: FingerprintResult) -> None:
        fingerprint_file = self._get_fingerprint_file(name)
        fingerprint_dict: dict[str, Optional[str | int | float]] = {
            "validation_version": FINGERPRINT_VALIDATION_VERSION,
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
        if fingerprint.source_max_mtime is not None:
            fingerprint_dict["source_max_mtime"] = fingerprint.source_max_mtime
        fingerprint_dict["aux_hash"] = fingerprint.aux_hash or fingerprint_aux_hash(
            name
        )
        if fingerprint.scope is not None:
            fingerprint_dict["scope"] = fingerprint.scope
        if fingerprint.num_examples_run is not None:
            fingerprint_dict["num_examples_run"] = fingerprint.num_examples_run
        if fingerprint.num_examples_passed is not None:
            fingerprint_dict["num_examples_passed"] = fingerprint.num_examples_passed
        if fingerprint.examples_included is not None:
            fingerprint_dict["examples_included"] = fingerprint.examples_included
        with open(fingerprint_file, "w") as f:
            json.dump(fingerprint_dict, f, indent=2)

    def check(self, name: str, calculator: Callable[[], FingerprintResult]) -> bool:
        prev_fingerprint = self.read(name)
        self._prev_fingerprints[name] = prev_fingerprint

        fingerprint_data = calculator()
        self._fingerprints[name] = fingerprint_data

        needs_run = prev_fingerprint is None or not prev_fingerprint.should_skip(
            fingerprint_data
        )
        self._needs_run[name] = needs_run
        return needs_run

    def save_success(self, name: str) -> None:
        """Persist a completed full scope; a planned cache hit stays untouched."""
        if not self._needs_run.get(name, False):
            return
        fingerprint = self._fingerprints[name]
        if fingerprint.status is not None and fingerprint.status != "success":
            raise ValueError(f"Cannot certify failed fingerprint calculation: {name}")
        aux_hash = self._observed_aux_hash.get(name)
        if aux_hash is None:
            aux_hash = fingerprint_aux_hash(name)
        if aux_hash is None:
            ts_print(f"Cannot persist {name} fingerprint: build input unreadable")
            return
        fingerprint.status = "success"
        fingerprint.source_max_mtime = self._observed_max_mtime.get(name)
        fingerprint.aux_hash = aux_hash
        self.write(name, fingerprint)

    def mark_failure(self, name: str) -> None:
        """Invalidate only a scope that actually failed, including forced runs."""
        fingerprint = self._fingerprints.get(name)
        if fingerprint is None:
            return
        fingerprint.status = "failure"
        fingerprint.source_max_mtime = self._observed_max_mtime.get(name)
        fingerprint.aux_hash = self._observed_aux_hash.get(name)
        self.write(name, fingerprint)

    def update_test_metadata(
        self,
        name: str,
        num_tests_run: int,
        num_tests_passed: int,
        duration_seconds: float,
        test_name: Optional[str] = None,
        scope: Optional[str] = None,
        num_examples_run: Optional[int] = None,
        num_examples_passed: Optional[int] = None,
        examples_included: Optional[bool] = None,
    ) -> None:
        """Update the test metadata for a fingerprint before saving.

        ``scope`` is "full" when the run covered the whole suite and
        "partial" when a filter narrowed it (#3763). The ``examples_*``
        arguments split the totals into the two populations a C++ run covers
        so the replayed line cannot present a unit-only count as a full pass
        (#3779); leave them ``None`` when the producing path cannot attribute
        its counts.
        """
        if name in self._fingerprints:
            self._fingerprints[name].num_tests_run = num_tests_run
            self._fingerprints[name].num_tests_passed = num_tests_passed
            self._fingerprints[name].duration_seconds = duration_seconds
            if test_name:
                self._fingerprints[name].test_name = test_name
            self._fingerprints[name].scope = scope
            self._fingerprints[name].num_examples_run = num_examples_run
            self._fingerprints[name].num_examples_passed = num_examples_passed
            self._fingerprints[name].examples_included = examples_included

    def get_prev_fingerprint(self, name: str) -> Optional[FingerprintResult]:
        """Get the previous fingerprint data (from last run) for display"""
        return self._prev_fingerprints.get(name)

    def _mtime_fast_path(
        self,
        name: str,
        *dirs: Path,
        exts: frozenset[str] | None = None,
    ) -> bool:
        """
        Mtime-based fast-path for fingerprint checks.

        If the fingerprint file for *name* is NEWER than all source file mtimes in
        *dirs*, AND the stored status is "success", we can skip the expensive
        rglob + SHA-256 hash computation entirely.

        Returns True if the fast-path fired (fingerprint up-to-date, no change),
        False if the full computation is needed.

        Side effects on True: populates the previous result and records a hit.

        Uses file-level mtime scanning (not directory-level) to correctly detect
        content-only modifications. On NTFS/ext4, directory mtimes do NOT update
        when a file's content changes (only on file create/delete), so the previous
        directory-only approach could produce false "no change" results.

        Args:
            name: Fingerprint cache name
            *dirs: Directories to scan for source file changes
            exts: File extensions to scan (default: _CPP_SOURCE_EXTS).
                  Pass _PY_SOURCE_EXTS for Python test fingerprinting.

        Overhead: ~20-70ms per call (file-level scanning of source files).
        Savings: ~200-400ms vs full SHA-256 computation when no changes detected.
        """
        try:
            max_file_mtime = max(
                (_get_max_source_file_mtime(d, exts=exts) for d in dirs), default=0.0
            )
            # Remember what this scan saw. If a run follows, save_success() persists
            # it as the new watermark, describing the tree at that run's START.
            self._observed_max_mtime[name] = max_file_mtime
            current_aux_hash = fingerprint_aux_hash(name)
            self._observed_aux_hash[name] = current_aux_hash
            if current_aux_hash is None:
                return False
            fp_file = self._get_fingerprint_file(name)
            if not fp_file.exists():
                return False
            prev = self.read(name)
            if prev is None or prev.status != "success":
                return False  # no previous result or previous run failed
            # Gate on the watermark recorded when the cached run BEGAN. There is
            # deliberately no fall back to fp_file.stat().st_mtime: that file is
            # written at run END, so a source file created mid-run compares as
            # older and is skipped forever. A cache without a watermark (written
            # before this field existed) simply takes the slow path once.
            if prev.source_max_mtime is None:
                return False
            if prev.aux_hash != current_aux_hash:
                return False
            if max_file_mtime > prev.source_max_mtime:
                return False  # a source file changed after the cached run began
            # Fast-path fires: nothing will be rebuilt, so do NOT advance the
            # watermark -- keep the one the cached run was actually gated on.
            self._observed_max_mtime.pop(name, None)
            self._prev_fingerprints[name] = prev
            self._fingerprints[name] = FingerprintResult(
                hash=prev.hash,
                source_max_mtime=prev.source_max_mtime,
                aux_hash=prev.aux_hash,
            )
            self._needs_run[name] = False
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
        # Fast-path: if ci/ has no .py file changes since last write, skip 135ms hash.
        # Python tests depend on ci/ Python modules and ci/tests/ test files.
        # IMPORTANT: Must use _PY_SOURCE_EXTS here since ci/ contains Python files,
        # not C++ files. Using the default C++ extensions would cause the fast path
        # to always fire (finding no .py changes), silently skipping Python tests.
        if self._mtime_fast_path("python_test", cwd / "ci", exts=_PY_SOURCE_EXTS):
            return False  # no change detected via mtime fast-path
        return self.check("python_test", calculate_python_test_fingerprint)

    def check_wasm(self) -> bool:
        cwd = Path.cwd()
        if self._mtime_fast_path(
            "wasm", cwd / "src", cwd / "examples" / "wasm", exts=_WASM_SOURCE_EXTS
        ):
            return False  # no change detected via mtime fast-path
        return self.check("wasm", calculate_wasm_fingerprint)

    def check_all(self) -> bool:
        cwd = Path.cwd()
        if self._mtime_fast_path("all", cwd / "src"):
            return False  # no change detected via mtime fast-path
        return self.check("all", calculate_fingerprint)
