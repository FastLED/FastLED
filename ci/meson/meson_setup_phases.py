#!/usr/bin/env python3
"""Helper phases for ``setup_meson_build`` orchestration.

These helpers were extracted from ``ci/meson/build_config.py`` so the main
``setup_meson_build`` orchestrator stays readable and the file stays under
1000 LOC. Each helper owns one cohesive piece of pre-setup logic — source
hash detection, marker-based reconfigure detection, file-change detection,
compiler/cache discovery, native-file generation, and the final meson setup
invocation.
"""
# pyright: reportMissingImports=false, reportUnknownVariableType=false

import json
import os
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from ci.meson.cache_utils import get_max_dir_mtime
from ci.meson.meson_cleanup import cleanup_build_artifacts
from ci.meson.output import print_warning
from ci.meson.test_discovery import get_source_files_hash, get_split_source_hashes
from ci.util.global_interrupt_handler import handle_keyboard_interrupt
from ci.util.timestamp_print import ts_print as _ts_print


@dataclass
class MarkerPaths:
    """Paths to every per-build marker file we maintain."""

    thin_archive: Path
    source_files: Path
    src_files: Path
    test_files: Path
    debug: Path
    check: Path
    build_mode: Path
    compiler_version: Path
    zccache_version: Path
    enable_examples: Path
    enable_unit_tests: Path

    @classmethod
    def for_build_dir(cls, build_dir: Path) -> "MarkerPaths":
        return cls(
            thin_archive=build_dir / ".thin_archive_config",
            source_files=build_dir / ".source_files_hash",
            src_files=build_dir / ".src_files_hash",
            test_files=build_dir / ".test_files_hash",
            debug=build_dir / ".debug_config",
            check=build_dir / ".check_config",
            build_mode=build_dir / ".build_mode_config",
            compiler_version=build_dir / ".compiler_version_config",
            zccache_version=build_dir / ".zccache_version_config",
            enable_examples=build_dir / ".enable_examples_config",
            enable_unit_tests=build_dir / ".enable_unit_tests_config",
        )


@dataclass
class SourceHashes:
    """Source file hashes computed once per setup invocation."""

    src_hash: str
    test_hash: str
    source_hash: str
    source_files: list[str]
    max_tests_dir_mtime: float


@dataclass
class ReconfigureDecision:
    """Outcome of marker-based reconfigure detection."""

    reasons: list[str] = field(default_factory=list)
    force_reconfigure: bool = False
    force_reason: Optional[str] = None
    debug_changed: bool = False
    build_mode_changed: bool = False
    last_build_mode: Optional[str] = None


@dataclass
class CompilerDetection:
    """Result of compiler wrapper + cache binary detection."""

    clang_wrapper: str
    clangxx_wrapper: str
    llvm_ar_wrapper: str
    cache_binary: Optional[str]
    cache_name: Optional[str]
    compiler_version: str
    zccache_version: str


def compute_source_hashes(source_dir: Path, markers: MarkerPaths) -> SourceHashes:
    """Compute src/, tests/, and combined source-file hashes.

    Uses directory mtimes as a fast structural-change proxy so we can skip
    the expensive rglob walk when nothing was added or removed. The tests/
    directory mtime is also returned so the caller can reuse it for the
    test_list_cache fast-path.
    """
    max_tests_dir_mtime = get_max_dir_mtime(source_dir / "tests")
    current_src_hash = ""
    current_test_hash = ""
    current_source_hash = ""
    current_source_files: list[str] = []

    max_src_dir_mtime = get_max_dir_mtime(source_dir / "src")
    if markers.src_files.exists():
        try:
            marker_mtime = markers.src_files.stat().st_mtime
            if max_src_dir_mtime <= marker_mtime:
                cached = markers.src_files.read_text(encoding="utf-8").strip()
                if cached:
                    current_src_hash = cached
        except OSError:
            pass

    if markers.test_files.exists():
        try:
            marker_mtime = markers.test_files.stat().st_mtime
            if max_tests_dir_mtime <= marker_mtime:
                cached = markers.test_files.read_text(encoding="utf-8").strip()
                if cached:
                    current_test_hash = cached
        except OSError:
            pass

    if not current_src_hash or not current_test_hash:
        split = get_split_source_hashes(source_dir)
        if not current_src_hash:
            current_src_hash = split.src_hash
        if not current_test_hash:
            current_test_hash = split.tests_hash
        current_source_files = sorted(split.src_files + split.test_files)

    if markers.source_files.exists():
        try:
            marker_mtime = markers.source_files.stat().st_mtime
            max_combined = max(max_src_dir_mtime, max_tests_dir_mtime)
            if max_combined <= marker_mtime:
                cached = markers.source_files.read_text(encoding="utf-8").strip()
                if cached:
                    current_source_hash = cached
        except OSError:
            pass
    if not current_source_hash:
        current_source_hash, current_source_files = get_source_files_hash(source_dir)

    return SourceHashes(
        src_hash=current_src_hash,
        test_hash=current_test_hash,
        source_hash=current_source_hash,
        source_files=current_source_files,
        max_tests_dir_mtime=max_tests_dir_mtime,
    )


