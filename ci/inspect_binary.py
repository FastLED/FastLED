import json
import os
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT_ROOT = HERE.parent


def cpp_filt(cpp_filt_path: str, stdout: str) -> str:
    p = Path(cpp_filt_path)
    if not p.exists():
        raise FileNotFoundError(f"cppfilt not found at '{p}'")
    command = f"{p} -t"
    result = subprocess.run(
        command,
        shell=True,
        input=stdout,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(f"Error running command: {result.stderr}")
    return result.stdout


def main() -> int:
    # change the directory to the PROJECT_ROOT
    os.chdir(str(PROJECT_ROOT))

    board = input("Enter the board name: ")
    if board == "":
        return 1

    root_build_dir = Path(".build") / board
    build_info_json = root_build_dir / "build_info.json"
    build_info = json.loads(build_info_json.read_text())
    board_info = build_info[board]
    prog_path = Path(board_info["prog_path"])
    # base_path = prog_path.parent
    aliases = board_info["aliases"]
    msg = f"Aliases for {board}:\n"
    for tool, alias in aliases.items():
        msg += f"    {tool}: {alias}\n"
    print(msg)

    tool = input("Enter the tool name: ")
    if tool == "":
        return 1
    alias = Path(aliases.get(tool))
    if alias is None:
        print(f"Alias for {tool} not found.")
        return 1

    print(f"Alias for {tool}: {alias}")
    # now run the command on the elf
    # command = f"{alias} --version"
    command = f"{alias.as_posix()} {prog_path.as_posix()}"
    print(f"Running command: {command}")
    result = subprocess.run(
        command,
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        print(f"Error running command: {result.stderr}")
        return 1

    cpp_filt_path = aliases.get("c++filt")
    stdout = cpp_filt(cpp_filt_path, result.stdout)
    # print(result.stdout)
    print(stdout)
    return 0


if __name__ == "__main__":
    main()
