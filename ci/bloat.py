"""Per-symbol bloat analysis wrapper around `fbuild symbols`.

Usage:
    bash bloat <board>                  # default: example=Blink, top=10
    bash bloat <board> --example FxFire # alternate example
    bash bloat esp32s3 --top 25         # deeper top-N table
    bash bloat lpc845brk --top 25       # ARM Cortex-M target
    bash bloat esp32s3 --no-summary     # don't print the table (just artifacts)
    bash bloat esp32s3 --build          # also runs `bash compile` first

What it does:
    1. Locates the newest firmware.elf fbuild produced for the board, in
       either of the layouts fbuild writes:
         - `.fbuild/build/<board>/release/firmware.elf` (standalone fbuild)
         - `.build/fbuild/<board>/.fbuild/build/<release|debug>/firmware.elf`
           (`bash compile <board>`)
    2. Invokes `fbuild symbols` against it. `fbuild symbols` auto-locates
       the sibling `build_info_<board>.json` (emitted by fbuild after every
       link) and reads `nm_path` from it. This works for ANY architecture
       fbuild can build — Xtensa, RISC-V, ARM Cortex-M — with zero
       board-specific configuration here.
    3. Writes `report.json` and `report.md` to `.build/symbols/<board>/`.
    4. Parses the JSON, collapses each demangled name across all its
       (section, source) rows, and prints a `top-N` table to stdout.

Why this script exists:
    Running the analysis by hand wires you through the build output path
    and the output directory — easy to get wrong and easy to forget. This
    wrapper encapsulates the convention; future agents only need
    `bash bloat <board>` to get a useful report on disk.

Lessons baked in (see agents/docs/binary-size-analysis.md):
    - The fbuild `symbols` subcommand requires fbuild >= 2.2.19 and the
      `build_info.json`-driven nm resolution requires fbuild >= 2.2.20
      (FastLED/fbuild#428). pyproject.toml pins a release that ships
      both.
    - Map-derived synthesis (fbuild #427) is what attributes anonymous
      `.rodata.<owner>.str1.<N>` blocks to the owning function. Without
      it, the biggest single-symbol contributor on ESP32-S3 Blink (the
      NEOPIXEL chipset ctor's FL_WARN/FL_LOG string pool, ~58 KB) shows
      up as anonymous bytes against main.cpp.o.
    - The dominant flash-bloat lever on ESP32-S3 is `FASTLED_LOG_VERBOSITY=0`
      (FastLED PR #2791). That single define recovers ~43-58 KB of FL_WARN
      string pool with no behavioural change for users who only need release-
      mode logging.
    - Over-budget builds: `bash bloat <board>` will retry the build with
      `fbuild build --bloat-analysis` (which sets `-Wl,--noinhibit-exec`)
      so the ELF survives even when the linker reports a region overflow.
      See FastLED/fbuild#594.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

from running_process import PIPE, STDOUT, CalledProcessError, RunningProcess

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


# Board build directory root used by `bash compile`; mirrors
# `ci/compiler/path_manager.py::FastLEDPaths.build_dir`.
BOARD_BUILD_SUBDIR = "fbuild"


@dataclass
class ElfLocation:
    """Where an ELF was found."""

    elf: Path


# See `_assert_fresh`: the coarsest common filesystem mtime granularity.
_MTIME_GRANULARITY_SLACK_S = 2.0


def _assert_fresh(location: ElfLocation, build_started: float | None) -> None:
    """Refuse to analyse an ELF older than the build that just ran.

    `--build` promises "a fresh ELF"; without this the promise is unchecked,
    and a layout the build did not write can be analysed instead while every
    number looks plausible. FastLED#4384.
    """

    if build_started is None:
        return
    mtime = location.elf.stat().st_mtime
    # Slack for coarse mtime granularity. `time.time()` is sub-microsecond,
    # but a filesystem may store mtimes to the second (ext3, HFS+) or to two
    # seconds (FAT/exFAT), so an ELF written moments after the build began can
    # carry a timestamp rounded below it. Two seconds covers the coarsest of
    # those while still refusing the artifact this guard exists for, which was
    # five days stale.
    if mtime >= build_started - _MTIME_GRANULARITY_SLACK_S:
        return
    age = build_started - mtime
    raise SystemExit(
        f"Bloat: --build ran, but the ELF selected for analysis predates it by "
        f"{age / 3600:.1f} h:\n  {location.elf}\n"
        "That is a stale artifact from an earlier build, so the numbers would "
        "describe a binary this run did not produce. Remove it, or rebuild."
    )


def elf_candidates(board: str, build_root: Path) -> list[Path]:
    """Every location fbuild may have written `firmware.elf` for `board`."""
    project_root = Path.cwd()
    board_dir = build_root / BOARD_BUILD_SUBDIR / board / ".fbuild" / "build"
    return [
        project_root / ".fbuild" / "build" / board / "release" / "firmware.elf",
        board_dir / "release" / "firmware.elf",
        board_dir / "debug" / "firmware.elf",
        board_dir / board / "release" / "firmware.elf",
    ]


def find_elf(board: str, build_root: Path) -> ElfLocation:
    """Auto-detect the firmware ELF for the given board.

    Newest wins, with the order of `elf_candidates` as the tie-break.

    It used to be priority alone, and that is how this command came to
    report five-day-old numbers for a change made minutes earlier: several
    layouts coexist under one board directory, and a fixed preference
    outranked the one `--build` had just written whatever its age. Two runs
    across a real code change produced byte-identical output, including
    total_flash. FastLED#4384.
    """
    candidates = elf_candidates(board, build_root)
    found = [(path, path.stat().st_mtime) for path in candidates if path.is_file()]
    if found:
        newest = max(mtime for _, mtime in found)
        for path, mtime in found:
            if mtime == newest:
                return ElfLocation(elf=path)

    paths = "\n  ".join(str(c) for c in candidates)
    raise SystemExit(
        "Bloat: no firmware.elf found. Looked at:\n  "
        + paths
        + "\nRun `bash compile "
        + board
        + " --examples Blink` first, or pass --build."
    )


def assert_fbuild_has_symbols() -> None:
    """Refuse to proceed if `fbuild symbols` isn't wired up."""
    try:
        out = RunningProcess.run(
            ["fbuild", "symbols", "--help"],
            stdout=PIPE,
            stderr=STDOUT,
            text=True,
            timeout=15,
            check=True,
            encoding="utf-8",
            errors="replace",
        ).stdout
    except (FileNotFoundError, RuntimeError) as e:
        raise SystemExit(
            "Bloat: `fbuild` not on PATH. Run from a uv-managed shell "
            "(`uv run bash bloat <board>` or set the project venv)."
        ) from e
    except CalledProcessError as e:
        raise SystemExit(
            "Bloat: `fbuild symbols` rejected. Your installed fbuild "
            "doesn't carry the symbols subcommand. Reinstall project "
            "dependencies (`uv sync`) — pyproject.toml pins a release "
            "that ships symbols/bloat. See agents/docs/binary-size-"
            "analysis.md for the upgrade path.\n\n"
            f"Output:\n{e.output}"
        ) from e
    if "symbols" not in out.lower() and "bloat" not in out.lower():
        raise SystemExit(
            "Bloat: `fbuild symbols --help` returned but doesn't mention "
            "the symbols/bloat subcommand. Likely a stale fbuild binary."
        )