def check_reconfigure_markers(
    *,
    build_dir: Path,
    markers: MarkerPaths,
    hashes: SourceHashes,
    debug: bool,
    check: bool,
    build_mode: str,
    enable_examples: bool,
    enable_unit_tests: bool,
    use_thin_archives: bool,
) -> ReconfigureDecision:
    """Inspect marker files and decide whether to reconfigure.

    Returns a decision struct capturing every reason found so the caller
    can print a single consolidated message instead of one per marker.
    """
    decision = ReconfigureDecision()

    # SELF-HEALING: introspection corruption marker
    intro_corruption_marker = build_dir / ".intro_corruption_detected"
    if intro_corruption_marker.exists():
        _ts_print("")
        _ts_print("=" * 80)
        _ts_print("[MESON] ⚠️  INTROSPECTION FILE CORRUPTION DETECTED - AUTO-HEALING")
        _ts_print("=" * 80)
        _ts_print("[MESON] Corruption marker detected (created by test discovery)")
        _ts_print("[MESON]")
        _ts_print(
            "[MESON] The build directory has corrupted or missing intro-*.json files."
        )
        _ts_print(
            "[MESON] These files are required for Meson introspection (test discovery, etc)."
        )
        _ts_print("[MESON]")
        _ts_print(
            "[MESON] 🔧 Auto-fix: Forcing reconfiguration to regenerate intro files"
        )
        _ts_print("=" * 80)
        _ts_print("")
        try:
            intro_corruption_marker.unlink()
            _ts_print("[MESON] ✅ Removed corruption marker")
        except (OSError, IOError) as e:
            _ts_print(f"[MESON] Warning: Could not remove corruption marker: {e}")
        decision.force_reconfigure = True
        decision.force_reason = "introspection files corrupted (auto-healing)"
        decision.reasons.append("introspection files corrupted")

    # Thin archive marker
    if markers.thin_archive.exists():
        try:
            last_thin_setting = markers.thin_archive.read_text().strip() == "True"
            if last_thin_setting != use_thin_archives:
                decision.reasons.append(
                    f"thin archive changed: {last_thin_setting} → {use_thin_archives}"
                )
        except (OSError, IOError):
            decision.reasons.append("thin archive marker unreadable")
    else:
        decision.reasons.append("thin archive marker missing")

    # Source/test file hash markers
    _src_changed = False
    _test_changed = False
    if hashes.src_hash:
        if markers.src_files.exists():
            try:
                if markers.src_files.read_text().strip() != hashes.src_hash:
                    _src_changed = True
            except (OSError, IOError):
                _src_changed = True
        else:
            _src_changed = True
    if hashes.test_hash:
        if markers.test_files.exists():
            try:
                if markers.test_files.read_text().strip() != hashes.test_hash:
                    _test_changed = True
            except (OSError, IOError):
                _test_changed = True
        else:
            _test_changed = True
    if not _src_changed and not _test_changed and hashes.source_hash:
        if markers.source_files.exists():
            try:
                last_hash = markers.source_files.read_text().strip()
                if last_hash != hashes.source_hash:
                    _src_changed = True
                    _test_changed = True
            except (OSError, IOError):
                _src_changed = True
                _test_changed = True
        else:
            _src_changed = True
            _test_changed = True
    if _src_changed and _test_changed:
        decision.reasons.append("source and test files changed")
    elif _src_changed:
        decision.reasons.append("source files changed (src/)")
    elif _test_changed:
        decision.reasons.append("test files changed (tests/)")

    # Debug marker
    if markers.debug.exists():
        try:
            last_debug_setting = markers.debug.read_text().strip() == "True"
            if last_debug_setting != debug:
                decision.reasons.append(
                    f"debug mode changed: {last_debug_setting} → {debug}"
                )
                decision.debug_changed = True
        except (OSError, IOError):
            decision.reasons.append("debug marker unreadable")
    else:
        decision.reasons.append("debug marker missing")

    # Check marker
    if markers.check.exists():
        try:
            last_check_setting = markers.check.read_text().strip() == "True"
            if last_check_setting != check:
                decision.reasons.append(
                    f"IWYU check mode changed: {last_check_setting} → {check}"
                )
        except (OSError, IOError):
            decision.reasons.append("check marker unreadable")
    else:
        decision.reasons.append("check marker missing")

    # Build mode marker
    if markers.build_mode.exists():
        try:
            decision.last_build_mode = markers.build_mode.read_text().strip()
            if decision.last_build_mode != build_mode:
                decision.reasons.append(
                    f"build mode changed: {decision.last_build_mode} → {build_mode}"
                )
                decision.build_mode_changed = True
        except (OSError, IOError):
            decision.reasons.append("build_mode marker unreadable")
    else:
        decision.reasons.append("build_mode marker missing")

    # enable_examples marker
    if markers.enable_examples.exists():
        try:
            last_enable_examples = markers.enable_examples.read_text().strip() == "True"
            if last_enable_examples != enable_examples:
                decision.reasons.append(
                    f"enable_examples changed: {last_enable_examples} → {enable_examples}"
                )
        except (OSError, IOError):
            decision.reasons.append("enable_examples marker unreadable")

    # enable_unit_tests marker
    if markers.enable_unit_tests.exists():
        try:
            last_enable_unit_tests = (
                markers.enable_unit_tests.read_text().strip() == "True"
            )
            if last_enable_unit_tests != enable_unit_tests:
                decision.reasons.append(
                    f"enable_unit_tests changed: {last_enable_unit_tests} → {enable_unit_tests}"
                )
        except (OSError, IOError):
            decision.reasons.append("enable_unit_tests marker unreadable")

    if decision.reasons:
        decision.force_reconfigure = True
        if not decision.force_reason:
            decision.force_reason = "configuration markers changed"
        missing_markers = [r for r in decision.reasons if "missing" in r]
        changed_settings = [
            r for r in decision.reasons if "missing" not in r and "unreadable" not in r
        ]
        unreadable_markers = [r for r in decision.reasons if "unreadable" in r]

        if missing_markers and not changed_settings and not unreadable_markers:
            num_missing = len(missing_markers)
            _ts_print(
                f"[MESON] ℹ️  Build directory needs configuration ({num_missing} marker files missing)"
            )
        else:
            _ts_print("[MESON] 🔄 Reconfiguration required:")
            for reason in decision.reasons:
                _ts_print(f"[MESON]     - {reason}")

    if decision.debug_changed:
        cleanup_build_artifacts(build_dir, "Debug mode changed")
    if decision.build_mode_changed:
        cleanup_build_artifacts(
            build_dir,
            f"Build mode changed ({decision.last_build_mode} → {build_mode})",
        )

    return decision


