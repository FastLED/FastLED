import argparse
import json
import subprocess
from dataclasses import dataclass
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT_ROOT = HERE.parent


def cpp_filt(cpp_filt_path: str | Path, stdout: str) -> str:
    p = Path(cpp_filt_path)
    if not p.exists():
        raise FileNotFoundError(f"cppfilt not found at '{p}'")
    command = [str(p), "-t"]
    print(f"Running command: {' '.join(command)}")
    result = subprocess.run(
        command,
        input=stdout,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(f"Error running command: {result.stderr}")
    return result.stdout


def dump_symbols(firmware_path: Path, objdump_path: Path, cpp_filt_path: Path) -> str:
    objdump_command = f"{objdump_path} -t {firmware_path}"
    objdump_result = subprocess.run(
        objdump_command,
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if objdump_result.returncode != 0:
        raise RuntimeError(f"Error running objdump command: {objdump_result.stderr}")

    cpp_filt_command = [str(cpp_filt_path), "--no-strip-underscore"]
    cpp_filt_result = subprocess.run(
        cpp_filt_command,
        input=objdump_result.stdout,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if cpp_filt_result.returncode != 0:
        raise RuntimeError(f"Error running c++filt command: {cpp_filt_result.stderr}")

    return cpp_filt_result.stdout


def dump_symbol_sizes(nm_path: Path, cpp_filt_path: Path, elf_file: Path) -> str:
    nm_command = [
        str(nm_path),
        "-S",
        "--size-sort",
        str(elf_file),
    ]
    print(f"Listing symbols and sizes in ELF file: {elf_file}")
    print("Running command: ", " ".join(nm_command))
    nm_result = subprocess.run(
        nm_command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if nm_result.returncode != 0:
        raise RuntimeError(f"Error running nm command: {nm_result.stderr}")

    cpp_filt_command = [str(cpp_filt_path), "--no-strip-underscore"]
    print("Running c++filt command: ", " ".join(cpp_filt_command))
    cpp_filt_result = subprocess.run(
        cpp_filt_command,
        input=nm_result.stdout,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if cpp_filt_result.returncode != 0:
        raise RuntimeError(f"Error running c++filt command: {cpp_filt_result.stderr}")

    # now reverse sort the lines
    lines = cpp_filt_result.stdout.splitlines()

    @dataclass
    class Entry:
        address: str
        size: int
        everything_else: str

    def parse_line(line: str) -> Entry:
        address, size, *rest = line.split()
        return Entry(address, int(size, 16), " ".join(rest))

    data: list[Entry] = [parse_line(line) for line in lines]
    data.sort(key=lambda x: x.size, reverse=True)
    lines = [f"{d.size:6d} {d.everything_else}" for d in data]
    return "\n".join(lines)


def dump_sections_size(firmware_path: Path, size_path: Path) -> str:
    command = f"{size_path} {firmware_path}"
    result = subprocess.run(
        command,
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(f"Error running command: {result.stderr}")
    return result.stdout


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Inspect a compiled binary")
    parser.add_argument("--first", action="store_true", help="Inspect the first board")
    parser.add_argument("--cwd", type=str, help="Custom working directory")
    parser.add_argument("--elf", type=str, help="Path to the ELF file to inspect")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.elf:
        firmware_path = Path(args.elf)
        if not firmware_path.exists():
            print(f"ELF file not found: {firmware_path}")
            return 1

    if args.cwd:
        # os.chdir(args.cwd)
        root_build_dir = Path(args.cwd) / ".build"
    else:
        root_build_dir = Path(".build")

    # Find the first board directory
    board_dirs = [d for d in root_build_dir.iterdir() if d.is_dir()]
    if not board_dirs:
        # print("No board directories found in .build")
        print(f"No board directories found in {root_build_dir.absolute()}")
        return 1

    # display all the boards to the user and ask them to select which one they want by number
    print("Available boards:")
    for i, board_dir in enumerate(board_dirs):
        print(f"[{i}]: {board_dir.name}")

    if args.first:
        which = 0
    else:
        which = int(input("Enter the number of the board you want to inspect: "))

    board_dir = board_dirs[which]
    board = board_dir.name

    build_info_json = board_dir / "build_info.json"
    build_info = json.loads(build_info_json.read_text())
    board_info = build_info.get(board) or build_info[next(iter(build_info))]

    firmware_path = Path(board_info["prog_path"])
    objdump_path = Path(board_info["aliases"]["objdump"])
    cpp_filt_path = Path(board_info["aliases"]["c++filt"])

    print(f"Dumping sections size for {board} firmware: {firmware_path}")
    try:
        size_path = Path(board_info["aliases"]["size"])
        sections_size = dump_sections_size(firmware_path, size_path)
        print(sections_size)
    except Exception as e:
        print(f"Error while dumping sections size: {e}")

    print(f"Dumping symbols for {board} firmware: {firmware_path}")
    try:
        symbols = dump_symbols(firmware_path, objdump_path, cpp_filt_path)
        print(symbols)
    except Exception as e:
        print(f"Error while dumping symbols: {e}")

    print(f"Dumping symbol sizes for {board} firmware: {firmware_path}")
    try:
        nm_path = Path(board_info["aliases"]["nm"])
        symbol_sizes = dump_symbol_sizes(nm_path, cpp_filt_path, firmware_path)
        print(symbol_sizes)
    except Exception as e:
        print(f"Error while dumping symbol sizes: {e}")

    return 0


if __name__ == "__main__":
    main()
