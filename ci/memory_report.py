"""Per-target memory usage report for pull requests (#4920, #4933).

`collect` builds an example for each board through `ci/bloat.py` and records
the image flash and attributed RAM. `render` compares two such files and
prints one markdown table: the PR's size with its delta from the merge base.
`plan` picks the boards worth measuring from the pull request's changed files.

    uv run ci/memory_report.py collect --out head.json uno esp32s3
    uv run ci/memory_report.py render --base base.json --head head.json
    uv run ci/memory_report.py plan --changed-file changed.txt --labels ci-full
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from running_process import RunningProcess


MARKER = "<!-- fastled-memory-report -->"
METRICS = (("image_flash", "FLASH"), ("total_ram", "RAM"))

Sizes = dict[str, dict[str, int]]

# Boards measured for a change that can affect every platform. AVR is the
# cheapest target, so it stands in for common code (`src/fl/`,
# `src/third_party/`, shared platform helpers).
COMMON_BOARDS = ("attiny85",)

# One representative set per platform family. A platform-only change measures
# just its own family, so an ESP8266 fix no longer builds AVR or ESP32.
PLATFORM_BOARDS: dict[str, tuple[str, ...]] = {
    "avr": ("attiny85",),
    "esp8266": ("esp8266",),
    "esp32": ("esp32dev", "esp32s3"),
    "esp": ("esp8266", "esp32dev", "esp32s3"),
    "teensy": ("teensy41",),
    "rp": ("rp2040",),
    "nrf52": ("nrf52840_dk",),
    "stm32": ("stm32f103c8",),
}

# Everything `ci-full` (and a manual dispatch) measures.
ALL_BOARDS = (
    "attiny85",
    "uno",
    "esp8266",
    "esp32dev",
    "esp32s3",
    "esp32c3",
    "esp32c6",
    "teensy41",
    "rp2040",
    "nrf52840_dk",
    "stm32f103c8",
)


def plan(changed: list[str], labels: list[str], everything: bool) -> list[str]:
    """Boards to measure for a change set.

    Common code (including `src/fl/` and `src/third_party/`) can move any
    target, so it falls back to the cheap AVR target alone. Platform code
    measures only that platform; a mixed change measures both. WASM and stub
    changes measure nothing.
    """
    # Imported lazily: the measure job runs a copy of this file against the
    # merge base, which predates ci/changed_targets.py, and only needs
    # `collect` there.
    from ci.changed_targets import affected

    if everything:
        return list(ALL_BOARDS)
    boards: set[str] = set()
    common, platforms = affected(changed)
    if common:
        boards.update(COMMON_BOARDS)
    for platform in platforms:
        boards.update(PLATFORM_BOARDS.get(platform, ()))
    for label in labels:
        if label in ("ci-full", "ci-test:bloat"):
            boards.update(ALL_BOARDS)
        elif label.startswith("ci-platform:"):
            name = label.split(":", 1)[1]
            if name == "esp*":
                boards.update(PLATFORM_BOARDS["esp"])
            elif name in ALL_BOARDS:
                boards.add(name)
    return sorted(boards)


def collect(boards: list[str], example: str) -> Sizes:
    sizes: Sizes = {}
    for board in boards:
        # A board that fails to build is left out, so the report lists it as
        # missing instead of the whole comment disappearing.
        result = RunningProcess.run(
            [
                sys.executable,
                "ci/bloat.py",
                board,
                "--example",
                example,
                "--build",
                "--no-summary",
            ],
            check=False,
            timeout=3600,
        )
        if result.returncode != 0:
            print(f"memory_report: {board} failed to build; skipping", file=sys.stderr)
            continue
        report = json.loads(
            Path(".build/symbols", board, "report.json").read_text(encoding="utf-8")
        )
        sizes[board] = {key: int(report[key]) for key, _ in METRICS}
    return sizes


def _delta(delta: int) -> str:
    """`+120` in red, `-120` in green, and `0` in green (nothing changed)."""
    if delta == 0:
        return "🟢 0"
    mark = "🟢" if delta < 0 else "🔴"
    return f"{mark} {delta:+,}"


def _cell(size: int, base: int) -> str:
    return f"{size:,} ({_delta(size - base)})"


def render(base: Sizes, head: Sizes, example: str) -> str:
    lines = [
        MARKER,
        "### Memory usage (this PR vs. its merge base)",
        "",
        f"Change in `examples/{example}` per target. The left number is this PR's "
        "image flash / attributed RAM in bytes; the colored delta is the change "
        "from the merge base (red is larger, green is smaller or unchanged).",
        "",
        "| Target | FLASH [bytes] | RAM [bytes] |",
        "|---|---:|---:|",
    ]
    missing: list[str] = []
    compared = 0
    for board in sorted(set(base) | set(head)):
        if board not in base or board not in head:
            missing.append(board)
            continue
        compared += 1
        b, h = base[board], head[board]
        cells = [board]
        for key, _ in METRICS:
            cells.append(_cell(h[key], b[key]))
        lines.append("| " + " | ".join(cells) + " |")

    if compared == 0:
        # Both sides collected nothing, which means every selected target
        # failed to build; an empty table would read as "no change".
        lines += [
            "",
            "No comparison: none of the selected targets built on both sides "
            "of the change. See the `measure` job for the failing target.",
        ]
    elif missing:
        lines += [
            "",
            "No comparison (build missing on one side): "
            + ", ".join(f"`{m}`" for m in missing),
        ]
    return "\n".join(lines) + "\n"


def _merge(paths: list[Path]) -> Sizes:
    sizes: Sizes = {}
    for path in paths:
        sizes.update(json.loads(path.read_text(encoding="utf-8")))
    return sizes


def _changed_paths(path: Path) -> list[str]:
    return [
        line.strip()
        for line in path.read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    sub = parser.add_subparsers(dest="cmd", required=True)
    p_collect = sub.add_parser("collect", help="Build boards and record their sizes.")
    p_collect.add_argument("boards", nargs="+")
    p_collect.add_argument("--out", type=Path, required=True)
    p_collect.add_argument("--example", default="Blink")
    p_render = sub.add_parser("render", help="Print the markdown comparison.")
    p_render.add_argument("--base", type=Path, nargs="+", required=True)
    p_render.add_argument("--head", type=Path, nargs="+", required=True)
    p_render.add_argument("--example", default="Blink")
    p_plan = sub.add_parser("plan", help="Pick boards from the changed files.")
    p_plan.add_argument("--changed-file", type=Path, required=True)
    p_plan.add_argument(
        "--labels",
        default="",
        help="Comma-separated pull request labels.",
    )
    p_plan.add_argument(
        "--all",
        action="store_true",
        help="Measure every board (workflow_dispatch).",
    )
    args = parser.parse_args()

    if args.cmd == "collect":
        args.out.write_text(
            json.dumps(collect(args.boards, args.example), indent=2), encoding="utf-8"
        )
    elif args.cmd == "plan":
        labels = [item.strip() for item in args.labels.split(",") if item.strip()]
        boards = plan(_changed_paths(args.changed_file), labels, args.all)
        sys.stdout.write(json.dumps(boards) + "\n")
    else:
        sys.stdout.write(render(_merge(args.base), _merge(args.head), args.example))
    return 0


if __name__ == "__main__":
    sys.exit(main())