def check_meson_build_modified(source_dir: Path, build_dir: Path) -> bool:
    """Return True if any meson.build is newer than build.ninja."""
    build_ninja_path = build_dir / "build.ninja"
    if not build_ninja_path.exists():
        return False
    try:
        build_ninja_mtime = build_ninja_path.stat().st_mtime
        meson_build_files = [
            source_dir / "meson.build",
            source_dir / "tests" / "meson.build",
            source_dir / "examples" / "meson.build",
        ]
        for meson_file in meson_build_files:
            if meson_file.exists():
                meson_file_mtime = meson_file.stat().st_mtime
                if meson_file_mtime > build_ninja_mtime:
                    _ts_print(
                        f"[MESON] ⚠️  Detected modified meson.build: {meson_file.relative_to(source_dir)}"
                    )
                    _ts_print(
                        f"[MESON]     File mtime: {meson_file_mtime:.6f} > build.ninja mtime: {build_ninja_mtime:.6f}"
                    )
                    return True
    except (OSError, IOError) as e:
        _ts_print(f"[MESON] Warning: Could not check meson.build timestamps: {e}")
        return True
    return False


def _write_test_list_watermark(build_dir: Path, max_tests_dir_mtime: float) -> None:
    """Persist the tests/ dir mtime that the current cached list corresponds to.

    Written atomically: a torn watermark would be parsed as corrupt and force a
    full rescan, which is safe but silently costs ~400ms on every build.
    """
    watermark_path = build_dir / "test_list_watermark.txt"
    temp_path = watermark_path.with_suffix(".tmp")
    try:
        temp_path.write_text(f"{max_tests_dir_mtime!r}\n")
        temp_path.replace(watermark_path)
    except OSError:
        try:
            temp_path.unlink()
        except OSError:
            pass


