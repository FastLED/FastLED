import argparse
import os
import sys
from pathlib import Path


# Cap how much of optimization_report.txt we dump to stdout. With -fopt-info-all
# applied across the full example matrix (~110 sketches on nrf52840_dk), the
# report file can exceed 100MB. Printing the entire blob to a GitHub Actions
# log triggers log-buffer limits and an external runner shutdown signal that
# fails the workflow even though the build itself succeeded — see nrf52840_dk
# run 26705682371 / 26705682359 (feather + supermini), where the build step
# succeeded but Optimization Report cancelled at 37s with the runner dying
# soon after. Cap to the LAST 1 MiB (most-recently emitted optimizer info,
# which is the most actionable for size-driven debugging).
#
# Override via FL_OPT_REPORT_MAX_BYTES env var (set to 0 to disable the cap)
# or the --max-bytes flag. Long-term, this script should print only a summary
# and let the workflow upload optimization_report.txt as a build artifact for
# devs to download — tracked as a follow-up.
DEFAULT_MAX_REPORT_BYTES = 1 * 1024 * 1024


def _env_default_max_bytes() -> int:
    raw = os.environ.get("FL_OPT_REPORT_MAX_BYTES")
    if raw is None:
        return DEFAULT_MAX_REPORT_BYTES
    try:
        value = int(raw)
    except ValueError:
        return DEFAULT_MAX_REPORT_BYTES
    return max(value, 0)


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Inspect the GCC -fopt-info-all optimization report for a board."
    )
    parser.add_argument("--first", action="store_true", help="Inspect the first board")
    parser.add_argument("--cwd", type=Path, help="Custom working directory")
    parser.add_argument(
        "--max-bytes",
        type=int,
        default=_env_default_max_bytes(),
        help=(
            f"Cap report tail printed to stdout (default "
            f"{DEFAULT_MAX_REPORT_BYTES} bytes; override via FL_OPT_REPORT_MAX_BYTES "
            f"env var; set to 0 to disable the cap entirely)."
        ),
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if args.cwd:
        root_build_dir = args.cwd / ".build"
    else:
        root_build_dir = Path(".build")

    # Support nested PlatformIO structure: .build/pio/<board>
    nested_pio_dir = root_build_dir / "pio"
    if nested_pio_dir.is_dir():
        root_build_dir = nested_pio_dir

    board_dirs = [d for d in root_build_dir.iterdir() if d.is_dir()]
    if not board_dirs:
        print(f"No board directories found in {root_build_dir.absolute()}")
        return 1
    print("Available boards:")
    for i, board_dir in enumerate(board_dirs):
        print(f"[{i}]: {board_dir.name}")
    which = (
        0
        if args.first
        else int(input("Enter the number of the board you want to inspect: "))
    )
    board_dir = board_dirs[which]
    optimization_report = board_dir / "optimization_report.txt"
    if not optimization_report.exists():
        print(
            f"No optimization report found for board '{board_dir.name}'.\n"
            f"Expected: {optimization_report}\n"
            "The build may not have completed successfully."
        )
        return 0

    total = optimization_report.stat().st_size
    max_bytes = max(args.max_bytes, 0)

    if total <= max_bytes or max_bytes == 0:
        text = optimization_report.read_text(encoding="utf-8", errors="replace")
        print(text)
        return 0

    # Tail the file: seek to the cut-off offset, read to end, then snap to the
    # next line boundary so we don't print a truncated leading line.
    with optimization_report.open("rb") as fh:
        fh.seek(total - max_bytes)
        tail_bytes = fh.read()
    first_newline = tail_bytes.find(b"\n")
    if 0 <= first_newline < len(tail_bytes) - 1:
        tail_bytes = tail_bytes[first_newline + 1 :]
    tail_text = tail_bytes.decode("utf-8", errors="replace")

    omitted = total - len(tail_bytes)
    print(
        f"[optimization_report.py] {optimization_report} is {total} bytes; "
        f"showing last {len(tail_bytes)} bytes ({omitted} bytes elided to keep "
        f"the GitHub Actions log under the runner's buffer limit)."
    )
    print(tail_text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
