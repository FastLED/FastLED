#!/usr/bin/env python3
"""Probe a FastLED precompiled header (PCH) for identity and staleness problems.

Subcommands:
  inputs  Dump the headers a PCH recorded, and flag odd spellings and inputs
          newer than the PCH (stale).
  tu      Run one real PCH-consuming compile with -H and report every header
          the TU opened from disk although the PCH already contains it. Each
          such header is a double include; both spellings are printed.

Usage (from the repo root):
  uv run python .claude/skills/pch-debug/pch_probe.py inputs
  uv run python .claude/skills/pch-debug/pch_probe.py tu tests/fl/stl/adl_barrier.cpp
  uv run python .claude/skills/pch-debug/pch_probe.py tu adl_barrier --build-dir .build/meson-debug

Read-only: it never writes into the build tree (output goes to the null device).
"""

import argparse
import json
import os
import re
import shlex
import shutil
import sys
from pathlib import Path

from running_process import RunningProcess


DEFAULT_BUILD_DIR = Path(".build/meson-quick")
PCH_REL = Path("tests/test_pch.h.pch")


def canon(path: str, cwd: Path) -> str:
    """One spelling per physical file: absolute, resolved, forward slashes, case-folded on Windows."""
    p = Path(path)
    if not p.is_absolute():
        p = cwd / p
    return os.path.normcase(os.path.realpath(p)).replace("\\", "/")


# The repo root, from this script's location (.claude/skills/pch-debug/), so it
# does not depend on where --build-dir points.
REPO_ROOT = Path(__file__).resolve().parents[3]


def is_project_header(path: str, cwd: Path) -> bool:
    """True for a file under the repo's src/ or tests/, however it is spelled."""
    root = canon(str(REPO_ROOT), cwd)
    return canon(path, cwd).startswith((root + "/src/", root + "/tests/"))


def odd_spelling(path: str) -> str | None:
    if "\\" in path and "/" in path:
        return "mixed separators"
    if "\\" in path and os.name != "nt":
        return "backslash"
    if not (path.startswith("/") or re.match(r"^[A-Za-z]:[\\/]", path)):
        return "relative"
    for marker, why in (
        ("//", "double slash"),
        ("/./", "dot segment"),
        ("/../", "dotdot segment"),
    ):
        if marker in path[2:]:
            return why
    return None


def native_cpp(build_dir: Path) -> list[str]:
    """The C++ compiler from the build's meson native file (what the build really uses)."""
    native = build_dir / "meson_native.txt"
    if native.exists():
        m = re.search(
            r"^cpp\s*=\s*\[([^\]]*)\]", native.read_text(encoding="utf-8"), re.M
        )
        if m:
            return [s.strip().strip("'\"") for s in m.group(1).split(",") if s.strip()]
    sys.exit(f"no cpp entry in {native}; pass --build-dir of a configured meson build")


