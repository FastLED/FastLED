import argparse
import json
import sys
from pathlib import Path
import subprocess

from ci.bin_2_elf import bin_to_elf

def run_command(command: list, show_output=False):
    """
    Run a command using subprocess and capture the output.

    Args:
        command (list): Command to run.
        show_output (bool): Print command and its output if True.

    Returns:
        str: Standard output of the command.

    Raises:
        RuntimeError: If the command fails.
    """
    if show_output:
        print(f"Running command: {' '.join(command)}")
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"Command failed: {' '.join(command)}\n{result.stderr}")
    if show_output and result.stdout:
        print(f"Command output: {result.stdout}")
    return result.stdout


def cpp_filt(cpp_filt_path: Path, input_text: str) -> str:
    """
    Demangle C++ symbols using c++filt.

    Args:
        cpp_filt_path (Path): Path to c++filt executable.
        input_text (str): Text to demangle.

    Returns:
        str: Demangled text.
    """
    if not cpp_filt_path.exists():
        raise FileNotFoundError(f"cppfilt not found at '{cpp_filt_path}'")
    command = [str(cpp_filt_path), "-t", "-n"]
    print(f"Running c++filt on input text with {cpp_filt_path}")
    process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    stdout, stderr = process.communicate(input=input_text)
    if process.returncode != 0:
        raise RuntimeError(f"Error running c++filt: {stderr}")
    return stdout


def analyze_elf_file(objdump_path: Path, elf_file: Path):
    """
    Analyze the ELF file using objdump to display its contents.

    Args:
        objdump_path (Path): Path to the objdump executable.
        elf_file (Path): Path to the ELF file.
    """
    command = [str(objdump_path), "-h", str(elf_file)]  # "-h" option shows headers.
    print(f"Analyzing ELF file: {elf_file}")
    output = run_command(command, show_output=True)
    print("\nELF File Analysis:")
    print(output)


def list_symbols_and_sizes(objdump_path: Path, elf_file: Path):
    """
    List all symbols and their sizes from the ELF file using objdump.

    Args:
        objdump_path (Path): Path to the objdump executable.
        elf_file (Path): Path to the ELF file.
    """
    command = [str(objdump_path), "-t", str(elf_file)]  # "-t" option lists symbols with sizes.
    print(f"Listing symbols and sizes in ELF file: {elf_file}")
    output = run_command(command, show_output=False)
    
    symbols = []
    for line in output.splitlines():
        parts = line.split()
        # Expected parts length can vary, check if size and section index (parts[2] & parts[4]) are valid
        if len(parts) > 5 and parts[2].isdigit() and parts[4].startswith('.'):
            symbol = {
                "name": parts[-1],
                "size": int(parts[2], 16),  # size is in hex format
                "section": parts[4],
                "type": parts[3]
            }
            symbols.append(symbol)

    if symbols:
        print("\nSymbols and Sizes in ELF File:")
        for symbol in symbols:
            print(f"Symbol: {symbol['name']}, Size: {symbol['size']} bytes, Type: {symbol['type']}, Section: {symbol['section']}")
    else:
        print("No symbols found or unable to parse symbols correctly.")


def check_elf_format(objdump_path: Path, elf_file: Path):
    """
    Check the format of the ELF file using objdump to confirm it's being read correctly.

    Args:
        objdump_path (Path): Path to the objdump executable.
        elf_file (Path): Path to the ELF file.
    """
    command = [str(objdump_path), "-f", str(elf_file)]
    print(f"Checking ELF file format: {elf_file}")
    output = run_command(command, show_output=True)
    print("\nELF File Format Information:")
    print(output)


def check_section_contents(objdump_path: Path, elf_file: Path):
    """
    Dump the contents of all sections in the ELF file using objdump.

    Args:
        objdump_path (Path): Path to the objdump executable.
        elf_file (Path): Path to the ELF file.
    """
    command = [str(objdump_path), "-s", str(elf_file)]
    print(f"Dumping all sections of ELF file: {elf_file}")
    output = run_command(command, show_output=True)
    print("\nELF File Sections Content:")
    print(output)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Convert a binary file to ELF using map file.")
    parser.add_argument("--first", action="store_true", help="Inspect the first board")
    parser.add_argument("--cwd", type=Path, help="Custom working directory")
    parser.add_argument("--output-elf", type=Path, help="Output ELF file path.", default="output.elf")
    return parser.parse_args()


def load_build_info(build_info_path: Path) -> dict:
    """
    Load build information from a JSON file.

    Args:
        build_info_path (Path): Path to the build_info.json file.

    Returns:
        dict: Parsed JSON data.
    """
    if not build_info_path.exists():
        raise FileNotFoundError(f"Build info JSON not found at '{build_info_path}'")
    return json.loads(build_info_path.read_text())


def main() -> int:
    args = parse_args()
    if args.cwd:
        root_build_dir = args.cwd / ".build"
    else:
        root_build_dir = Path(".build")

    board_dirs = [d for d in root_build_dir.iterdir() if d.is_dir()]
    if not board_dirs:
        print(f"No board directories found in {root_build_dir.absolute()}")
        return 1

    print("Available boards:")
    for i, board_dir in enumerate(board_dirs):
        print(f"[{i}]: {board_dir.name}")

    which = 0 if args.first else int(input("Enter the number of the board you want to inspect: "))
    board_dir = board_dirs[which]
    build_info_json = board_dir / "build_info.json"

    build_info = load_build_info(build_info_json)
    board = board_dir.name
    board_info = build_info.get(board) or build_info[next(iter(build_info))]

    # Validate paths from build_info.json
    elf_path = Path(board_info.get("prog_path", ""))
    if not elf_path.exists():
        print(f"Error: ELF path '{elf_path}' does not exist. Check the 'prog_path' in build_info.json.")
        return 1

    bin_file = elf_path.with_suffix(".bin")
    if not bin_file.exists():
        # use .hex or .uf2 if .bin doesn't exist
        bin_file = elf_path.with_suffix(".hex")
        if not bin_file.exists():
            bin_file = elf_path.with_suffix(".uf2")
            if not bin_file.exists():
                print(f"Error: Binary file not found for '{elf_path}'")
                return 1
    cpp_filt_path = Path(board_info["aliases"]["c++filt"])
    ld_path = Path(board_info["aliases"]["ld"])
    as_path = Path(board_info["aliases"]["as"])
    objcopy_path = Path(board_info["aliases"]["objcopy"])
    objdump_path = Path(board_info["aliases"]["objdump"])
    map_file = board_dir / "firmware.map"
    if not map_file.exists():
        map_file = bin_file.with_suffix(".map")

    # Convert binary to ELF
    output_elf = bin_to_elf(bin_file, map_file, as_path, ld_path, objcopy_path, args.output_elf)

    # Analyze the generated ELF file using objdump
    analyze_elf_file(objdump_path, output_elf)

    # List symbols and sizes from the ELF file
    list_symbols_and_sizes(objdump_path, output_elf)

    # Check ELF file format (to ensure it is being read correctly)
    # check_elf_format(objdump_path, output_elf)

    # Dump the contents of all sections in the ELF file
    # check_section_contents(objdump_path, output_elf)

    # Demangle and print map file using c++filt
    print("\n##################################################")
    print("# Map file dump:")
    print("##################################################\n")
    map_text = map_file.read_text()
    dmangled_text = cpp_filt(cpp_filt_path, map_text)
    print(dmangled_text)

    return 0


if __name__ == "__main__":
    sys.exit(main())
