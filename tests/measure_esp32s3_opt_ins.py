"""Multi-config ESP32-S3 NEOPIXEL Blink bloat audit driver (FastLED #2905).

Builds the ESP32-S3 Blink under each release-mode opt-in combo from
#2886 (baseline, +Stage 2 boot-banner shim, +Stage 3 sdkconfig
overlay, +Stage 4 RMT static allocation, and the full "stack"), runs
`bash bloat esp32s3` against each ELF, and emits a Markdown comparison
table.

The output table maps 1:1 onto rows 1-4 of the
`docs/SLIM_ESP32S3.md` priority table and records bloat totals, symbol
count, and the whole `firmware.bin` size for each configuration.

USAGE

    uv run python tests/measure_esp32s3_opt_ins.py
    uv run python tests/measure_esp32s3_opt_ins.py --config stage2
    uv run python tests/measure_esp32s3_opt_ins.py --config all --out compare.md
    uv run python tests/measure_esp32s3_opt_ins.py --keep-ini-backup

Exits 0 on success. Exits 1 if any requested config's build / bloat
run fails. Exits 2 on infrastructure failure (missing project ini,
missing bash compile, etc.).

The script's first action is to back up the root `platformio.ini`
(fbuild's project-file format) to `platformio.ini.bak` and install
signal handlers so a Ctrl-C mid-run restores the file before exit. The
build / bloat invocations go through the same `bash compile` /
`bash bloat` wrappers the rest of the project uses.

Run from the FastLED project root.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import signal
import sys
from dataclasses import dataclass
from pathlib import Path

from running_process import STDOUT, RunningProcess


PROJECT_ROOT = Path(__file__).resolve().parent.parent
PROJECT_INI = PROJECT_ROOT / "platformio.ini"
PROJECT_INI_BACKUP = PROJECT_ROOT / "platformio.ini.bak"
REPORT_JSON = PROJECT_ROOT / ".build" / "symbols" / "esp32s3" / "report.json"
OVERLAY_PATH = "tools/sdkconfig_for_smallest_fastled.defaults"


@dataclass(frozen=True)
class OptInConfig:
    """One opt-in combo to measure.

    Attributes:
        name: short name (e.g. "stage2"); also the doc-row key.
        label: human-readable description shown in the comparison table.
        defines: compile-time defines passed directly to `bash compile`.
            Entries use `NAME=VALUE` form without a leading `-D`.
        overlay: True iff sdkconfig_for_smallest_fastled.defaults must
            be appended to board_build.sdkconfig_defaults.
        slim_row: priority-table row in docs/SLIM_ESP32S3.md that
            this config corresponds to (None for stacked combos).
    """

    name: str
    label: str
    defines: tuple[str, ...]
    overlay: bool
    slim_row: int | None = None


# All opt-in build flags we measure. The flag set extends over time as
# new SLIM rows ship; each new row should add a per-config entry below
# plus appear in `max_savings`. See #2906 (original) and #2933 (this
# extension that added the 4 post-#2906 gates + max_savings).
_ALL_OPT_IN_DEFINES: tuple[str, ...] = (
    "FASTLED_SUPPRESS_ARDUINO_CHIP_DEBUG_REPORT=1",
    "FL_RMT_STATIC_ALLOCATION=1",
    "FASTLED_DISABLE_SPI_CHIPSETS=1",
    "FASTLED_DISABLE_UCS7604=1",
    "FASTLED_DISABLE_DYNAMIC_DRIVER=1",
    "FASTLED_DISABLE_CHANNEL_EVENTS=1",
)

CONFIGS: dict[str, OptInConfig] = {
    "baseline": OptInConfig(
        name="baseline",
        label="Stage 1 only (NDEBUG default - `FASTLED_LOG_VERBOSITY=0`)",
        defines=(),
        overlay=False,
        slim_row=1,
    ),
    "stage2": OptInConfig(
        name="stage2",
        label="+ Stage 2 (`FASTLED_SUPPRESS_ARDUINO_CHIP_DEBUG_REPORT=1`)",
        defines=("FASTLED_SUPPRESS_ARDUINO_CHIP_DEBUG_REPORT=1",),
        overlay=False,
        slim_row=4,
    ),
    "stage3": OptInConfig(
        name="stage3",
        label="+ Stage 3 (`tools/sdkconfig_for_smallest_fastled.defaults` overlay)",
        defines=(),
        overlay=True,
        slim_row=2,
    ),
    "stage4": OptInConfig(
        name="stage4",
        label="+ Stage 4 (`FL_RMT_STATIC_ALLOCATION=1`)",
        defines=("FL_RMT_STATIC_ALLOCATION=1",),
        overlay=False,
        slim_row=3,
    ),
    "stack": OptInConfig(
        name="stack",
        label="SLIM TL;DR base stack (Stages 2 + 3 + 4)",
        defines=(
            "FASTLED_SUPPRESS_ARDUINO_CHIP_DEBUG_REPORT=1",
            "FL_RMT_STATIC_ALLOCATION=1",
        ),
        overlay=True,
        slim_row=None,
    ),
    "disable_spi": OptInConfig(
        name="disable_spi",
        label="+ #2914 (`FASTLED_DISABLE_SPI_CHIPSETS=1`)",
        defines=("FASTLED_DISABLE_SPI_CHIPSETS=1",),
        overlay=False,
        slim_row=6,
    ),
    "disable_ucs7604": OptInConfig(
        name="disable_ucs7604",
        label="+ #2921 (`FASTLED_DISABLE_UCS7604=1`)",
        defines=("FASTLED_DISABLE_UCS7604=1",),
        overlay=False,
        slim_row=7,
    ),
    "disable_dyndriver": OptInConfig(
        name="disable_dyndriver",
        label="+ #2927 (`FASTLED_DISABLE_DYNAMIC_DRIVER=1`)",
        defines=("FASTLED_DISABLE_DYNAMIC_DRIVER=1",),
        overlay=False,
        slim_row=8,
    ),
    "disable_events": OptInConfig(
        name="disable_events",
        label="+ #2932 (`FASTLED_DISABLE_CHANNEL_EVENTS=1`)",
        defines=("FASTLED_DISABLE_CHANNEL_EVENTS=1",),
        overlay=False,
        slim_row=9,
    ),
    "max_savings": OptInConfig(
        name="max_savings",
        label="Max-savings stack (Stages 2/3/4 + #2914 + #2921 + #2927 + #2932)",
        defines=_ALL_OPT_IN_DEFINES,
        overlay=True,
        slim_row=None,
    ),
}


def backup_project_ini() -> None:
    if not PROJECT_INI.is_file():
        print(
            f"measure-opt-ins: platformio.ini not found at {PROJECT_INI}",
            file=sys.stderr,
        )
        sys.exit(2)
    shutil.copyfile(PROJECT_INI, PROJECT_INI_BACKUP)


def restore_project_ini(keep_backup: bool) -> None:
    if PROJECT_INI_BACKUP.is_file():
        shutil.copyfile(PROJECT_INI_BACKUP, PROJECT_INI)
        if not keep_backup:
            PROJECT_INI_BACKUP.unlink()


def patch_project_ini(cfg: OptInConfig) -> None:
    """Inject cfg's sdkconfig overlay into the [env:esp32s3] block.

    This is a targeted regex patch - we don't fully re-parse INI
    because the project INI flavor carries continuation lines and
    `${var.x}` interpolations that configparser mangles. The regex
    target is the literal `[env:esp32s3]` header through the next
    `[env:` header (or EOF), which is well-defined in practice.
    """
    text = PROJECT_INI.read_text(encoding="utf-8")
    block_re = re.compile(r"(\[env:esp32s3\][^\[]*?)(?=\n\[env:|\Z)", re.DOTALL)
    match = block_re.search(text)
    if not match:
        print(
            "measure-opt-ins: could not locate [env:esp32s3] block in platformio.ini",
            file=sys.stderr,
        )
        sys.exit(2)
    block = match.group(1)
    if cfg.overlay:
        line = f"board_build.sdkconfig_defaults = {OVERLAY_PATH}\n"
        if "board_build.sdkconfig_defaults" in block:
            block = re.sub(
                r"board_build\.sdkconfig_defaults\s*=.*",
                line.rstrip(),
                block,
            )
        else:
            block = block.rstrip() + "\n" + line
    new_text = text[: match.start()] + block + text[match.end() :]
    PROJECT_INI.write_text(new_text, encoding="utf-8")


def _run_compile_cmd(example: str, cfg: OptInConfig) -> list[str]:
    """Pick the right compile entry point per platform.

    On Linux/macOS: `bash compile esp32s3 --examples <example>`.
    On Windows: `<PROJECT_ROOT>/compile.bat esp32s3 --examples <example>`
    (absolute path - Windows process spawning does not
    auto-resolve `.bat` files from the cwd argument). `bash` is not on
    PATH in a non-WSL Windows Python launched by `uv run`, and there is
    no `bash.exe` in the project; the project ships `compile.bat` for
    this case. See #2935.
    """
    import os

    args = ["esp32s3", "--examples", example]
    if cfg.defines:
        args.extend(("--defines", ",".join(cfg.defines)))
    if os.name == "nt":
        return [str(PROJECT_ROOT / "compile.bat"), *args]
    return ["bash", "compile", *args]


def _run_bloat_cmd() -> list[str]:
    """Pick the right bloat entry point per platform.

    On Linux/macOS: `bash bloat esp32s3 --no-summary`.
    On Windows: `uv run ci/bloat.py esp32s3 --no-summary` - there is no
    `bloat.bat` shim, so we invoke the underlying Python entry point
    directly (the bash wrapper just does `uv run ci/bloat.py "$@"`).
    See #2935.
    """
    import os

    args = ["esp32s3", "--no-summary"]
    if os.name == "nt":
        return ["uv", "run", "ci/bloat.py", *args]
    return ["bash", "bloat", *args]


_LOG_DIR = PROJECT_ROOT / ".build" / "measure-logs"


def _run_with_log(cmd: list[str], log_name: str) -> bool:
    """Run `cmd` in PROJECT_ROOT, tee output to a per-config log file.

    Pre-#2938 versions of the script swallowed each build's stdout/stderr,
    so when a config crashed mid-run (#2935 saw 4 of 10 configs crash on
    Windows with `extras+.cpp.o` exit 3221225794), there was no way to
    diagnose without a manual rerun. This wrapper writes the combined
    stream to `.build/measure-logs/<log_name>` while also printing it to
    the operator's terminal. See #2938.
    """
    _LOG_DIR.mkdir(parents=True, exist_ok=True)
    log_path = _LOG_DIR / log_name
    print(f"measure-opt-ins: $ {' '.join(cmd)}", flush=True)
    print(
        f"measure-opt-ins:   (log: {log_path.relative_to(PROJECT_ROOT).as_posix()})",
        flush=True,
    )
    with log_path.open("wb") as log_f:
        proc = RunningProcess(
            cmd,
            cwd=PROJECT_ROOT,
            auto_run=True,
            capture=True,
            stderr=STDOUT,
            text=False,
        )
        for line in proc.line_iter(timeout=None):
            chunk = (line if isinstance(line, bytes) else line.encode("utf-8")) + b"\n"
            log_f.write(chunk)
            sys.stdout.buffer.write(chunk)
            sys.stdout.buffer.flush()
        proc.wait()
    return proc.returncode == 0


def run_compile(example: str, cfg: OptInConfig) -> bool:
    return _run_with_log(_run_compile_cmd(example, cfg), f"{cfg.name}-compile.log")


def run_bloat(config_name: str) -> bool:
    return _run_with_log(_run_bloat_cmd(), f"{config_name}-bloat.log")


def archive_report(cfg: OptInConfig) -> Path:
    dst = REPORT_JSON.with_name(f"report.{cfg.name}.json")
    shutil.copyfile(REPORT_JSON, dst)
    return dst


_ELF_ROOT = PROJECT_ROOT / ".build" / "fbuild" / "esp32s3" / ".fbuild" / "build"


@dataclass(frozen=True)
class BuildMeasurement:
    total_flash: int
    total_ram: int
    symbol_count: int
    firmware_bin_bytes: int


def read_measurement() -> BuildMeasurement | None:
    """Read symbol totals and whole-firmware size from the completed build."""
    if not REPORT_JSON.is_file():
        return None
    data = json.loads(REPORT_JSON.read_text(encoding="utf-8"))
    symbols = data.get("symbols")
    if not isinstance(symbols, list):
        return None
    firmware_bins = list(_ELF_ROOT.glob("**/firmware.bin"))
    if not firmware_bins:
        return None
    firmware_bin = max(firmware_bins, key=lambda path: path.stat().st_mtime_ns)
    try:
        return BuildMeasurement(
            total_flash=int(data["total_flash"]),
            total_ram=int(data["total_ram"]),
            symbol_count=len(symbols),
            firmware_bin_bytes=firmware_bin.stat().st_size,
        )
    except (KeyError, TypeError, ValueError):
        return None


def _force_relink() -> None:
    """Delete the prior ELF so the link step actually runs.

    The build cache returns cached .o files on consecutive runs with
    overlapping object hashes - that's the right speed/correctness
    trade-off. But a link step that decides whether to relink based on
    object-file timestamps, NOT on project ini / sdkconfig changes, can
    leave the previous config's ELF on disk when switching configs that
    affect only sdkconfig (e.g. stage3 <-> baseline) - and the downstream
    `bash bloat` step then measures that stale ELF.

    Deleting the ELF before each compile forces the link to run; the
    cached objects make the rebuild fast anyway. See #2940.
    """
    for elf in _ELF_ROOT.glob("**/firmware.elf"):
        elf.unlink()
        firmware_bin = elf.with_suffix(".bin")
        if firmware_bin.is_file():
            firmware_bin.unlink()


def measure_one(cfg: OptInConfig, example: str) -> BuildMeasurement | None:
    """Build + bloat one config; return its measured sizes or None on failure."""
    print(f"\n=== measure-opt-ins: config {cfg.name} ({cfg.label}) ===", flush=True)
    restore_project_ini(keep_backup=True)
    patch_project_ini(cfg)
    _force_relink()
    if not run_compile(example, cfg):
        print(f"measure-opt-ins: compile failed for {cfg.name}", file=sys.stderr)
        return None
    if not run_bloat(cfg.name):
        print(f"measure-opt-ins: bloat run failed for {cfg.name}", file=sys.stderr)
        return None
    measurement = read_measurement()
    if measurement is None:
        print(
            f"measure-opt-ins: incomplete report or firmware.bin after {cfg.name}",
            file=sys.stderr,
        )
        return None
    archived = archive_report(cfg)
    print(
        f"measure-opt-ins: {cfg.name}: flash={measurement.total_flash:,} B, "
        f"RAM={measurement.total_ram:,} B, symbols={measurement.symbol_count:,}, "
        f"firmware.bin={measurement.firmware_bin_bytes:,} B "
        f"(report archived: {archived.relative_to(PROJECT_ROOT).as_posix()})",
        flush=True,
    )
    return measurement


def format_table(results: dict[str, BuildMeasurement]) -> str:
    baseline = results.get("baseline")
    lines = [
        "| Config | SLIM row | Label | Flash | Flash delta | RAM | RAM delta | Symbols | Symbol delta | firmware.bin | Bin delta |",
        "|---|:---:|---|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for name, cfg in CONFIGS.items():
        if name not in results:
            continue
        measurement = results[name]
        flash_delta = (
            "" if baseline is None else f"{measurement.total_flash - baseline.total_flash:+,} B"
        )
        ram_delta = (
            "" if baseline is None else f"{measurement.total_ram - baseline.total_ram:+,} B"
        )
        symbol_delta = (
            "" if baseline is None else f"{measurement.symbol_count - baseline.symbol_count:+,}"
        )
        firmware_delta = (
            ""
            if baseline is None
            else f"{measurement.firmware_bin_bytes - baseline.firmware_bin_bytes:+,} B"
        )
        row = "-" if cfg.slim_row is None else f"{cfg.slim_row}"
        lines.append(
            f"| `{name}` | {row} | {cfg.label} | {measurement.total_flash:,} B | "
            f"{flash_delta} | {measurement.total_ram:,} B | {ram_delta} | "
            f"{measurement.symbol_count:,} | {symbol_delta} | "
            f"{measurement.firmware_bin_bytes:,} B | {firmware_delta} |"
        )
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--config",
        default="all",
        help=(
            "Comma-separated list of configs to run, or `all`. Available: "
            + ", ".join(CONFIGS.keys())
        ),
    )
    parser.add_argument(
        "--example",
        default="Blink",
        help="Example sketch to build (default: Blink).",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=None,
        help="If set, write the Markdown comparison table to this path.",
    )
    parser.add_argument(
        "--keep-ini-backup",
        action="store_true",
        help="Keep platformio.ini.bak around after the run (default: delete on success).",
    )
    args = parser.parse_args()

    if args.config == "all":
        requested = list(CONFIGS.keys())
    else:
        requested = [c.strip() for c in args.config.split(",") if c.strip()]
        for name in requested:
            if name not in CONFIGS:
                print(
                    f"measure-opt-ins: unknown config '{name}'. Available: "
                    + ", ".join(CONFIGS.keys()),
                    file=sys.stderr,
                )
                return 2

    backup_project_ini()

    def _restore_and_exit(*_: object) -> None:
        restore_project_ini(keep_backup=args.keep_ini_backup)
        sys.exit(130)

    signal.signal(signal.SIGINT, _restore_and_exit)
    if hasattr(signal, "SIGTERM"):
        signal.signal(signal.SIGTERM, _restore_and_exit)

    results: dict[str, BuildMeasurement] = {}
    failed: list[str] = []
    try:
        for name in requested:
            total = measure_one(CONFIGS[name], args.example)
            if total is None:
                failed.append(name)
            else:
                results[name] = total
    finally:
        restore_project_ini(keep_backup=args.keep_ini_backup)

    print()
    print("=== Measured comparison ===")
    table = format_table(results)
    print(table)

    if args.out is not None:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(table, encoding="utf-8")
        print(f"measure-opt-ins: wrote table to {args.out}")

    if failed:
        print(
            f"measure-opt-ins: FAIL - config(s) failed to measure: {', '.join(failed)}",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
