"""Meson strict path normalization check (issues #2328, #2376).

On Windows, native backslash and relative path spellings can defeat clang's
file identity checks and are rejected by zccache strict path mode. This test
inspects generated Meson compile databases and fails if any path-bearing
compiler flag would be rejected by ``ZCCACHE_STRICT_PATHS=absolute``.

Run via:
    bash test
    uv run pytest ci/tests/test_meson_include_paths.py -v
"""

from __future__ import annotations

import json
import re
import shlex
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

_SEPARATE_PATH_FLAGS = {
    "-I",
    "-F",
    "-isystem",
    "-iquote",
    "-idirafter",
    "-iframework",
    "-imsvc",
    "-include",
    "-include-pch",
    "-imacros",
    "/I",
}
_ATTACHED_PATH_PREFIXES = ("-I", "-F", "/I")


def _tokens_from_entry(entry: dict[str, object]) -> list[str]:
    raw_args = entry.get("arguments")
    if isinstance(raw_args, list):
        return [str(arg) for arg in raw_args]

    raw = entry.get("command", "")
    assert isinstance(raw, str)
    return shlex.split(raw, posix=True)


def _extract_strict_path_flags(entry: dict[str, object]) -> list[tuple[str, str]]:
    """Extract path-bearing compiler flags checked by zccache strict mode."""
    tokens = _tokens_from_entry(entry)
    out: list[tuple[str, str]] = []
    i = 0
    while i < len(tokens):
        token = tokens[i]
        if token in _SEPARATE_PATH_FLAGS and i + 1 < len(tokens):
            out.append((token, tokens[i + 1]))
            i += 2
            continue

        attached = next(
            (
                prefix
                for prefix in _ATTACHED_PATH_PREFIXES
                if token.startswith(prefix) and len(token) > len(prefix)
            ),
            None,
        )
        if attached is not None:
            out.append((attached, token[len(attached) :]))
        i += 1
    return out


def _is_absolute_forward_slash(path: str) -> bool:
    if "\\" in path:
        return False
    if re.match(r"^[A-Za-z]:/", path):
        return True
    return path.startswith("/")


def _path_violation(path: str) -> str | None:
    if "\\" in path:
        return "backslash in path"
    if not _is_absolute_forward_slash(path):
        return "non-absolute path"

    parts = [part for part in re.split(r"/+", path) if part]
    if "." in parts or ".." in parts:
        return "dot path component"
    return None


class TestMesonIncludePaths(unittest.TestCase):
    """Every path-bearing compile flag must satisfy zccache strict mode."""

    def _check_build_dir(self, build_dir: Path) -> None:
        cc_json = build_dir / "compile_commands.json"
        if not cc_json.exists():
            self.skipTest(f"No compile_commands.json in {build_dir}")

        with open(cc_json, "r", encoding="utf-8") as f:
            data = json.load(f)

        offenders: dict[tuple[str, str, str], list[str]] = {}

        for entry in data:
            source_file = entry.get("file", "<unknown>")
            assert isinstance(source_file, str)
            try:
                source_rel = str(Path(source_file).relative_to(PROJECT_ROOT))
            except ValueError:
                source_rel = source_file

            assert isinstance(entry, dict)
            for flag, path in _extract_strict_path_flags(entry):
                violation = _path_violation(path)
                if violation is not None:
                    offenders.setdefault((flag, path, violation), []).append(source_rel)

        if offenders:
            lines = [
                f"Non-normalized compiler path flags found in {cc_json}:",
                "",
                "See https://github.com/FastLED/FastLED/issues/2328 and #2376.",
                "All path-bearing compiler flags must satisfy",
                "ZCCACHE_STRICT_PATHS=absolute: absolute, forward-slash, and no",
                "dot path components.",
                "",
            ]
            for (flag, path, violation), sources in sorted(offenders.items())[:20]:
                lines.append(
                    f"    {flag} {path}  ({violation}; seen in {len(sources)} TUs, e.g. {sources[0]})"
                )
            if len(offenders) > 20:
                lines.append(f"    ... {len(offenders) - 20} more")
            self.fail("\n".join(lines))

    def test_all_meson_build_dirs_have_normalized_includes(self) -> None:
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
