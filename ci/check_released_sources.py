"""Fail if a compiled source file from the previous release is gone.

Arduino builds every .c/.cpp/.S under src/. A user who unpacks a new
FastLED over an old copy keeps the files the new release deleted or
renamed, and a stale unity unit then redefines symbols (FastLED #4704:
fl.system.sd+.cpp vs fl.fs.sd+.cpp). Any such path must ship as an empty
tombstone so the overlay overwrites it. Needs the release tags (full clone).
"""

import argparse
import re
import sys

from running_process import RunningProcess


_COMPILED = re.compile(r"^src/.*\.(c|cpp|S)$")
_RELEASE_TAG = re.compile(r"^\d+\.\d+\.\d+$")


def _git(*args: str) -> list[str]:
    out = RunningProcess.run(
        ["git", *args],
        check=True,
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        timeout=60,
    ).stdout
    return [line for line in out.splitlines() if line]


def compiled_sources(rev: str) -> set[str]:
    return {
        p
        for p in _git("ls-tree", "-r", "--name-only", rev, "src")
        if _COMPILED.match(p)
    }


def previous_release(rev: str) -> str | None:
    tags = _git("tag", "--merged", rev)
    releases = [t for t in tags if _RELEASE_TAG.match(t)]
    head = _git("rev-parse", f"{rev}^{{commit}}")[0]
    releases = [t for t in releases if _git("rev-parse", f"{t}^{{commit}}")[0] != head]
    if not releases:
        return None
    return max(releases, key=lambda t: tuple(int(x) for x in t.split(".")))


def missing_sources(old: set[str], new: set[str]) -> list[str]:
    return sorted(old - new)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rev", default="HEAD")
    parser.add_argument("--base", help="release tag to compare with (default: newest)")
    args = parser.parse_args()
    base = args.base or previous_release(args.rev)
    if base is None:
        print(
            "No earlier release tag found; fetch tags (fetch-depth: 0).",
            file=sys.stderr,
        )
        return 1
    missing = missing_sources(compiled_sources(base), compiled_sources(args.rev))
    if missing:
        print(
            f"Compiled sources shipped in {base} are missing at {args.rev}:",
            file=sys.stderr,
        )
        for path in missing:
            print(f"  {path}", file=sys.stderr)
        print(
            "Ship each as an empty tombstone file so an in-place upgrade "
            "overwrites the stale copy (see src/fl/build/fl.system.sd+.cpp).",
            file=sys.stderr,
        )
        return 1
    print(f"OK: every compiled source from {base} is still shipped.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
