#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
import os
import subprocess
import sys
from pathlib import Path
from typing import List

from ci.util.paths import BUILD
from ci.util.tools import load_tools


def _list_builds() -> List[Path]:
    str_paths = os.listdir(BUILD)
    paths = [BUILD / p for p in str_paths]
    dirs = [p for p in paths if p.is_dir()]
    return dirs


def _check_build(build: Path) -> bool:
    # 1. should contain a build_info.json file
    # 2. should contain a .pio/build directory
    has_build_info = (build / "build_info.json").exists()
    has_pio_build = (build / ".pio" / "build").exists()
    return has_build_info and has_pio_build


def _prompt_build() -> Path:
    builds = _list_builds()
    if not builds:
        print("Error: No builds found", file=sys.stderr)
        sys.exit(1)
    print("Select a build:")
    for i, build in enumerate(builds):
        print(f"  [{i}]: {build}")
    while True:
        try:
            which = int(input("Enter the number of the build to use: "))
            if 0 <= which < len(builds):
                valid = _check_build(BUILD / builds[which])
                if valid:
                    return BUILD / builds[which]
                print("Error: Invalid build", file=sys.stderr)
            else:
                print("Error: Invalid selection", file=sys.stderr)
                continue
        except ValueError:
            print("Error: Invalid input", file=sys.stderr)
            continue


def _prompt_object_file(build: Path) -> Path:
    # Look for object files in .pio/build directory
    build_dir = build / ".pio" / "build"
    object_files: List[Path] = list(build_dir.rglob("*.o"))

    if not object_files:
        print("Error: No object files found", file=sys.stderr)
        sys.exit(1)

    print("\nSelect an object file:")
    for i, obj_file in enumerate(object_files):
        print(f"  [{i}]: {obj_file.relative_to(build_dir)}")

    while True:
        try:
            which = int(input("Enter the number of the object file to use: "))
            if 0 <= which < len(object_files):
                return object_files[which]
            print("Error: Invalid selection", file=sys.stderr)
        except ValueError:
            print("Error: Invalid input", file=sys.stderr)
            continue


def cli() -> None:
    import argparse

    parser = argparse.ArgumentParser(
        description="Dump object file information using build tools"
    )

    parser.add_argument(
        "build_path",
        type=Path,
        nargs="?",
        help="Path to build directory containing build info JSON file",
    )

    args = parser.parse_args()
    build_path = args.build_path

    # Check if object file was provided and exists
    if build_path is None:
        build_path = _prompt_build()
    else:
        if not _check_build(build_path):
            print("Error: Invalid build directory", file=sys.stderr)
            sys.exit(1)

    assert build_path is not None
    assert build_path

    build_info_path = build_path / "build_info.json"
    assert build_info_path.exists(), f"File not found: {build_info_path}"

    tools = load_tools(build_info_path)

    object_file = _prompt_object_file(build_path)

    cmd = [str(tools.objdump_path), "--syms", str(object_file)]
    if sys.platform == "win32":
        cmd = ["cmd", "/c"] + cmd
    cmd_str = subprocess.list2cmdline(cmd)
    subprocess.run(cmd, check=True)
    print("\nDone. Command used:", cmd_str)


if __name__ == "__main__":
    try:
        cli()
    except KeyboardInterrupt:
        print("Exiting...")
        sys.exit(1)
