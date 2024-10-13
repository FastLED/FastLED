import argparse
import os
import subprocess
import sys
import webbrowser
from pathlib import Path
from typing import List

from ci.paths import PROJECT_ROOT

HERE: Path = Path(__file__).parent
DOCKER_FILE: Path = PROJECT_ROOT / "src" / "platforms" / "stub" / "wasm" / "Dockerfile"

assert DOCKER_FILE.exists(), f"ERROR: Dockerfile not found at {DOCKER_FILE}"


class WASMCompileError(Exception):
    """Custom exception for WASM compilation errors."""

    pass


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
            str(PROJECT_ROOT),
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
            # "--rm",
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
    parser.add_argument(
        "-o",
        "--open",
        action="store_true",
        help="Launch a web server and open a browser after compilation",
    )
    args: argparse.Namespace = parser.parse_args()

    try:
        if args.clean:
            clean()
            return

        if args.directory is None:
            parser.error("ERROR: directory is required unless --clean is specified")

        if args.build or not image_exists():
            build_image()

        run_container(args.directory, args.interactive)

        output_dir = str(Path(args.directory) / "fastled_js")

        if args.open:
            print(f"Launching web server from {output_dir}")
            run_web_server(output_dir)

    except WASMCompileError as e:
        print(f"\033[91m{str(e)}\033[0m")  # Print error in red
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nOperation cancelled by user.")
        sys.exit(0)
    except Exception as e:
        print(f"\033[91mUnexpected error: {str(e)}\033[0m")  # Print error in red
        sys.exit(1)


def run_web_server(directory: str, port: int = 8000) -> None:
    os.chdir(directory)
    print(f"Launching web server at http://localhost:{port}")
    proc = subprocess.Popen(["python", "-m", "http.server", str(port)])
    import time

    time.sleep(3)
    webbrowser.open(f"http://localhost:{port}")
    try:
        proc.wait()
    except KeyboardInterrupt:
        proc.terminate()


if __name__ == "__main__":
    main()
