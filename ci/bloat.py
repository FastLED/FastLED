"""Per-symbol bloat analysis wrapper around `fbuild symbols`.

Usage:
    bash bloat <board>                  # default: example=Blink, top=10
    bash bloat <board> --example FxFire # alternate example
    bash bloat esp32s3 --top 25         # deeper top-N table
    bash bloat esp32s3 --no-summary     # don't print the table (just artifacts)
    bash bloat esp32s3 --build          # also runs `bash compile` first

What it does:
    1. Resolves the cross-toolchain `nm` for the requested board (PIO
       packages directory). Until fbuild#428 ships, the bloat tool can't
       read these paths from `build_info.json` — we hard-code the well-
       known PIO toolchain layout instead.
    2. Locates the latest firmware.elf for the build:
       `.build/pio/<board>/.pio/build/<board>/firmware.elf` (PIO) or
       `.build/<board>/firmware.elf` (fbuild).
    3. Invokes `fbuild symbols` against it with `--output-dir
       .build/symbols/<board>/`, which writes BOTH `report.json` and
       `report.md` side by side.
    4. Parses the JSON, collapses each demangled name across all its
       (section, source) rows, and prints a `top-N` table to stdout.

Why this script exists:
    Running the analysis by hand wires you through the toolchain
    prefix, the PIO output path, the `--nm` override, and the output
    directory — easy to get wrong and easy to forget. This wrapper
    encapsulates the convention; future agents only need
    `bash bloat <board>` to get a useful report on disk.

Lessons baked in (see CLAUDE.md > "Binary Size Analysis"):
    - The fbuild `symbols` subcommand requires fbuild >= 2.2.19. The
      released 2.2.18 wheel does NOT carry it; sync to fbuild#main
      until the next release tag (#424 + #427 merged after 2.2.18).
    - Map-derived synthesis (fbuild #427) is what attributes anonymous
      `.rodata.<owner>.str1.<N>` blocks to the owning function. Without
      it, the biggest single-symbol contributor on ESP32-S3 Blink (the
      NEOPIXEL chipset ctor's FL_WARN/FL_LOG string pool, ~58 KB) shows
      up as anonymous bytes against main.cpp.o.
    - The dominant flash-bloat lever on ESP32-S3 is `FASTLED_LOG_VERBOSITY=0`
      (FastLED PR #2791). That single define recovers ~43-58 KB of FL_WARN
      string pool with no behavioural change for users who only need release-
      mode logging.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


# Per-board cross-toolchain `nm` resolution. The Xtensa / RISC-V split mirrors
# the PlatformIO board → architecture mapping; the file names follow
# `<triple>-nm[.exe]` per binutils convention.
#
# Until fbuild#428 lands (`BuildInfo` carries `nm_path` + `cppfilt_path`), we
# resolve directly against the PIO packages directory. After that lands the
# script can switch to reading `build_info_<env>.json::nm_path`.
PIO_NM_BY_ARCH = {
    # arch → (toolchain-package-dir, tool-prefix)
    "xtensa": ("toolchain-xtensa-esp-elf", "xtensa-{board_chip}-elf"),
    "riscv": ("toolchain-riscv32-esp-elf", "riscv32-esp-elf"),
}

# Board → chip-id used to construct the toolchain prefix. Add entries as
# bloat analysis is needed on new platforms.
BOARD_CHIP_MAP = {
    "esp32": ("xtensa", "esp32"),
    "esp32s2": ("xtensa", "esp32s2"),
    "esp32s3": ("xtensa", "esp32s3"),
    "esp32c3": ("riscv", "esp32c3"),
    "esp32c6": ("riscv", "esp32c6"),
    "esp32h2": ("riscv", "esp32h2"),
}


def pio_packages_dir() -> Path:
    """User-scoped PIO packages dir; works on Windows / Linux / macOS."""
    home = Path.home()
    candidate = home / ".platformio" / "packages"
    if candidate.is_dir():
        return candidate
    raise SystemExit(
        f"PlatformIO packages dir not found at {candidate}. "
        "Run a PIO build first (`bash compile <board> --examples Blink`)."
    )


def resolve_nm(board: str) -> Path:
    """Locate the cross-toolchain `nm` binary for the given board."""
    if board not in BOARD_CHIP_MAP:
        raise SystemExit(
            f"Bloat: board '{board}' has no nm mapping. Add it to "
            "BOARD_CHIP_MAP in ci/bloat.py — entries are mechanical "
            "(arch + chip prefix). Supported boards today: "
            f"{', '.join(sorted(BOARD_CHIP_MAP))}."
        )
    arch, chip = BOARD_CHIP_MAP[board]
    pkg_dir, prefix_tpl = PIO_NM_BY_ARCH[arch]
    prefix = prefix_tpl.format(board_chip=chip)
    bin_dir = pio_packages_dir() / pkg_dir / "bin"
    # Windows has .exe; Unix doesn't. Prefer the os-specific one.
    nm = bin_dir / f"{prefix}-nm.exe"
    if not nm.exists():
        nm = bin_dir / f"{prefix}-nm"
    if not nm.exists():
        raise SystemExit(
            f"Bloat: nm binary not found under {bin_dir}. Expected "
            f"`{prefix}-nm[.exe]`. Has the toolchain been installed via PIO?"
        )
    return nm


def find_elf(board: str, build_root: Path) -> Path:
    """Auto-detect the firmware ELF for the given board."""
    candidates = [
        build_root / "pio" / board / ".pio" / "build" / board / "firmware.elf",
        build_root / board / "firmware.elf",
    ]
    for c in candidates:
        if c.is_file():
            return c
    paths = "\n  ".join(str(c) for c in candidates)
    raise SystemExit(
        "Bloat: no firmware.elf found. Looked at:\n  "
        + paths
        + "\nRun `bash compile "
        + board
        + " --examples Blink` first, or pass "
        "--build."
    )


def assert_fbuild_has_symbols() -> None:
    """Refuse to proceed if `fbuild symbols` isn't wired up."""
    try:
        out = subprocess.check_output(
            ["fbuild", "symbols", "--help"],
            stderr=subprocess.STDOUT,
            text=True,
            timeout=15,
        )
    except FileNotFoundError as e:
        raise SystemExit(
            "Bloat: `fbuild` not on PATH. Run from a uv-managed shell "
            "(`uv run bash bloat <board>` or set the project venv)."
        ) from e
    except subprocess.CalledProcessError as e:
        raise SystemExit(
            "Bloat: `fbuild symbols` rejected. Your installed fbuild "
            "doesn't carry the symbols subcommand. Reinstall project "
            "dependencies (`uv sync`) — pyproject.toml pins a release "
            "that ships symbols/bloat. See agents/docs/binary-size-"
            "analysis.md for the upgrade path.\n\n"
            f"Output:\n{e.output}"
        ) from e
    if "symbols" not in out.lower() and "bloat" not in out.lower():
        # Sanity-check that the help text describes the right surface.
        raise SystemExit(
            "Bloat: `fbuild symbols --help` returned but doesn't mention "
            "the symbols/bloat subcommand. Likely a stale fbuild binary."
        )