def detect_test_file_changes(
    source_dir: Path, build_dir: Path, max_tests_dir_mtime: float
) -> bool:
    """Return True when discovered test files differ from the cached list."""
    test_list_cache_path = build_dir / "test_list_cache.txt"
    # Watermark = the max tests/ dir mtime observed by the scan that produced
    # the cached list. It must NOT be the cache file's own mtime: the cache is
    # written *after* discovery, so a test file created in between would carry
    # an older parent-dir mtime than the cache and be masked forever -- the
    # fast path would report "no changes" on every subsequent run and the new
    # test would silently never build, while the suite still reported green.
    # Comparing against a watermark captured *before* the scan makes anything
    # added during or after that scan strictly newer, so it is always caught.
    watermark_path = build_dir / "test_list_watermark.txt"
    if test_list_cache_path.exists():
        try:
            watermark = float(watermark_path.read_text().strip())
            if max_tests_dir_mtime <= watermark:
                return False
        except (OSError, ValueError):
            # No/corrupt watermark -- fall through to the authoritative compare.
            pass

    try:
        import importlib.util

        tests_py_path = source_dir / "tests"

        spec_dt = importlib.util.spec_from_file_location(
            "discover_tests", tests_py_path / "discover_tests.py"
        )
        assert spec_dt is not None and spec_dt.loader is not None
        mod_dt = importlib.util.module_from_spec(spec_dt)
        spec_dt.loader.exec_module(mod_dt)
        discover_test_files = mod_dt.discover_test_files

        spec_tc = importlib.util.spec_from_file_location(
            "test_config", tests_py_path / "test_config.py"
        )
        assert spec_tc is not None and spec_tc.loader is not None
        mod_tc = importlib.util.module_from_spec(spec_tc)
        spec_tc.loader.exec_module(mod_tc)
        EXCLUDED_TEST_FILES = mod_tc.EXCLUDED_TEST_FILES
        EXCLUDED_TEST_DIRS = mod_tc.EXCLUDED_TEST_DIRS

        tests_dir = source_dir / "tests"
        current_test_files: list[str] = sorted(
            discover_test_files(tests_dir, EXCLUDED_TEST_FILES, EXCLUDED_TEST_DIRS)
        )

        test_list_cache = build_dir / "test_list_cache.txt"
        cached_test_files: list[str] = []
        try:
            with open(test_list_cache, "r") as f:
                cached_test_files = [
                    line.strip()
                    for line in f
                    if line.strip() and not line.startswith("#")
                ]
        except FileNotFoundError:
            cached_test_files = []
        except (IOError, OSError) as e:
            _ts_print(f"[MESON] Warning: Could not read test cache: {e}")
            cached_test_files = []

        if current_test_files != cached_test_files:
            added: set[str] = set(current_test_files) - set(cached_test_files)
            removed: set[str] = set(cached_test_files) - set(current_test_files)

            print_warning("[MESON] ⚠️  Detected test file changes:")
            if added:
                added_list = sorted(added)
                if len(added_list) > 10:
                    print_warning(f"[MESON]     Added: {len(added_list)} test files")
                    print_warning(
                        f"[MESON]     (First 10: {', '.join(added_list[:10])}...)"
                    )
                else:
                    print_warning(f"[MESON]     Added: {', '.join(added_list)}")
            if removed:
                removed_list = sorted(removed)
                if len(removed_list) > 10:
                    print_warning(
                        f"[MESON]     Removed: {len(removed_list)} test files"
                    )
                    print_warning(
                        f"[MESON]     (First 10: {', '.join(removed_list[:10])}...)"
                    )
                else:
                    print_warning(f"[MESON]     Removed: {', '.join(removed_list)}")

            temp_cache = test_list_cache.with_suffix(".tmp")
            try:
                with open(temp_cache, "w") as f:
                    f.write(
                        "# Auto-generated test file list cache for Meson reconfiguration\n"
                    )
                    f.write(f"# Total tests: {len(current_test_files)}\n")
                    f.write("# Generated by meson_runner.py\n\n")
                    for test_file in current_test_files:
                        f.write(f"{test_file}\n")
                temp_cache.replace(test_list_cache)
            except (OSError, IOError) as e:
                _ts_print(
                    f"[MESON] Warning: Could not update test cache atomically: {e}"
                )
                try:
                    temp_cache.unlink()
                except (OSError, IOError):
                    pass
            _write_test_list_watermark(build_dir, max_tests_dir_mtime)
            return True
        # No changes — record the watermark so the fast path activates next run.
        _write_test_list_watermark(build_dir, max_tests_dir_mtime)
        return False

    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        _ts_print(f"[MESON] Warning: Could not check test file changes: {e}")
        return True


