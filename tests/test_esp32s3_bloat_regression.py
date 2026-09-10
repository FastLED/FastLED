"""ESP32-S3 NEOPIXEL Blink flash-bloat regression gate (FastLED #2886 Stage 6).

Asserts `bash bloat esp32s3 --build` produces a `report.json` whose
`total_flash` is at most the pinned baseline in
`tests/data/esp32s3_bloat_baseline.txt`.

THE BASELINE IS A RATCHET, AND IT MOVES IN BOTH DIRECTIONS

An earlier revision of this docstring said the baseline "ratchets DOWN"
and that "any PR which regresses fails this gate", while this script's
own failure message says to update the baseline when a regression is
intentional. Both cannot be the rule, and a contributor reading one and
a reviewer reading the other is how #4200 stalled. The rule in practice
is the second one -- #3944/#3945 took TM1812's measured +283 B into the
baseline -- because no policy that forbids every flash-costing feature
is a policy anyone runs.

So, stated once:

  * a build UNDER the baseline must re-pin it DOWN in the same PR. Not
    doing so is what let 232 B of slack accumulate before #4200, which
    then had to explain that the 197 B the gate printed was really 429 B
    against current master. A ratchet with slack is not a ratchet, so
    this gate now FAILS on unclaimed headroom rather than congratulating
    you on it;
  * a build OVER the baseline may re-pin it UP, in the same PR, with the
    reason written into the baseline file as a `#` comment. An upward
    move is a decision and should read like one in `git log`;
  * either way the file ends the PR equal to what the PR builds.

`kHeadroomTolerance` is the one piece of slack, and it is for build
noise rather than for savings: master measured the same total on three
consecutive runs, so the observed noise is zero, and this is well under
the 232 B drift that made #4200 unreadable.

USAGE

    uv run python tests/test_esp32s3_bloat_regression.py
    uv run python tests/test_esp32s3_bloat_regression.py --no-build
    uv run python tests/test_esp32s3_bloat_regression.py --baseline 350000

Exits 0 on pass. Exits 1 on regression (with a one-line summary of how
many bytes the current build is over the baseline). Exits 2 on
infrastructure failure (the bloat run itself fell over).

Run from the FastLED project root; this script changes directory
internally so the relative paths under `.build/` resolve.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from typeguard import typechecked


PROJECT_ROOT = Path(__file__).resolve().parent.parent
BASELINE_FILE = PROJECT_ROOT / "tests" / "data" / "esp32s3_bloat_baseline.txt"
REPORT_JSON = PROJECT_ROOT / ".build" / "symbols" / "esp32s3" / "report.json"


# Bytes of drift tolerated below the baseline before the gate calls it
# unclaimed headroom. Build noise, not savings -- see the docstring.
kHeadroomTolerance = 64


@typechecked
def parse_baseline(text: str) -> int | None:
    """The pinned byte count, ignoring `#` comments and blank lines.

    Comments exist so an upward move can carry its reason in the file that
    records it. Returns None when the text holds no number, which the caller
    reports rather than raising -- a malformed baseline is an infrastructure
    failure, not a regression, and the two exit differently.
    """

    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        try:
            return int(stripped)
        except ValueError:
            return None
    return None


def read_baseline() -> int:
    value = parse_baseline(BASELINE_FILE.read_text(encoding="utf-8"))
    if value is None:
        print(
            f"esp32s3-bloat-regression: baseline file holds no byte count: "
            f"{BASELINE_FILE}",
            file=sys.stderr,
        )
        sys.exit(2)
    return value


@typechecked
@dataclass(frozen=True, slots=True)
class FlashSymbol:
    """One sized symbol from the bloat report."""

    size: int
    name: str
    archive: str
    object_file: str


def largest_flash_symbols(report: dict[str, Any], count: int) -> list[FlashSymbol]:
    """The `count` biggest flash symbols in the report, largest first."""

    raw = report.get("symbols")
    if not isinstance(raw, list):
        return []
    sized: list[FlashSymbol] = []
    for entry in raw:
        if not isinstance(entry, dict):
            continue
        if entry.get("region") != "flash":
            continue
        size = entry.get("size")
        # `isinstance(True, int)` is true in Python, so a JSON `true` would
        # otherwise be accepted as a one-byte symbol and could displace a real
        # entry from the table.
        if isinstance(size, bool) or not isinstance(size, int) or size <= 0:
            continue
        name = entry.get("demangled") or entry.get("mangled") or "(anonymous)"
        sized.append(
            FlashSymbol(
                size=size,
                name=str(name),
                archive=str(entry.get("archive") or "(none)"),
                object_file=str(entry.get("object") or "(none)"),
            )
        )
    sized.sort(key=lambda symbol: symbol.size, reverse=True)
    return sized[:count]


def print_largest_symbols(report: dict[str, Any], count: int) -> None:
    """Print the biggest flash symbols this build produced.

    The gate has `report.json` in hand on the runner that built it, and used
    to tell the reader to reproduce the build locally instead -- which means
    an ESP32 toolchain and a three-minute compile to see data CI already had
    (#4165). Printing it turns "go and reproduce this" into "here is where
    the bytes went".
    """

    symbols = largest_flash_symbols(report, count)
    if not symbols:
        print(
            "esp32s3-bloat-regression: report.json carried no sized flash "
            "symbols, so there is no table to show.",
            file=sys.stderr,
        )
        return
    print(
        f"esp32s3-bloat-regression: {len(symbols)} largest flash symbols in "
        "this build:",
        file=sys.stderr,
    )
    print(f"    {'BYTES':>9}  {'ARCHIVE':<22} SYMBOL", file=sys.stderr)
    for symbol in symbols:
        name = symbol.name
        if len(name) > 88:
            name = name[:85] + "..."
        print(
            f"    {symbol.size:>9,}  {symbol.archive[:22]:<22} {name}",
            file=sys.stderr,
        )


def run_bloat(skip_build: bool) -> None:
    cmd: list[str] = ["bash", "bloat", "esp32s3"]
    if not skip_build:
        cmd.append("--build")
    cmd += ["--no-summary"]
    print(f"esp32s3-bloat-regression: invoking `{' '.join(cmd)}` ...", flush=True)
    result = subprocess.run(cmd, cwd=PROJECT_ROOT)
    if result.returncode != 0:
        print(
            f"esp32s3-bloat-regression: `bash bloat esp32s3` exited "
            f"{result.returncode} — cannot enforce regression gate.",
            file=sys.stderr,
        )
        sys.exit(2)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--no-build",
        action="store_true",
        help="Skip `--build`; assume the ELF + report.json already exist.",
    )
    parser.add_argument(
        "--baseline",
        type=int,
        default=None,
        help=(
            "Override the pinned baseline (bytes). Default: read from "
            "tests/data/esp32s3_bloat_baseline.txt."
        ),
    )
    args = parser.parse_args()

    baseline = args.baseline if args.baseline is not None else read_baseline()

    run_bloat(skip_build=args.no_build)

    if not REPORT_JSON.exists():
        print(
            f"esp32s3-bloat-regression: report.json missing after bloat run: "
            f"{REPORT_JSON}",
            file=sys.stderr,
        )
        return 2

    data = json.loads(REPORT_JSON.read_text(encoding="utf-8"))
    total_flash = int(data["total_flash"])

    delta = total_flash - baseline
    if delta <= 0:
        headroom = -delta
        if headroom > kHeadroomTolerance:
            print(
                f"esp32s3-bloat-regression: FAIL — total_flash="
                f"{total_flash:,} B is {headroom:,} B UNDER "
                f"baseline={baseline:,} B.",
                file=sys.stderr,
            )
            print(
                "esp32s3-bloat-regression: a saving has to be claimed. Set "
                f"{BASELINE_FILE.relative_to(PROJECT_ROOT).as_posix()} to "
                f"{total_flash} in this PR. Left unclaimed, the slack hides "
                "the next regression inside it -- which is exactly what "
                "happened before #4200, where 232 B of stale headroom made a "
                "429 B cost print as 197 B.",
                file=sys.stderr,
            )
            return 1
        print(
            f"esp32s3-bloat-regression: PASS — total_flash={total_flash:,} B "
            f"<= baseline={baseline:,} B (headroom={headroom:,} B).",
            flush=True,
        )
        return 0

    print(
        f"esp32s3-bloat-regression: FAIL — total_flash={total_flash:,} B "
        f"exceeds baseline={baseline:,} B by {delta:,} B.",
        file=sys.stderr,
    )
    print(
        "esp32s3-bloat-regression: if the cost is intentional, set "
        f"{BASELINE_FILE.relative_to(PROJECT_ROOT).as_posix()} to "
        f"{total_flash} in this PR and write the reason into that file as a "
        "`#` comment -- an upward move is a decision and should read like one "
        "in git log. Otherwise, this is where the flash went:",
        file=sys.stderr,
    )
    print_largest_symbols(data, 25)
    print(
        "esp32s3-bloat-regression: to compare against another build, run "
        "`bash bloat esp32s3 --top 25` locally and diff with "
        ".claude/symbolaudit/diff.py (see agents/docs/binary-size-analysis.md).",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
