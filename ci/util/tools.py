# pyright: reportUnknownMemberType=false
"""
Tools for working with build info and tool paths.
"""

import json
import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict

from ci.util.paths import BUILD


@dataclass
class Tools:
    as_path: Path
    ld_path: Path
    objcopy_path: Path
    objdump_path: Path
    cpp_filt_path: Path
    nm_path: Path


def load_tools(build_info_path: Path) -> Tools:
    build_info: Dict[str, Any] = json.loads(build_info_path.read_text())
    board_info: Dict[str, Any] = build_info[next(iter(build_info))]
    aliases: Dict[str, str] = board_info["aliases"]
    as_path = Path(aliases["as"])
    ld_path = Path(aliases["ld"])
    objcopy_path = Path(aliases["objcopy"])
    objdump_path = Path(aliases["objdump"])
    cpp_filt_path = Path(aliases["c++filt"])
    nm_path = Path(aliases["nm"])
    if sys.platform == "win32":
        as_path = as_path.with_suffix(".exe")
        ld_path = ld_path.with_suffix(".exe")
        objcopy_path = objcopy_path.with_suffix(".exe")
        objdump_path = objdump_path.with_suffix(".exe")
        cpp_filt_path = cpp_filt_path.with_suffix(".exe")
        nm_path = nm_path.with_suffix(".exe")
    out = Tools(as_path, ld_path, objcopy_path, objdump_path, cpp_filt_path, nm_path)
    tools = [as_path, ld_path, objcopy_path, objdump_path, cpp_filt_path, nm_path]
    for tool in tools:
        if not tool.exists():
            raise FileNotFoundError(f"Tool not found: {tool}")
    return out


def _list_builds() -> list[Path]:
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
    object_files: list[Path] = []

    # Walk through build directory to find .o files
    for root, _, files in os.walk(build_dir):
        for file in files:
            if file.endswith(".o") and "FrameworkArduino" not in file:
                full_path = Path(root) / file
                if "FrameworkArduino" not in full_path.parts:
                    object_files.append(full_path)

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

    parser.add_argument(
        "--symbols", action="store_true", help="Dump symbol table using nm"
    )
    parser.add_argument(
        "--disassemble", action="store_true", help="Dump disassembly using objdump"
    )

    args = parser.parse_args()
    build_path = args.build_path
    symbols = args.symbols
    disassemble = args.disassemble

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

    if not symbols and not disassemble:
        while True:
            print(
                "Error: Please specify at least one action to perform", file=sys.stderr
            )
            action = input(
                "Enter 's' to dump symbols, 'd' to disassemble, or 'q' to quit: "
            )
            if action == "s":
                symbols = True
                break
            elif action == "d":
                disassemble = True
                break
            elif action == "q":
                sys.exit(0)
            else:
                print("Error: Invalid action", file=sys.stderr)

    object_file = _prompt_object_file(build_path)
    if symbols:
        import subprocess

        cmd_str = subprocess.list2cmdline(
            [str(tools.objdump_path), str(object_file), "--syms"]
        )
        print(f"Running command: {cmd_str}")
        subprocess.run([str(tools.objdump_path), str(object_file)])

    if disassemble:
        import subprocess

        cmd_str = subprocess.list2cmdline(
            [str(tools.objdump_path), "-d", str(object_file)]
        )
        print(f"Running command: {cmd_str}")
        subprocess.run([str(tools.objdump_path), "-d", str(object_file)])

    if not (symbols or disassemble):
        parser.print_help()


if __name__ == "__main__":
    try:
        cli()
    except KeyboardInterrupt:
        print("Exiting...")
        sys.exit(1)
