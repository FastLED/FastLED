"""
Uses the latest wasm compiler image to compile the FastLED sketch.

Probably an unfortunate name.

Push instructions:
  1. docker login
  2. (build the image, use the wasm_compiler in ci/)
    a. This will create an image tagged by fastled-wasm-compiler
  3. docker tag fastled-wasm-compiler:latest niteris/fastled-wasm:latest
  4. docker push niteris/fastled-wasm:latest
"""
import sys
import subprocess
import time

import os
from pathlib import Path

try:
    import docker
except ImportError:
    print("Please install the 'docker' package by running 'pip install docker'.")
    sys.exit(1)


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

def ensure_image_exists():
    """Check if local image exists, pull from remote if not."""
    try:
        # Check if local image exists
        result = subprocess.run(["docker", "image", "inspect", "fastled-wasm-compiler:latest"], 
                              capture_output=True, check=False)
        if result.returncode != 0:
            print("Local image not found. Pulling from niteris/fastled-wasm...")
            subprocess.run(["docker", "pull", "niteris/fastled-wasm:latest"], check=True)
            subprocess.run(["docker", "tag", "niteris/fastled-wasm:latest", "fastled-wasm-compiler:latest"], check=True)
            print("Successfully pulled and tagged remote image.")
        return True
    except subprocess.CalledProcessError as e:
        print(f"Failed to ensure image exists: {e}")
        return False

def container_exists(container_name):
    """Check if a container with the given name exists."""
    try:
        result = subprocess.run(
            ["docker", "container", "inspect", container_name],
            capture_output=True,
            check=False
        )
        return result.returncode == 0
    except subprocess.CalledProcessError:
        return False

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

    container_name = "fastled-wasm-compiler"

    # Ensure the image exists (pull if needed)
    if not ensure_image_exists():
        print("Failed to ensure Docker image exists. Exiting.")
        sys.exit(1)

    # Check if container exists
    container_exists_flag = container_exists(container_name)

    # Launch or start the Docker container if Docker is running
    try:
        if container_exists_flag:
            # Start existing container
            docker_command = [
                "docker",
                "start",
                "-a",  # Attach to container's output
                container_name
            ]
        else:
            # Create new container
            docker_command = [
                "docker",
                "run",
            ]
        
        if not container_exists_flag:
            # Only add these flags for 'docker run'
            if sys.stdout.isatty():
                docker_command.append("-it")
            docker_command.extend([
                "--name",
                container_name,
                "--platform",
                "linux/amd64",
                "-v",
                f"{absolute_directory}:/mapped/{base_name}",
                "fastled-wasm-compiler:latest"
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
