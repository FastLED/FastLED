import argparse
import os
import subprocess
import sys
from pathlib import Path
from typing import List

from ci.paths import PROJECT_ROOT

HERE: Path = Path(__file__).parent
WASM_DIR: Path = HERE / "wasm"
DOCKER_FILE: Path = WASM_DIR / "Dockerfile"


class WASMCompileError(Exception):
    """Custom exception for WASM compilation errors."""

    pass


def check_prerequisites() -> None:
    if not WASM_DIR.exists():
        raise WASMCompileError(f"ERROR: WASM directory not found at {WASM_DIR}")
    if not DOCKER_FILE.exists():
        raise WASMCompileError(f"ERROR: Dockerfile not found at {DOCKER_FILE}")


def filter_containers() -> str:
    return subprocess.run(
        ["docker", "ps", "-a", "-q", "--filter", "ancestor=fastled-wasm-compiler"],
        capture_output=True,
        text=True,
    ).stdout.strip()


def container_exists() -> bool:
    return bool(filter_containers())


def image_exists() -> bool:
    return (
        subprocess.run(
            ["docker", "image", "inspect", "fastled-wasm-compiler"],
            capture_output=True,
        ).returncode
        == 0
    )


def clean() -> None:
    if container_exists():
        print("Stopping and removing containers...")
        container_ids = filter_containers()
        if container_ids:
            subprocess.run(["docker", "stop"] + container_ids.split(), check=True)
            subprocess.run(["docker", "rm"] + container_ids.split(), check=True)
        else:
            print("No containers found for fastled-wasm-compiler.")
    else:
        print("No containers found for fastled-wasm-compiler.")

    if image_exists():
        print("Removing fastled-wasm-compiler image...")
        subprocess.run(["docker", "rmi", "fastled-wasm-compiler"], check=True)
    else:
        print("No image found for fastled-wasm-compiler.")


def build_image() -> None:
    print("Building Docker image...")
    try:
        cmd_list: List[str] = [
            "docker",
            "build",
            "--platform",
            "linux/amd64",
            "-t",
            "fastled-wasm-compiler",
            "-f",
            str(DOCKER_FILE),
            str(WASM_DIR),
        ]
        cmd_str: str = subprocess.list2cmdline(cmd_list)
        print(f"Running command: {cmd_str}")
        subprocess.run(
            cmd_list,
            check=True,
            text=True,
        )
    except subprocess.CalledProcessError as e:
        raise WASMCompileError(f"ERROR: Failed to build Docker image.\nError: {e}")


def is_tty() -> bool:
    return sys.stdout.isatty()


def run_container(directory: str, interactive: bool) -> None:
    absolute_directory: str = os.path.abspath(directory)
    if not os.path.isdir(absolute_directory):
        raise WASMCompileError(
            f"ERROR: Directory '{absolute_directory}' does not exist."
        )

    try:
        docker_command: List[str] = [
            "docker",
            "run",
            "--platform",
            "linux/amd64",
            "--rm",
            "-v",
            f"{absolute_directory}:/mapped",
            "fastled-wasm-compiler",
        ]
        if is_tty():
            docker_command.insert(4, "-it")
        if interactive:
            docker_command.append("/bin/bash")
        cmd_str: str = subprocess.list2cmdline(docker_command)
        print(f"Running command: {cmd_str}")
        subprocess.run(docker_command, check=True)
    except subprocess.CalledProcessError as e:
        raise WASMCompileError(f"ERROR: Failed to run Docker container.\n{e}")


def _copy_index_html_if_necessary() -> None:
    src_file = PROJECT_ROOT / "src" / "platforms" / "stub" / "wasm" / "index.html"
    dest_file = WASM_DIR / "index.html"
    if not dest_file.exists():
        print(f"Copying {src_file} to {dest_file}")
        dest_file.write_text(src_file.read_text())
        return
    # check if the contents are the same
    if src_file.read_text() != dest_file.read_text():
        print(f"Copying {src_file} to {dest_file}")
        dest_file.write_text(src_file.read_text())


def main() -> None:
    parser: argparse.ArgumentParser = argparse.ArgumentParser(
        description="WASM Compiler for FastLED"
    )
    parser.add_argument(
        "directory", nargs="?", help="The directory to mount as a volume"
    )
    parser.add_argument(
        "-b",
        "--build",
        action="store_true",
        help="Build the Docker image before running",
    )
    parser.add_argument(
        "-c",
        "--clean",
        action="store_true",
        help="Clean up Docker containers and images",
    )
    parser.add_argument(
        "-i",
        "--interactive",
        action="store_true",
        help="Run the container in interactive mode with /bin/bash",
    )
    args: argparse.Namespace = parser.parse_args()

    try:
        check_prerequisites()

        if args.clean:
            clean()
            return

        if args.directory is None:
            parser.error("ERROR: directory is required unless --clean is specified")

        _copy_index_html_if_necessary()

        if args.build or not image_exists():
            build_image()

        run_container(args.directory, args.interactive)

    except WASMCompileError as e:
        print(f"\033[91m{str(e)}\033[0m")  # Print error in red
        sys.exit(1)
    except Exception as e:
        print(f"\033[91mUnexpected error: {str(e)}\033[0m")  # Print error in red
        sys.exit(1)


if __name__ == "__main__":
    main()
