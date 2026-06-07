"""ESP32-S3 NEOPIXEL Blink flash-bloat regression gate (FastLED #2886 Stage 6).

Asserts `bash bloat esp32s3 --build` produces a `report.json` whose
`total_flash` is at most the pinned baseline in
`tests/data/esp32s3_bloat_baseline.txt`.

The baseline is a single integer (in bytes) that ratchets DOWN as each
later Stage of #2886 lands measurable savings. The intent is that any
PR which lands a saving lowers the file in the same change; any PR
which regresses fails this gate at CI.

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
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent
BASELINE_FILE = PROJECT_ROOT / "tests" / "data" / "esp32s3_bloat_baseline.txt"
REPORT_JSON = PROJECT_ROOT / ".build" / "symbols" / "esp32s3" / "report.json"


def read_baseline() -> int:
    raw = BASELINE_FILE.read_text(encoding="utf-8").strip()
    if not raw:
        print(
            f"esp32s3-bloat-regression: baseline file is empty: {BASELINE_FILE}",
            file=sys.stderr,
        )
        sys.exit(2)
    return int(raw)


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
        print(
            f"esp32s3-bloat-regression: PASS — total_flash={total_flash:,} B "
            f"<= baseline={baseline:,} B (headroom={-delta:,} B).",
            flush=True,
        )
        return 0

    print(
        f"esp32s3-bloat-regression: FAIL — total_flash={total_flash:,} B "
        f"exceeds baseline={baseline:,} B by {delta:,} B.",
        file=sys.stderr,
    )
    print(
        "esp32s3-bloat-regression: if the regression is intentional, update "
        f"{BASELINE_FILE.relative_to(PROJECT_ROOT).as_posix()} in the same PR. "
        "Otherwise inspect the top-N table:",
        file=sys.stderr,
    )
    print(
        "    bash bloat esp32s3 --top 25",
        file=sys.stderr,
    )
    print(
        "Then diff against the previous build with "
        ".claude/symbolaudit/diff.py (see agents/docs/binary-size-analysis.md).",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
