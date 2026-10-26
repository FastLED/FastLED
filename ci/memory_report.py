"""Per-target memory usage report for pull requests (#4920).

`collect` builds an example for each board through `ci/bloat.py` and records
the image flash and attributed RAM. `render` compares two such files and
prints a markdown table in the style of arduino-esp32's size-deltas comment.

    uv run ci/memory_report.py collect --out head.json uno esp32s3
    uv run ci/memory_report.py render base.json head.json
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


def _bytes(delta: int) -> str:
    if delta == 0:
        return "0"
    mark = "💚" if delta < 0 else "⚠️"
    text = f"{delta:+,}"
    if abs(delta) >= 10_000:
        text = f"{delta / 1024:+.0f}K"
    return f"{mark} {text}"


def _percent(delta: int, base: int) -> str:
    if delta == 0 or base == 0:
        return "0.00"
    mark = "💚" if delta < 0 else "⚠️"
    return f"{mark} {delta * 100 / base:+.2f}"


def render(base: Sizes, head: Sizes, example: str) -> str:
    lines = [
        MARKER,
        "### Memory usage (this PR vs. its merge base)",
        "",
        f"Change in `examples/{example}` per target: image flash and attributed RAM, "
        "in bytes and as a percentage of the merge base. Negative is smaller.",
        "",
        "| Target | FLASH [bytes] | FLASH [%] | RAM [bytes] | RAM [%] |",
        "|---|---:|---:|---:|---:|",
    ]
    missing: list[str] = []
    for board in sorted(set(base) | set(head)):
        if board not in base or board not in head:
            missing.append(board)
            continue
        cells = [board]
        for key, _ in METRICS:
            delta = head[board][key] - base[board][key]
            cells += [_bytes(delta), _percent(delta, base[board][key])]
        lines.append("| " + " | ".join(cells) + " |")

    lines += ["", "<details><summary>Absolute sizes [bytes]</summary>", ""]
    lines += [
        "| Target | FLASH base | FLASH PR | RAM base | RAM PR |",
        "|---|---:|---:|---:|---:|",
    ]
    for board in sorted(set(base) & set(head)):
        b, h = base[board], head[board]
        lines.append(
            f"| {board} | {b['image_flash']:,} | {h['image_flash']:,} | {b['total_ram']:,} | {h['total_ram']:,} |"
        )
    lines += ["", "</details>"]
    if missing:
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
    args = parser.parse_args()

    if args.cmd == "collect":
        args.out.write_text(
            json.dumps(collect(args.boards, args.example), indent=2), encoding="utf-8"
        )
    else:
        sys.stdout.write(render(_merge(args.base), _merge(args.head), args.example))
    return 0


if __name__ == "__main__":
    sys.exit(main())
