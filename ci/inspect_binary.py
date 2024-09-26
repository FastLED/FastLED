import argparse
import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT_ROOT = HERE.parent


def format_size(size: int | float) -> str:
    for unit in ["B", "KB", "MB", "GB"]:
        if size < 1024.0:
            return f"{size:.2f} {unit}"
        size /= 1024.0
    return f"{size:.2f} TB"


def parse_map_file(
    map_content: str,
) -> dict[str, dict[str, int | list[tuple[str, int]]]]:
    sections: dict[str, dict[str, int | list[tuple[str, int]]]] = {}
    current_section = None
    for line in map_content.split("\n"):
        if line.startswith("."):
            current_section = line.split()[0]
            sections[current_section] = {"size": 0, "symbols": []}
        elif line.strip() and current_section:
            parts = line.split()
            if len(parts) >= 3:
                try:
                    size = int(parts[1], 16)
                    symbol = parts[2]
                    # Check if the symbol is not from a .a file
                    if not any(part.endswith(".a") for part in parts):
                        section_data = sections[current_section]
                        if isinstance(section_data["size"], int):
                            section_data["size"] += size
                        symbols = section_data["symbols"]
                        if isinstance(symbols, list):
                            symbols.append((symbol, size))
                except ValueError:
                    # Skip lines that don't conform to the expected format
                    continue
    return sections


def cpp_filt(cpp_filt_path: str | Path, input_text: str) -> str:
    p = Path(cpp_filt_path)
    if not p.exists():
        raise FileNotFoundError(f"cppfilt not found at '{p}'")
    command = [str(p), "-t"]
    print(f"Running command: {' '.join(command)}")
    result = subprocess.run(
        command,
        input=input_text,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(f"Error running command: {result.stderr}")
    return result.stdout


def run_objdump(cpp_filt_path: Path, objdump_path: Path, elf_path: Path) -> dict:
    command = [str(objdump_path), "-t", str(elf_path)]
    print(f"Running command: {' '.join(command)}")
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"Error running objdump: {result.stderr}")

    stdout = result.stdout
    stdout = cpp_filt(cpp_filt_path, stdout)

    symbols = {}
    for line in stdout.splitlines():
        parts = line.split()
        if len(parts) >= 6:
            size = int(parts[4], 16)
            name = parts[5]
            section = parts[3]
            if size > 0:
                symbols[name] = {"size": size, "section": section}
    return symbols


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Inspect a compiled binary")
    parser.add_argument("--first", action="store_true", help="Inspect the first board")
    parser.add_argument("--cwd", type=str, help="Custom working directory")
    parser.add_argument(
        "--top",
        type=int,
        default=10,
        help="Number of top symbols to display per section",
    )
    return parser.parse_args()


def _find_binary_path(elf_path: Path) -> Path:
    suffixes = [".bin", ".hex", ".uf2"]
    for suffix in suffixes:
        binary_path = elf_path.with_suffix(suffix)
        if binary_path.exists():
            return binary_path
    raise FileNotFoundError(
        f"None of the following files found: {', '.join(str(elf_path.with_suffix(suffix)) for suffix in suffixes)}"
    )


def main() -> int:
    args = parse_args()
    if args.cwd:
        root_build_dir = Path(args.cwd) / ".build"
    else:
        root_build_dir = Path(".build")

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
    board = board_dir.name

    build_info_json = board_dir / "build_info.json"
    build_info = json.loads(build_info_json.read_text())
    board_info = build_info.get(board) or build_info[next(iter(build_info))]
    elf_path = Path(board_info["prog_path"])
    # binary_path = _find_binary_path(elf_path)
    cpp_filt_path = Path(board_info["aliases"]["c++filt"])
    objdump_path = Path(board_info["aliases"]["objdump"])
    map_path = board_dir / "firmware.map"
    if not map_path.exists():
        map_path = elf_path.with_suffix(".map")

    if map_path.exists() and elf_path.exists():
        print(f"Analyzing map file and ELF for {board} compiled firmware binary")
        # input_text = map_path.read_text()
        # demangled_text = cpp_filt(cpp_filt_path, input_text)
        # map_sections = parse_map_file(demangled_text)

        objdump_symbols = run_objdump(cpp_filt_path, objdump_path, elf_path)

        print("\nMemory Usage Summary (from objdump):")
        total_size = sum(symbol["size"] for symbol in objdump_symbols.values())
        sections = {}
        for symbol, data in objdump_symbols.items():
            section = data["section"]
            if section not in sections:
                sections[section] = {"size": 0, "symbols": []}
            sections[section]["size"] += data["size"]
            sections[section]["symbols"].append((symbol, data["size"]))  # type: ignore

        sorted_sections = sorted(
            sections.items(),
            key=lambda x: x[1]["size"] if isinstance(x[1]["size"], (int, float)) else 0,
            reverse=True,
        )
        print(f"{'Section':<20} {'Size':<15} {'Percentage':<10}")
        print("-" * 45)
        for section, data in sorted_sections:
            size = data["size"]
            percentage = size / total_size * 100
            print(f"{section:<20} {format_size(size):<15} {percentage:.2f}%")
        print("-" * 45)
        print(f"{'Total':<20} {format_size(total_size):<15} 100.00%")

        print(f"\nTop {args.top} largest symbols per section (from objdump):")
        for section, data in sorted_sections:
            symbols = data.get("symbols", [])
            if isinstance(symbols, list) and symbols:
                print(f"\n{section}:")
                sorted_symbols = sorted(symbols, key=lambda x: x[1], reverse=True)
                for symbol, size in sorted_symbols[: args.top]:
                    print(f"  {symbol:<60} {format_size(size):>10}")
            else:
                print(f"\n{section}: No symbols")

        print("\nDetailed symbol sizes (for easy diffing):")
        for symbol, data in sorted(
            objdump_symbols.items(), key=lambda x: x[1]["size"], reverse=True
        ):
            print(f"{data['size']},{data['section']},{symbol}")
    else:
        print(f"Map file not found at {map_path} or ELF file not found at {elf_path}")
        return 1

    print("Map file dump:")
    print(map_path.read_text())

    return 0


if __name__ == "__main__":
    sys.exit(main())