def run_fbuild_symbols(elf: Path, nm: Path, out_dir: Path, top: int) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    cmd = [
        "fbuild",
        "symbols",
        str(elf),
        "--nm",
        str(nm),
        "--output-dir",
        str(out_dir),
        "--top",
        str(top),
    ]
    print(f"$ {' '.join(cmd)}")
    try:
        subprocess.run(cmd, check=True)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise


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
        help="Board name (esp32s3, esp32, esp32c3, ...). "
        "See BOARD_CHIP_MAP in ci/bloat.py to add a new target.",
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
        help="Run `bash compile <board> --examples <example> --platformio` "
        "first to produce a fresh ELF.",
    )
    parser.add_argument(
        "--build-root",
        default=".build",
        help="Build output root (default: .build).",
    )
    args = parser.parse_args()

    project_root = Path(__file__).resolve().parent.parent
    os.chdir(project_root)

    assert_fbuild_has_symbols()

    if args.build:
        compile_script = "compile.bat" if os.name == "nt" else "./compile"
        if os.name != "nt" and not shutil.which("bash"):
            raise SystemExit("bash not on PATH; cannot --build")
        cmd = [
            compile_script,
            args.board,
            "--examples",
            args.example,
            "--platformio",
        ]
        print(f"$ {' '.join(cmd)}")
        subprocess.run(cmd, check=True)

    nm = resolve_nm(args.board)
    elf = find_elf(args.board, Path(args.build_root))
    out_dir = Path(args.build_root) / "symbols" / args.board

    print(f"Board:   {args.board}")
    print(f"Example: {args.example}")
    print(f"ELF:     {elf}")
    print(f"nm:      {nm}")
    print(f"Output:  {out_dir}")
    print()

    run_fbuild_symbols(elf=elf, nm=nm, out_dir=out_dir, top=args.top)

    if not args.no_summary:
        print_summary(out_dir / "report.json", args.top)

    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
