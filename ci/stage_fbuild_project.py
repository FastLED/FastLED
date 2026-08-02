#!/usr/bin/env python3
"""Stage a FastLED example as an fbuild project without compiling it.

fbuild currently consumes a PlatformIO-compatible project manifest, but this
entrypoint never invokes PlatformIO. It only synthesizes that manifest and
copies the selected sketch/library sources so a native fbuild command such as
``fbuild test-emu`` can own the complete build and emulator lifecycle.
"""

import argparse
from pathlib import Path

from ci.boards import create_board
from ci.compiler.compiler import InitResult
from ci.compiler.path_manager import FastLEDPaths, resolve_project_root
from ci.compiler.pio import init_fbuild_project


def stage_fbuild_project(
    board_name: str,
    example: str,
    defines: list[str] | None = None,
    build_dir: Path | None = None,
    verbose: bool = False,
) -> InitResult:
    """Prepare one example for fbuild and return its staged project path."""
    board = create_board(board_name)
    target_dir = build_dir or (
        resolve_project_root() / ".build" / "fbuild" / board.board_name
    )
    paths = FastLEDPaths(board.board_name)
    return init_fbuild_project(
        board=board,
        verbose=verbose,
        example=example,
        paths=paths,
        build_dir=target_dir,
        additional_defines=defines,
    )


def _parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Stage a FastLED example for a native fbuild command"
    )
    parser.add_argument("--board", required=True, help="fbuild environment name")
    parser.add_argument("--example", required=True, help="FastLED example path")
    parser.add_argument(
        "--define",
        action="append",
        default=[],
        dest="defines",
        help="Additional preprocessor definition (repeatable)",
    )
    parser.add_argument("--build-dir", type=Path, help="Staged project directory")
    parser.add_argument("--verbose", action="store_true")
    return parser.parse_args(argv)


def main() -> int:
    args = _parse_args()
    result = stage_fbuild_project(
        board_name=args.board,
        example=args.example,
        defines=args.defines,
        build_dir=args.build_dir,
        verbose=args.verbose,
    )
    if not result.success:
        print(result.output)
        return 1
    print(f"Staged {args.example} for {args.board} at {result.build_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
