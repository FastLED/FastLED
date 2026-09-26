"""ESP32-S3 NEOPIXEL Blink flash-bloat regression gate (FastLED #2886 Stage 6).

Asserts `bash bloat esp32s3 --build` produces a `report.json` whose
`image_flash` (the firmware image size from ELF section headers; #4468)
is at most the pinned baseline in
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
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from running_process import RunningProcess
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


@typechecked
def comment_lines(text: str) -> tuple[str, ...]:
    """The `#` lines of a baseline file, in order."""

    found: list[str] = []
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith("#"):
            found.append(stripped)
    return tuple(found)


@typechecked
def raise_lacks_reason(previous_text: str, current_text: str) -> bool:
    """True when the baseline moved UP and the file did not say why.

    Documenting the requirement is not the same as having it. Without this a
    PR could raise the number, match it, and pass -- which is the state the
    docstring above was written to end, so leaving it unenforced would have
    reproduced the original problem in a new place.

    The comparison is against the *previous* comments rather than against
    "has any comment", so a reason left over from an earlier raise cannot
    stand in for this one. Comparing full lines rather than counting them
    means an edit in place counts as well as an addition.
    """

    previous = parse_baseline(previous_text)
    current = parse_baseline(current_text)
    if previous is None or current is None:
        # A malformed baseline is an infrastructure failure and is reported
        # separately; calling it a missing reason would name the wrong fault.
        return False
    if current <= previous:
        return False
    return comment_lines(current_text) == comment_lines(previous_text)


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


# Entry points every bound colour pipeline links, and that Blink -- which binds
# no profile -- must not. The pipeline is reached only through the hooks
# `setColorProfile` installs (`ColorPipelineHooks`), so a sketch that never
# binds one pays nothing for it (#4455). The byte gate would catch a leak only
# as an unexplained few KB; this names it. These are the stable anchors, not
# every symbol: any leak reaches at least one of them.
kUnboundPipelineSymbols = (
    "fl::installColorPipelineHooks",
    "fl::buildStreamingPipelineQ16",
    "fl::processPixelQ16",
    "fl::buildGamutMapQ16",
    "fl::mapAndSolveDrivesQ16",
    "fl::buildRgbSolveMatrixFromQ16",
    "fl::q16FromFloatBits",
    "fl::detail::five_bit_hd_solve16",
)
kUnboundPipelinePrefixes = ("fl::ColorManagedPixelSource::",)


@typechecked
def linked_pipeline_symbols(report: dict[str, Any]) -> list[str]:
    """Colour-pipeline symbols the build linked, sorted and deduplicated.

    Matches on the demangled name up to its parameter list, so an overload or
    a signature change still counts. A symbol row without a demangled name
    raises ValueError rather than being skipped: a mangled name cannot match
    these anchors, so skipping it would let a leak pass unseen.
    """

    raw = report.get("symbols")
    if not isinstance(raw, list):
        raise ValueError("report.json has no `symbols` list")
    found: set[str] = set()
    for entry in raw:
        if not isinstance(entry, dict):
            raise ValueError(f"report.json symbol row is not an object: {entry!r}")
        name = entry.get("demangled")
        if not isinstance(name, str) or not name:
            raise ValueError(
                "report.json symbol row has no demangled name, so it cannot "
                f"be checked for pipeline code: {entry.get('mangled')!r}"
            )
        qualified = name.split("(", 1)[0]
        if qualified in kUnboundPipelineSymbols or qualified.startswith(
            kUnboundPipelinePrefixes
        ):
            found.add(name)
    return sorted(found)


@typechecked
def gate_flash(report: dict[str, Any]) -> int:
    """The byte count this gate compares: `image_flash` from report.json.

    `image_flash` is the sum of allocated, file-backed ELF sections, read by
    fbuild from section headers (FastLED/fbuild#1456). `total_flash` is the sum
    of attributed symbol rows; it depended on which `nm` ran and put a local
    build ~4 KB over CI for the same image (#4468), so it is not used here.
    A missing or malformed value raises ValueError: falling back to
    `total_flash` would compare a different metric against the baseline.
    """

    value = report.get("image_flash")
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise ValueError(
            f"report.json has no usable `image_flash` (got {value!r}); fbuild "
            "older than the pyproject.toml pin? Run `uv sync`."
        )
    return value


def run_bloat(skip_build: bool) -> None:
    cmd: list[str] = ["bash", "bloat", "esp32s3"]
    if not skip_build:
        cmd.append("--build")
    cmd += ["--no-summary"]
    print(f"esp32s3-bloat-regression: invoking `{' '.join(cmd)}` ...", flush=True)
    result = RunningProcess.run(cmd, cwd=PROJECT_ROOT)
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
        "--previous-baseline",
        type=str,
        default=None,
        help=(
            "Path to the baseline file as it stands on the merge base. When "
            "given, a raise without a new `#` reason fails."
        ),
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

    if args.previous_baseline is not None:
        previous_text = Path(args.previous_baseline).read_text(encoding="utf-8")
        current_text = BASELINE_FILE.read_text(encoding="utf-8")
        if raise_lacks_reason(previous_text, current_text):
            print(
                "esp32s3-bloat-regression: FAIL — the baseline was raised from "
                f"{parse_baseline(previous_text)} to "
                f"{parse_baseline(current_text)} with no reason recorded.",
                file=sys.stderr,
            )
            print(
                "esp32s3-bloat-regression: add a `#` comment to "
                f"{BASELINE_FILE.relative_to(PROJECT_ROOT).as_posix()} saying "
                "what bought the bytes. Going up is allowed; going up silently "
                "is what makes the next reader unable to tell a decision from "
                "a slip.",
                file=sys.stderr,
            )
            return 1

    run_bloat(skip_build=args.no_build)

    if not REPORT_JSON.exists():
        print(
            f"esp32s3-bloat-regression: report.json missing after bloat run: "
            f"{REPORT_JSON}",
            file=sys.stderr,
        )
        return 2

    data = json.loads(REPORT_JSON.read_text(encoding="utf-8"))
    try:
        image_flash = gate_flash(data)
    except ValueError as error:
        print(f"esp32s3-bloat-regression: {error}", file=sys.stderr)
        return 2

    try:
        leaked = linked_pipeline_symbols(data)
    except ValueError as error:
        print(f"esp32s3-bloat-regression: {error}", file=sys.stderr)
        return 2
    if leaked:
        print(
            "esp32s3-bloat-regression: FAIL — Blink binds no colour profile "
            "but links colour-pipeline code:",
            file=sys.stderr,
        )
        for name in leaked:
            print(f"    {name}", file=sys.stderr)
        print(
            "esp32s3-bloat-regression: the pipeline must be reached only "
            "through ColorPipelineHooks, which setColorProfile installs. "
            "Something now calls it directly (#4455).",
            file=sys.stderr,
        )
        return 1

    delta = image_flash - baseline
    if delta <= 0:
        headroom = -delta
        if headroom > kHeadroomTolerance:
            print(
                f"esp32s3-bloat-regression: FAIL — image_flash="
                f"{image_flash:,} B is {headroom:,} B UNDER "
                f"baseline={baseline:,} B.",
                file=sys.stderr,
            )
            print(
                "esp32s3-bloat-regression: a saving has to be claimed. Set "
                f"{BASELINE_FILE.relative_to(PROJECT_ROOT).as_posix()} to "
                f"{image_flash} in this PR. Left unclaimed, the slack hides "
                "the next regression inside it -- which is exactly what "
                "happened before #4200, where 232 B of stale headroom made a "
                "429 B cost print as 197 B.",
                file=sys.stderr,
            )
            return 1
        print(
            f"esp32s3-bloat-regression: PASS — image_flash={image_flash:,} B "
            f"<= baseline={baseline:,} B (headroom={headroom:,} B).",
            flush=True,
        )
        return 0

    print(
        f"esp32s3-bloat-regression: FAIL — image_flash={image_flash:,} B "
        f"exceeds baseline={baseline:,} B by {delta:,} B.",
        file=sys.stderr,
    )
    print(
        "esp32s3-bloat-regression: if the cost is intentional, set "
        f"{BASELINE_FILE.relative_to(PROJECT_ROOT).as_posix()} to "
        f"{image_flash} in this PR and write the reason into that file as a "
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