def detect_example_file_changes(source_dir: Path, build_dir: Path) -> "str | None":
    """Return a reconfigure reason when ``examples/`` needs re-discovery, else None.

    The reason is distinct for "cache missing" vs "files actually changed": a
    missing cache means we *cannot prove* the examples are unchanged, which is
    not the same claim as having observed a change (issue #3761). The caller
    prints the returned reason, so this does not print one itself.

    This never deletes the cache. Deleting it made a single false positive
    self-perpetuating: only meson's ``--update`` rewrites the file, so any run
    that was fully fingerprint-cached (meson never invoked) left the cache
    missing and guaranteed another false "example files changed" next time.

    The cache file's mtime doubles as a "contents last verified at" marker, and
    is re-stamped whenever the content hash matches. Without that re-stamp a
    single ``git checkout`` would leave every later run paying the full content
    hash (~0.2s for ~300 files) forever, because the checkout's mtimes stay
    newer than the cache the fast path compares against.
    """
    try:
        from ci.meson.example_metadata_cache import (
            compute_example_files_hash,
            max_example_files_mtime,
        )

        examples_dir = source_dir / "examples"
        example_cache_file = build_dir / "examples" / "example_metadata.cache"

        if not examples_dir.exists():
            return None

        if not example_cache_file.exists():
            return "example metadata cache missing - cannot prove examples unchanged"

        # Fast path: if nothing under examples/ is newer than the last-verified
        # marker, the contents cannot have changed since. Both probes are
        # needed — file mtimes catch in-place edits, directory mtimes catch
        # additions and deletions (which leave sibling files untouched).
        try:
            verified_at = example_cache_file.stat().st_mtime
            newest = max(
                get_max_dir_mtime(examples_dir), max_example_files_mtime(examples_dir)
            )
            # Strict `<`: an entry whose mtime lands in the same clock tick as
            # the marker (Windows' system clock granularity is ~15ms) must fall
            # through to the hash rather than be assumed unchanged.
            if newest < verified_at:
                return None
        except OSError:
            pass

        cached_example_hash = ""
        try:
            # save_cache() writes UTF-8; reading with the platform locale codec
            # (cp1252 on Windows) turns any non-ASCII example path or @filter
            # string into a permanent decode failure, and therefore a
            # reconfigure on every single run.
            with open(example_cache_file, "r", encoding="utf-8") as f:
                cache_data = json.load(f)
            cached_example_hash = cache_data.get("hash", "")
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
        except Exception:
            cached_example_hash = ""

        if not cached_example_hash:
            return "example metadata cache unreadable - cannot prove examples unchanged"

        # Stamp the marker from *before* the hash, so an edit that lands while
        # we are hashing still looks newer than the marker on the next run.
        verify_started = time.time()
        if compute_example_files_hash(examples_dir) != cached_example_hash:
            return "example files changed"

        # Contents match: re-arm the fast path so the next run skips the hash.
        try:
            os.utime(example_cache_file, (verify_started, verify_started))
        except OSError:
            pass
        return None

    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        _ts_print(f"[MESON] Warning: Could not check example file changes: {e}")
        return "example file check failed"


def check_obsolete_zig_wrappers(source_dir: Path) -> None:
    """Fail loudly when the legacy ``.build/meson/zig-*-wrapper.exe`` files exist."""
    meson_dir = source_dir / ".build" / "meson"
    if not meson_dir.exists():
        return
    obsolete_wrappers = list(meson_dir.glob("zig-*-wrapper.exe"))
    if not obsolete_wrappers:
        return
    _ts_print("=" * 80)
    _ts_print("[MESON] ❌ ERROR: Obsolete zig wrapper artifacts detected!")
    _ts_print("[MESON]")
    _ts_print(
        "[MESON] Found old zig-based wrapper executables in .build/meson/ directory:"
    )
    for wrapper in obsolete_wrappers:
        _ts_print(f"[MESON]   - {wrapper.name}")
    _ts_print("[MESON]")
    _ts_print("[MESON] These wrappers are from the old zig-based compiler system")
    _ts_print("[MESON] and are no longer compatible with clang-tool-chain.")
    _ts_print("[MESON]")
    _ts_print("[MESON] To fix this issue, delete the .build/meson directory:")
    _ts_print("[MESON]   rm -rf .build/meson")
    _ts_print("[MESON]")
    _ts_print("[MESON] Then run your build command again. The system will use")
    _ts_print("[MESON] clang-tool-chain wrappers instead.")
    _ts_print("=" * 80)
    raise RuntimeError(
        "Obsolete zig wrapper artifacts detected in .build/meson/ directory. "
        "Delete .build/meson/ directory and try again."
    )
