#!/usr/bin/env python3
"""
Content-Preserving Archive Wrapper for llvm-ar.

Problem:
    When sccache returns cached .obj files on a cache hit, it updates the output
    file mtime to NOW. This causes ninja to see .obj files as newer than
    libfastled.a, triggering a re-archive of libfastled.a (same content, new
    mtime). Ninja then sees libfastled.a as newer than all 327 DLLs, triggering
    relinking of all DLLs even though the final output is bit-for-bit identical.

Solution:
    This wrapper intercepts the archive command. If the newly-created archive
    content is identical to the existing archive, it does NOT replace the archive
    file. The existing archive's mtime is preserved.

    When combined with `restat = 1` on the STATIC_LINKER rule in build.ninja,
    ninja re-checks libfastled.a's mtime after the archive command. If mtime is
    unchanged (because we didn't replace it), ninja removes libfastled.a from
    the "recently built" list and does NOT propagate dirty to downstream DLL
    targets. This suppresses the entire DLL relinking cascade.

    The archive content comparison is RELIABLE because llvm-ar is invoked with
    the `D` (deterministic) flag, which produces byte-for-byte identical output
    for identical inputs (no embedded timestamps in the archive).

Usage (as configured in meson_native.txt):
    ar = ['python.exe', 'path/to/ar_content_preserving.py', 'clang-tool-chain-ar.EXE']

    The STATIC_LINKER rule in build.ninja then becomes:
    command = "python.exe" "ar_content_preserving.py" "clang-tool-chain-ar.EXE" $LINK_ARGS $out $in

    Arguments received by this script (sys.argv):
        [0]: path to this script
        [1]: actual ar executable (e.g., 'clang-tool-chain-ar.EXE')
        [2]: link flags (e.g., 'csrDT')
        [3]: output archive path (e.g., 'libfastled.a')
        [4+]: input object files

Performance:
    - Existing archive hash: ~1ms (757KB at ~1GB/s)
    - New archive created in temp file: same as original archive step
    - New archive hash: ~1ms
    - Total overhead when content unchanged: ~2ms (vs. 0ms without this wrapper)
    - Total overhead when content changed: ~2ms (negligible, just double-hashing)

    The savings: skip relinking 327 DLLs when src/ whitespace changes, saving
    potentially 60+ seconds of linking time per `bash test --no-fingerprint` run.
"""

import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def _hash_file(path: Path) -> str:
    """Compute SHA-256 hash of a file. Returns empty string on error."""
    try:
        hasher = hashlib.sha256()
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(65536), b""):
                hasher.update(chunk)
        return hasher.hexdigest()
    except OSError:
        return ""


def main() -> None:
    """
    Main entry point. Called by ninja as the STATIC_LINKER command.

    Expected argv:
        sys.argv[0]: this script
        sys.argv[1]: actual ar executable path
        sys.argv[2]: link flags (e.g., 'csrDT')
        sys.argv[3]: output archive path
        sys.argv[4+]: input object file paths
    """
    if len(sys.argv) < 2:
        print(
            f"Usage: {sys.argv[0]} <ar_exe> <flags> <output.a> [input.obj ...]",
            file=sys.stderr,
        )
        sys.exit(1)

    if len(sys.argv) < 4:
        # Probe mode or version check - forward directly to actual ar
        # This handles cases like: ar_wrapper.py clang-tool-chain-ar --version
        ar_exe_probe = sys.argv[1]
        probe_args = sys.argv[2:]
        result = subprocess.run([ar_exe_probe] + probe_args)
        sys.exit(result.returncode)

    ar_exe = sys.argv[1]
    flags = sys.argv[2]
    output = Path(sys.argv[3])
    inputs = sys.argv[4:]

    # If output doesn't exist yet, just run the archiver directly (first build)
    if not output.exists():
        result = subprocess.run([ar_exe, flags, str(output)] + inputs)
        sys.exit(result.returncode)

    # Output exists: hash it to check if new archive will have same content
    old_hash = _hash_file(output)
    if not old_hash:
        # Can't read existing archive - just run directly
        result = subprocess.run([ar_exe, flags, str(output)] + inputs)
        sys.exit(result.returncode)

    # Create new archive in a temp file so we can compare without clobbering output.
    # IMPORTANT: mkstemp creates an empty file (0 bytes). llvm-ar with the 'r' flag
    # would try to open this empty file as an existing archive and fail with
    # "file too small to be an archive". Delete the empty file before running ar
    # so ar creates the archive fresh.
    tmp_fd, tmp_path_str = tempfile.mkstemp(
        suffix=".a",
        prefix="fastled_ar_",
        dir=output.parent,  # Same directory for atomic rename
    )
    os.close(tmp_fd)
    tmp_path = Path(tmp_path_str)
    # Remove the empty temp file so ar creates the archive from scratch
    tmp_path.unlink(missing_ok=True)

    try:
        # Run archiver to temp file (ar will create it fresh)
        result = subprocess.run([ar_exe, flags, str(tmp_path)] + inputs)

        if result.returncode != 0:
            # Archiver failed - fall back to direct output (preserves error semantics)
            tmp_path.unlink(missing_ok=True)
            result2 = subprocess.run([ar_exe, flags, str(output)] + inputs)
            sys.exit(result2.returncode)

        # Compare new archive content with existing
        new_hash = _hash_file(tmp_path)

        if new_hash and new_hash == old_hash:
            # Content identical! Don't replace the archive - preserve its mtime.
            # With restat=1 on the STATIC_LINKER rule, ninja will re-check output
            # mtime after this command. Since mtime is unchanged, ninja will NOT
            # propagate dirty to downstream DLL targets → no DLL relinking cascade!
            tmp_path.unlink(missing_ok=True)
            # Exit 0 so ninja considers the build step "successful"
            sys.exit(0)
        else:
            # Content changed (real code change) - replace the existing archive
            try:
                # Atomic replace: rename temp to output
                tmp_path.replace(output)
            except OSError:
                # rename failed (e.g., cross-device) - try copy+delete
                shutil.copy2(str(tmp_path), str(output))
                tmp_path.unlink(missing_ok=True)
            sys.exit(0)

    except KeyboardInterrupt:
        tmp_path.unlink(missing_ok=True)
        import _thread  # noqa: PLC0415

        _thread.interrupt_main()
    except Exception as e:
        # Unexpected error - clean up temp and fall back to direct archiving
        print(
            f"[ar_content_preserving] Warning: error during comparison: {e}",
            file=sys.stderr,
        )
        tmp_path.unlink(missing_ok=True)
        result = subprocess.run([ar_exe, flags, str(output)] + inputs)
        sys.exit(result.returncode)


if __name__ == "__main__":
    main()