def run_fbuild_symbols(location: ElfLocation, out_dir: Path, top: int) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    cmd = [
        "fbuild",
        "symbols",
        str(location.elf),
        "--output-dir",
        str(out_dir),
        "--top",
        str(top),
    ]
    print(f"$ {' '.join(cmd)}")
    try:
        RunningProcess.run(cmd, check=True)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise


def run_fbuild_build_bloat(board: str, example: str) -> int:
    """Build with `--bloat-analysis` so over-budget links still emit an ELF.

    Returns the build's exit code. A non-zero exit is *expected* on
    over-budget builds — the linker still reports the region overflow —
    but `firmware.elf` survives for nm-based analysis. See
    FastLED/fbuild#594.
    """
    env = os.environ.copy()
    # fbuild reads the sketch directory from this environment variable.
    env["PLATFORMIO_SRC_DIR"] = str(Path("examples") / example)
    cmd = ["fbuild", "build", "-e", board, "--bloat-analysis"]
    print(f"$ {' '.join(cmd)}  (sketch={env['PLATFORMIO_SRC_DIR']})")
    try:
        result = RunningProcess.run(cmd, env=env)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    return result.returncode


@dataclass
class _AggBucket:
    """Per-demangled-name aggregation row for the summary table."""

    size: int = 0
    archive: str = ""
    object: str = ""
    sources: set[str] = field(default_factory=lambda: set[str]())