def run(args: list[str], cwd: Path) -> tuple[int, str]:
    proc = RunningProcess.run(
        args,
        cwd=cwd,
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    return proc.returncode, (proc.stdout or "") + (proc.stderr or "")


def bcanalyzer_inputs(pch: Path) -> dict[str, tuple[int, int]] | None:
    """Recorded inputs as {path: (size, mtime_seconds)}, read with llvm-bcanalyzer.

    These are the exact values clang compares against when it validates the
    PCH, and llvm-bcanalyzer reads them without validating, so it also works
    on the stale PCH clang refuses to load. None if the tool is not installed.
    """
    tool = os.environ.get("LLVM_BCANALYZER") or shutil.which("llvm-bcanalyzer")
    if not tool:
        return None
    rc, out = run([tool, "-dump", str(pch)], pch.parent)
    if rc != 0:
        return None
    recorded: dict[str, tuple[int, int]] = {}
    for m in re.finditer(
        r"<INPUT_FILE [^>]*op1=(\d+) op2=(\d+)[^>]*/> blob data = '([^']*)'", out
    ):
        recorded.setdefault(m.group(3), (int(m.group(1)), int(m.group(2))))
    return recorded or None


def pch_inputs(build_dir: Path, pch: Path) -> list[str]:
    recorded = bcanalyzer_inputs(pch)
    if recorded:
        return list(recorded)
    rc, out = run(
        native_cpp(build_dir)
        + [
            "-Xclang",
            "-module-file-info",
            str(pch),
            "-fsyntax-only",
            "-x",
            "c++",
            os.devnull,
        ],
        build_dir,
    )
    inputs = [
        line.split("Input file:", 1)[1].strip()
        for line in out.splitlines()
        if "Input file:" in line
    ]
    if inputs:
        # Entries can carry a trailing " [System]" / " [Overridden]" tag.
        return [re.sub(r"\s+\[[A-Za-z ]+\]$", "", i) for i in inputs]
    # clang validates the PCH even for -module-file-info, so a stale PCH (the
    # interesting case) cannot be dumped. Its refusal names the stale input and
    # both mtimes; print it, then recover the recorded paths from the PCH bytes
    # (input-file names are stored verbatim, as `strings` shows).
    refusal = [l for l in out.splitlines() if "fatal error" in l or "note:" in l]
    print("clang refused to load the PCH (this is itself the finding):")
    for line in refusal[:3]:
        print(f"  {line.strip()}")
    data = pch.read_bytes()
    found: dict[str, None] = {}
    for run_ in re.findall(rb"[\x20-\x7e]{6,}", data):
        for m in re.finditer(rb"(?:[A-Za-z]:)?[/\\][\x21-\x7e]*[^/\\\x20-\x2f]", run_):
            s = m.group(0).decode("ascii")
            if Path(s).is_file():
                found.setdefault(s, None)
    if not found:
        sys.exit(f"could not read {pch} (rc={rc}):\n{out[-2000:]}")
    print(f"(input paths recovered by scanning the PCH bytes: {len(found)})")
    return list(found)


def cmd_inputs(a: argparse.Namespace) -> int:
    build_dir = Path(a.build_dir).resolve()
    pch = build_dir / PCH_REL
    inputs = pch_inputs(build_dir, pch)
    pch_mtime = pch.stat().st_mtime_ns
    print(f"PCH: {pch}\nrecorded inputs: {len(inputs)}")
    odd = [(i, odd_spelling(i)) for i in inputs if odd_spelling(i)]
    # Relative entries resolve against the build dir, where the PCH was built.
    project_odd = [(i, w) for i, w in odd if is_project_header(i, build_dir)]
    print(f"odd spellings: {len(odd)} total, {len(project_odd)} in src/ or tests/")
    for i, why in project_odd[: a.limit]:
        print(f"  [{why}] {i}")
    seen: dict[str, str] = {}
    dups = []
    for i in inputs:
        c = canon(i, build_dir)
        if c in seen and seen[c] != i:
            dups.append((seen[c], i))
        seen.setdefault(c, i)
    print(f"same file recorded under two spellings: {len(dups)}")
    for x, y in dups[: a.limit]:
        print(f"  {x}\n    == {y}")

    # A recorded path is relative to the PCH build's working directory (the
    # build dir), never to wherever this script runs.
    def on_disk(i: str) -> Path:
        return Path(i) if Path(i).is_absolute() else build_dir / i

    missing = [i for i in inputs if not on_disk(i).exists()]
    recorded = bcanalyzer_inputs(pch)
    if recorded:
        # clang's own check: current size/mtime against what the PCH recorded.
        newer = []
        for i, (size, mtime) in recorded.items():
            if on_disk(i).exists():
                st = on_disk(i).stat()
                if st.st_size != size or int(st.st_mtime) != mtime:
                    newer.append(i)
                    print(
                        f"  changed since PCH: {i}\n"
                        f"    recorded size={size} mtime={mtime}, now size={st.st_size} mtime={int(st.st_mtime)}"
                    )
        print(
            f"inputs whose size/mtime differ from the PCH record (stale): {len(newer)}"
        )
    else:
        print(
            "(llvm-bcanalyzer not on PATH: falling back to 'newer than the PCH file')"
        )
        newer = [
            i
            for i in inputs
            if on_disk(i).exists() and on_disk(i).stat().st_mtime_ns > pch_mtime
        ]
        print(f"inputs newer than the PCH (stale): {len(newer)}")
        for i in newer[: a.limit]:
            print(f"  {i}")
    print(f"inputs that no longer exist: {len(missing)}")
    for i in missing[: a.limit]:
        print(f"  {i}")
    return 1 if (project_odd or dups or newer or missing) else 0


def cmd_tu(a: argparse.Namespace) -> int:
    build_dir = Path(a.build_dir).resolve()
    entries = json.loads(
        (build_dir / "compile_commands.json").read_text(encoding="utf-8")
    )
    matches = [e for e in entries if a.file in e["file"].replace("\\", "/")]
    if len(matches) != 1:
        sys.exit(
            f"{len(matches)} compile_commands entries match {a.file!r}; be more specific"
        )
    e = matches[0]
    args = (
        e["arguments"]
        if "arguments" in e
        else shlex.split(e["command"], posix=os.name != "nt")
    )
    clean: list[str] = []
    skip = 0
    for arg in args:
        if skip:
            skip -= 1
            continue
        if arg in ("-o", "-MF", "-MQ", "-MT"):
            skip = 1
            continue
        if arg in ("-MD", "-MMD", "-c"):
            continue
        clean.append(arg)
    # -fsyntax-only leaves link flags from clang driver config files unused;
    # same suppression the PCH build passes.
    clean += ["-fsyntax-only", "-H", "-Wno-unused-command-line-argument"]
    if a.zccache:
        clean = [a.zccache] + clean
    cwd = Path(e["directory"])
    rc, out = run(clean, cwd)
    inputs = pch_inputs(build_dir, build_dir / PCH_REL)
    in_pch = {canon(i, build_dir): i for i in inputs}
    # Project headers are all once-only (#pragma once), so re-opening one from
    # disk means clang did not match it to the PCH copy. System headers such
    # as <stddef.h> and bits/version.h are built to be entered repeatedly, so
    # they are only counted.
    reopened: dict[str, tuple[str, str]] = {}
    system_reopens = 0
    for line in out.splitlines():
        m = re.match(r"^\.+ (.+)$", line)
        if not m:
            continue
        c = canon(m.group(1), cwd)
        if c not in in_pch:
            continue
        if is_project_header(m.group(1), cwd):
            reopened.setdefault(c, (m.group(1), in_pch[c]))
        else:
            system_reopens += 1
    first_error = next((l for l in out.splitlines() if "error:" in l), "")
    print(f"TU: {e['file']}\nrc={rc}  first error: {first_error or '-'}")
    print(
        f"project headers re-opened from disk although the PCH has them: {len(reopened)}"
    )
    for tu_spelling, pch_spelling in list(reopened.values())[: a.limit]:
        same = "same spelling" if tu_spelling == pch_spelling else "DIFFERENT spelling"
        print(f"  TU : {tu_spelling}\n  PCH: {pch_spelling}   ({same})")
    print(
        f"(system headers re-entered, normal for <stddef.h>-style headers: {system_reopens})"
    )
    return 1 if (rc or reopened) else 0


def main() -> int:
    p = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    p.add_argument("--build-dir", default=str(DEFAULT_BUILD_DIR))
    p.add_argument("--limit", type=int, default=20, help="max lines per list")
    sub = p.add_subparsers(dest="cmd", required=True)
    sub.add_parser("inputs", help="dump and check the PCH's recorded inputs")
    t = sub.add_parser("tu", help="probe one PCH-consuming TU for double includes")
    t.add_argument("file", help="substring of the TU path in compile_commands.json")
    t.add_argument(
        "--zccache",
        help="path to zccache, to run the TU through the cache like ninja does",
    )
    a = p.parse_args()
    return cmd_inputs(a) if a.cmd == "inputs" else cmd_tu(a)


if __name__ == "__main__":
    sys.exit(main())
