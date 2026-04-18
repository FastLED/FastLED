"""Meson `-I` path normalization check (issue #2328).

On Windows, Meson's `include_directories()` propagates `-I` flags using native
backslash + relative path spellings (e.g. `-I..\\..\\src\\platforms\\stub`).
Clang keys `#pragma once` deduplication on the raw resolved path string — if
the same directory reaches a translation unit via two different spellings
(forward slash vs backslash, absolute vs relative, with/without `/./`), the
header appears to be two different files and `#pragma once` is silently
defeated, producing double-definition errors (see #2324, #2325).

The fix is to make every `-I` flag we generate be absolute + forward-slash +
spelled identically everywhere. This test inspects the generated
`compile_commands.json` after `meson setup` and fails if any `-I` flag
contains a backslash or is not absolute.

Run via:
    bash test                              # all python tests, incl. this one
    uv run pytest ci/tests/test_meson_include_paths.py -v
"""

from __future__ import annotations

import json
import re
import unittest
from pathlib import Path

from ci.util.paths import BUILD, PROJECT_ROOT


# Directories under .build/ where meson may have written a compile_commands.json.
# We check whichever ones exist at test time; we do NOT invoke meson setup here
# (that is done by the `bash test` driver / developer). Test is a no-op if no
# compile_commands.json files are present, so fresh clones don't fail here.
_MESON_BUILD_DIRS = [
    BUILD / "meson",
    BUILD / "meson-quick",
    BUILD / "meson-debug",
    BUILD / "meson-release",
    BUILD / "meson-wasm-quick",
]

# Matches a single -I flag token. compile_commands.json "command" strings
# emit tokens wrapped in double-quotes like `"-Ici/foo"` or `"-IC:/foo"` on
# Windows, so we match between quotes as well as bare -Ifoo. The captured
# path value never includes the surrounding quotes.
_I_FLAG_RE = re.compile(r'"-I([^"]+)"|(?<!\S)-I(\S+)')

# Whitelisted -I flags that come from Meson itself, not our meson.build files.
# Meson unconditionally adds four kinds of -I for every target; we cannot
# remove them without patching Meson. They are harmless because our canonical
# absolute forward-slash -I flags are tried first during header resolution:
#
#   1. "-I<target-name>.p"         — private scratch dir for the target
#                                     (holds .obj files, no headers)
#   2. "-I<path-to-build-subdir>"  — e.g. "tests", "ci/meson/native"
#                                     (build output subdir, usually empty)
#   3. "-I<path-to-source-subdir>" — e.g. "..\..\tests", "..\..\examples"
#                                     (source subdir containing the meson.build)
#   4. "-I." / "-I<up-relative>"   — build root and project root
#
# These are always *relative* to the build dir and cannot be normalized by us.
# The PCH is built with absolute forward-slash spellings, so consumer compiles
# resolve the same absolute path first; pragma-once dedup works despite these.
#
# We whitelist them by shape: any relative path that either
#   (a) ends in .p (Meson's "target private scratch"), or
#   (b) is a known source/build subdirectory (examples, tests, tests/profile,
#       ci/meson/*, or the plain build-dir markers "." / "..").
_MESON_AUTO_SUBDIRS = {
    ".",
    "..",
    "../..",
    "examples",
    "tests",
    "tests/profile",
    "ci/meson",
    "ci/meson/native",
    "ci/meson/shared",
    "ci/meson/wasm",
}
_MESON_P_SCRATCH_RE = re.compile(r"(?:^|/)[A-Za-z0-9._+-]+\.p$")


def _extract_includes_from_entry(entry: dict[str, object]) -> list[str]:
    """Extract -I <path> values from a compile_commands.json entry."""
    raw = entry.get("command") or " ".join(entry.get("arguments", []))  # type: ignore[arg-type]
    assert isinstance(raw, str)
    out: list[str] = []
    for m in _I_FLAG_RE.finditer(raw):
        out.append(m.group(1) or m.group(2))
    return out


