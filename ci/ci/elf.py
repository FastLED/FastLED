import subprocess
from pathlib import Path


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
    list_symbols_and_sizes(objdump_path, elf_file)


def list_symbols_and_sizes(objdump_path: Path, elf_file: Path):
    """
    List all symbols and their sizes from the ELF file using objdump.

    Args:
        objdump_path (Path): Path to the objdump executable.
        elf_file (Path): Path to the ELF file.
    """
    command = [
        str(objdump_path),
        "-t",
        str(elf_file),
    ]  # "-t" option lists symbols with sizes.
    print(f"Listing symbols and sizes in ELF file: {elf_file}")
    output = run_command(command, show_output=False)

    symbols = []
    for line in output.splitlines():
        parts = line.split()
        # Expected parts length can vary, check if size and section index (parts[2] & parts[4]) are valid
        if len(parts) > 5 and parts[2].isdigit() and parts[4].startswith("."):
            symbol = {
                "name": parts[-1],
                "size": int(parts[2], 16),  # size is in hex format
                "section": parts[4],
                "type": parts[3],
            }
            symbols.append(symbol)

    if symbols:
        print("\nSymbols and Sizes in ELF File:")
        for symbol in symbols:
            print(
                f"Symbol: {symbol['name']}, Size: {symbol['size']} bytes, Type: {symbol['type']}, Section: {symbol['section']}"
            )
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
