import json
import subprocess
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


def dump_symbols(firmware_path: Path, objdump_path: Path) -> str:
    command = f"{objdump_path} -t {firmware_path}"
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


def main() -> int:
    root_build_dir = Path(".build")

    # Find the first board directory
    board_dirs = [d for d in root_build_dir.iterdir() if d.is_dir()]
    if not board_dirs:
        print("No board directories found in .build")
        return 1

    # display all the boards to the user and ask them to select which one they want by number
    print("Available boards:")
    for i, board_dir in enumerate(board_dirs):
        print(f"[{i}]: {board_dir.name}")
    which = int(input("Enter the number of the board you want to inspect: "))

    board_dir = board_dirs[which]
    board = board_dir.name

    build_info_json = board_dir / "build_info.json"
    build_info = json.loads(build_info_json.read_text())
    board_info = build_info[board]

    firmware_path = Path(board_info["prog_path"])
    objdump_path = Path(board_info["aliases"]["objdump"])
    cpp_filt_path = Path(board_info["aliases"]["c++filt"])

    print(f"Dumping symbols for {board} firmware: {firmware_path}")

    try:
        symbols = dump_symbols(firmware_path, objdump_path)
        symbols = cpp_filt(cpp_filt_path, symbols)
        print(symbols)
    except Exception as e:
        print(f"Error: {e}")
        return 1

    return 0


if __name__ == "__main__":
    main()
