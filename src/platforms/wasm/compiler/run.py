"""
Uses the latest wasm compiler image to compile the FastLED sketch.

Probably an unfortunate name.
"""

import docker
import subprocess
import time
import sys
import os
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[4]
DEFAULT_WASM_PROJECT_DIR = PROJECT_ROOT / "examples" / "wasm"
# Relative to the current driectory
DEFAULT_WASM_PROJECT_DIR = DEFAULT_WASM_PROJECT_DIR.relative_to(Path.cwd())
DEFAULT_WASM_PROJECT_DIR = str(DEFAULT_WASM_PROJECT_DIR)

def is_docker_running():
    """Check if Docker is running by pinging the Docker daemon."""
    try:
        client = docker.from_env()
        client.ping()  # Ping the Docker daemon to verify connectivity
        print("Docker is running.")
        return True
    except docker.errors.DockerException as e:
        print(f"Docker is not running: {str(e)}")
        return False

def start_docker():
    """Attempt to start Docker Desktop (or the Docker daemon) automatically."""
    print("Attempting to start Docker...")
    try:
        if sys.platform == "win32":
            subprocess.run(["start", "Docker Desktop"], shell=True)
        elif sys.platform == "darwin":
            subprocess.run(["open", "/Applications/Docker.app"])
        elif sys.platform.startswith("linux"):
            subprocess.run(["sudo", "systemctl", "start", "docker"])
        else:
            print("Unknown platform. Cannot auto-launch Docker.")
            return False

        # Wait for Docker to start up
        print("Waiting for Docker to start...")
        for _ in range(10):
            if is_docker_running():
                print("Docker started successfully.")
                return True
            time.sleep(3)

        print("Failed to start Docker within the expected time.")
    except Exception as e:
        print(f"Error starting Docker: {str(e)}")
    return False

def remove_existing_container(container_name):
    try:
        subprocess.run(["docker", "rm", "-f", container_name], check=True, capture_output=True)
        print(f"Removed existing container: {container_name}")
    except subprocess.CalledProcessError:
        # If the container doesn't exist, it's not an error
        pass

def main():
    if not is_docker_running():
        if start_docker():
            print("Docker is now running.")
        else:
            print("Docker could not be started. Exiting.")
            sys.exit(1)

    # Get the directory to mount
    directory = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_WASM_PROJECT_DIR
    absolute_directory = os.path.abspath(directory)
    base_name = os.path.basename(absolute_directory)

    if not os.path.isdir(absolute_directory):
        print(f"ERROR: Directory '{absolute_directory}' does not exist.")
        sys.exit(1)

    # Remove existing container if it exists
    remove_existing_container("fastled-wasm-compiler")

    # Launch the Docker container if Docker is running
    try:
        docker_command = [
            "docker",
            "run",
            "--rm",  # Automatically remove the container when it exits
        ]
        
        if sys.stdout.isatty():
            docker_command.append("-it")
        
        docker_command.extend([
            "--name",
            "fastled-wasm-compiler",
            "--platform",
            "linux/amd64",
            "-v",
            f"{absolute_directory}:/mapped/{base_name}",
            "fastled-wasm-compiler:latest",
        ])

        print(f"Running command: {' '.join(docker_command)}")
        process = subprocess.Popen(docker_command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

        # Stream the output
        for line in process.stdout:
            print(line, end='')

        # Wait for the process to complete
        process.wait()

        print("\nContainer execution completed.")

    except subprocess.CalledProcessError as e:
        print(f"Failed to run Docker container: {e}")
    except KeyboardInterrupt:
        print("\nOperation cancelled by user.")

if __name__ == "__main__":
    main()