def print_summary(report_json: Path, top: int) -> None:
    """Read report.json and print a top-N table of flash bloat."""
    data = json.loads(report_json.read_text(encoding="utf-8"))
    flash_syms = [s for s in data["symbols"] if s["region"] == "flash"]
    total_flash: int = int(data["total_flash"])
    total_ram: int = int(data["total_ram"])

    by_name: dict[str, _AggBucket] = {}
    for s in flash_syms:
        n: str = s["demangled"]
        bucket = by_name.setdefault(n, _AggBucket())
        bucket.size += int(s["size"])
        if not bucket.archive:
            bucket.archive = s.get("archive") or "(none)"
            bucket.object = s.get("object") or "-"
        bucket.sources.add(s["source"])

    rows: list[tuple[str, _AggBucket]] = sorted(
        by_name.items(), key=lambda kv: -kv[1].size
    )[:top]

    print()
    print(
        f"Total flash: {total_flash:,} B  "
        f"({len(flash_syms):,} sized flash symbols).  "
        f"RAM: {total_ram:,} B."
    )
    print()
    print(f"{'#':>3}  {'BYTES':>8}  {'ARCHIVE':<22}  {'OBJECT':<26}  SYMBOL")
    print("-" * 120)
    for i, (name, bucket) in enumerate(rows, 1):
        srcs = "/".join(sorted(bucket.sources))
        shown = name if len(name) <= 70 else name[:67] + "..."
        print(
            f"{i:>3}  {bucket.size:>8,}  "
            f"{bucket.archive[:22]:<22}  {bucket.object[:26]:<26}  "
            f"{shown}  [{srcs}]"
        )
    print()
    print("Artifacts:")
    print(f"  JSON: {report_json}")
    print(f"  MD:   {report_json.with_name('report.md')}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Per-symbol flash/RAM bloat analysis for FastLED builds."
    )
    parser.add_argument(
        "board",
        help="Board name (esp32s3, esp32, esp32c3, lpc845brk, ...).",
    )
    parser.add_argument(
        "--example",
        default="Blink",
        help="Example whose build to analyze (default: Blink).",
    )
    parser.add_argument(
        "--top",
        type=int,
        default=10,
        help="Top-N symbols to print in the summary (default: 10).",
    )
    parser.add_argument(
        "--no-summary",
        action="store_true",
        help="Skip the stdout summary; just write the JSON + MD artifacts.",
    )
    parser.add_argument(
        "--build",
        action="store_true",
        help="Run `bash compile <board> --examples <example>` first to "
        "produce a fresh ELF.",
    )
    parser.add_argument(
        "--build-root",
        default=".build",
        help="Build output root (default: .build).",
    )
    parser.add_argument(
        "--allow-overflow",
        action="store_true",
        help="When the build is over-budget, retry with "
        "`fbuild build --bloat-analysis` so the ELF survives the region "
        "overflow and bloat analysis can still run. See "
        "FastLED/fbuild#594.",
    )
    args = parser.parse_args()

    project_root = Path(__file__).resolve().parent.parent
    os.chdir(project_root)

    assert_fbuild_has_symbols()

    build_started: float | None = None
    if args.build:
        compile_script = "compile.bat" if os.name == "nt" else "./compile"
        if os.name != "nt" and not shutil.which("bash"):
            raise SystemExit("bash not on PATH; cannot --build")
        cmd = [compile_script, args.board, "--examples", args.example]
        print(f"$ {' '.join(cmd)}")
        build_started = time.time()
        RunningProcess.run(cmd, check=True)

    try:
        location = find_elf(args.board, Path(args.build_root))
    except SystemExit:
        if not args.allow_overflow:
            raise
        # Retry: build with --bloat-analysis so the over-budget ELF survives.
        print(
            f"Bloat: no ELF for '{args.board}'. Retrying via "
            "`fbuild build --bloat-analysis` (FastLED/fbuild#594)..."
        )
        rc = run_fbuild_build_bloat(args.board, args.example)
        if rc != 0:
            print(
                f"Bloat: `fbuild build --bloat-analysis` exited {rc} "
                "(expected for over-budget builds). Checking for ELF..."
            )
        location = find_elf(args.board, Path(args.build_root))

    # Outside the block above on purpose. That `except SystemExit` means
    # "no ELF was found, retry with --allow-overflow"; a stale-ELF refusal
    # raised inside it would be caught and rebuilt as though the ELF were
    # missing, which is the opposite of what it is. Checked here so it covers
    # both the normal and the recovery selection.
    _assert_fresh(location, build_started)

    out_dir = Path(args.build_root) / "symbols" / args.board

    print(f"Board:   {args.board}")
    print(f"Example: {args.example}")
    print(f"ELF:     {location.elf}")
    print("nm:      (auto-resolved by fbuild via build_info_<board>.json)")
    print(f"Output:  {out_dir}")
    print()

    run_fbuild_symbols(location=location, out_dir=out_dir, top=args.top)

    if not args.no_summary:
        print_summary(out_dir / "report.json", args.top)

    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