def _is_meson_auto_include(include_path: str) -> bool:
    """Return True if this -I path is Meson-injected (not user-controlled).

    We normalize forward/back slashes before matching so the function behaves
    the same regardless of how Meson spelled the path on a given platform.
    """
    norm = include_path.replace("\\", "/").rstrip("/")
    # (a) Private per-target scratch dir (contains only .obj files)
    if _MESON_P_SCRATCH_RE.search(norm):
        return True
    # (b) Known Meson source/build subdirs; also accept them with a
    #     leading "../../" prefix (Meson relativises them to the build root).
    if norm.startswith("../../"):
        norm = norm[len("../../") :]
    return norm in _MESON_AUTO_SUBDIRS


def _is_absolute_forward_slash(path: str) -> bool:
    """Return True if `path` is absolute (POSIX `/...` or Windows `X:/...`)
    and contains no backslashes.
    """
    if "\\" in path:
        return False
    # Windows drive letter
    if re.match(r"^[A-Za-z]:/", path):
        return True
    # POSIX absolute
    if path.startswith("/"):
        return True
    return False


class TestMesonIncludePaths(unittest.TestCase):
    """Every -I flag in every compile_commands.json must be absolute +
    forward-slash + spelled identically everywhere.
    """

    def _check_build_dir(self, build_dir: Path) -> None:
        cc_json = build_dir / "compile_commands.json"
        if not cc_json.exists():
            self.skipTest(f"No compile_commands.json in {build_dir}")

        with open(cc_json, "r", encoding="utf-8") as f:
            data = json.load(f)

        offenders_backslash: dict[str, list[str]] = {}
        offenders_relative: dict[str, list[str]] = {}

        for entry in data:
            source_file = entry.get("file", "<unknown>")
            assert isinstance(source_file, str)
            try:
                source_rel = str(Path(source_file).relative_to(PROJECT_ROOT))
            except ValueError:
                source_rel = source_file

            for inc in _extract_includes_from_entry(entry):
                if _is_meson_auto_include(inc):
                    continue
                if "\\" in inc:
                    offenders_backslash.setdefault(inc, []).append(source_rel)
                elif not _is_absolute_forward_slash(inc):
                    offenders_relative.setdefault(inc, []).append(source_rel)

        if offenders_backslash or offenders_relative:
            lines = [
                f"Non-normalized -I flags found in {cc_json}:",
                "",
                "See https://github.com/FastLED/FastLED/issues/2328 — all -I flags",
                "must be absolute + forward-slash. Backslashes or relative paths",
                "defeat clang's #pragma once dedup on Windows.",
                "",
            ]
            if offenders_backslash:
                lines.append(
                    f"  Backslash in path ({len(offenders_backslash)} distinct):"
                )
                for flag, sources in sorted(offenders_backslash.items())[:10]:
                    lines.append(
                        f"    {flag}  (seen in {len(sources)} TUs, e.g. {sources[0]})"
                    )
                if len(offenders_backslash) > 10:
                    lines.append(f"    ... {len(offenders_backslash) - 10} more")
                lines.append("")
            if offenders_relative:
                lines.append(
                    f"  Non-absolute path ({len(offenders_relative)} distinct):"
                )
                for flag, sources in sorted(offenders_relative.items())[:10]:
                    lines.append(
                        f"    {flag}  (seen in {len(sources)} TUs, e.g. {sources[0]})"
                    )
                if len(offenders_relative) > 10:
                    lines.append(f"    ... {len(offenders_relative) - 10} more")
            self.fail("\n".join(lines))

    def test_all_meson_build_dirs_have_normalized_includes(self) -> None:
        """Check every known meson build dir. Skip any that don't exist yet."""
        checked_any = False
        for build_dir in _MESON_BUILD_DIRS:
            if not (build_dir / "compile_commands.json").exists():
                continue
            checked_any = True
            with self.subTest(build_dir=build_dir.name):
                self._check_build_dir(build_dir)

        if not checked_any:
            self.skipTest(
                "No meson compile_commands.json found under .build/; "
                "run `bash test --cpp` first to generate one."
            )


if __name__ == "__main__":
    unittest.main()
