import subprocess
import sys
from typing import Any, Dict, List, Optional


class DockerManager:
    def __init__(self):
        # No direct client object, will use subprocess for docker CLI commands
        pass

    def _run_docker_command(
        self, command: List[str], check: bool = True
    ) -> subprocess.CompletedProcess[str]:
        full_command = ["docker"] + command
        print(f"Executing Docker command: {' '.join(full_command)}")
        result = subprocess.run(
            full_command, capture_output=True, text=True, check=check
        )
        if check and result.returncode != 0:
            print(
                f"Docker command failed with exit code {result.returncode}",
                file=sys.stderr,
            )
            print(f"Stdout: {result.stdout}", file=sys.stderr)
            print(f"Stderr: {result.stderr}", file=sys.stderr)
        return result

    def pull_image(self, image_name: str, tag: str = "latest"):
        """Pulls the specified Docker image."""
        print(f"Pulling image: {image_name}:{tag}")
        self._run_docker_command(["pull", f"{image_name}:{tag}"])
        print(f"Image {image_name}:{tag} pulled successfully.")

    def run_container(
        self,
        image_name: str,
        command: List[str],
        volumes: Optional[Dict[str, Dict[str, str]]] = None,
        environment: Optional[Dict[str, str]] = None,
        detach: bool = False,
        name: Optional[str] = None,
    ) -> str:  # Return container ID as string
        """
        Runs a Docker container with specified command, volume mounts, and environment variables.
        Returns the container ID.
        """
        print(
            f"Running container from image: {image_name} with command: {' '.join(command)}"
        )
        docker_cmd = ["run", "--rm"]  # --rm to automatically remove container on exit
        if detach:
            docker_cmd.append("-d")
        if name:
            docker_cmd.extend(["--name", name])
        if volumes:
            for host_path, container_path_info in volumes.items():
                mode = container_path_info.get("mode", "rw")
                docker_cmd.extend(
                    ["-v", f"{host_path}:{container_path_info['bind']}:{mode}"]
                )
        if environment:
            for key, value in environment.items():
                docker_cmd.extend(["-e", f"{key}={value}"])

        docker_cmd.append(image_name)
        docker_cmd.extend(command)

        result = self._run_docker_command(docker_cmd)
        container_id = result.stdout.strip()
        print(f"Container {container_id} started.")
        return container_id

    def get_container_logs(self, container_id_or_name: str) -> str:
        """Retrieves logs from a container."""
        print(f"Getting logs for container: {container_id_or_name}")
        result = self._run_docker_command(["logs", container_id_or_name])
        logs = result.stdout
        print(f"Logs retrieved for {container_id_or_name}.")
        return logs

    def stop_container(self, container_id_or_name: str):
        """Stops a running container."""
        print(f"Stopping container: {container_id_or_name}")
        self._run_docker_command(["stop", container_id_or_name])
        print(f"Container {container_id_or_name} stopped.")

    def remove_container(self, container_id_or_name: str):
        """Removes a container."""
        print(f"Removing container: {container_id_or_name}")
        self._run_docker_command(["rm", container_id_or_name])
        print(f"Container {container_id_or_name} removed.")

    def execute_command_in_container(
        self, container_id_or_name: str, command: List[str]
    ) -> tuple[int, str, str]:
        """Executes a command inside a running container and returns exit code, stdout, and stderr."""
        print(
            f"Executing command '{' '.join(command)}' in container: {container_id_or_name}"
        )
        result = self._run_docker_command(
            ["exec", container_id_or_name] + command, check=False
        )
        stdout = result.stdout
        stderr = result.stderr
        print(
            f"Command executed in {container_id_or_name}. Exit code: {result.returncode}"
        )
        return result.returncode, stdout, stderr


if __name__ == "__main__":
    # Example Usage:
    manager = DockerManager()
    image_to_use = "mluis/qemu-esp32"
    container_name = "test_esp32_qemu"

    try:
        print(f"--- Starting DockerManager test with image: {image_to_use} ---")
        manager.pull_image(image_to_use)

        # Example: Run a simple command in the container
        print("\n--- Running a simple command ---")
        container_id = manager.run_container(
            image_name=image_to_use,
            command=["echo", "Hello from Docker!"],
            detach=True,
            name=container_name,
        )

        # Wait a bit for the command to execute and logs to be generated
        import time

        time.sleep(2)

        logs = manager.get_container_logs(container_id)
        print("Container Logs:\n", logs)

        # Check if the expected output is in the logs
        if "Hello from Docker!" in logs:
            print("SUCCESS: Expected output found in container logs.")
        else:
            print("FAILURE: Expected output NOT found in container logs.")

        # Example: Execute a command inside a running container
        print("\n--- Executing command inside container ---")
        exit_code, stdout, stderr = manager.execute_command_in_container(
            container_id, ["sh", "-c", "echo 'Executed inside!'; exit 42"]
        )
        print(f"Exec command stdout: {stdout}")
        print(f"Exec command stderr: {stderr}")
        print(f"Exec command exit code: {exit_code}")
        if exit_code == 42 and "Executed inside!" in stdout:
            print("SUCCESS: Command executed inside container as expected.")
        else:
            print("FAILURE: Command execution inside container failed.")

    except subprocess.CalledProcessError as e:
        print(f"Error during Docker operation: {e}", file=sys.stderr)
        print(f"Stdout: {e.stdout}", file=sys.stderr)
        print(f"Stderr: {e.stderr}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"An unexpected error occurred: {e}", file=sys.stderr)
        sys.exit(1)
    finally:
        # Ensure cleanup
        try:
            manager.stop_container(container_name)
            manager.remove_container(container_name)
            print(f"Container {container_name} stopped and removed.")
        except subprocess.CalledProcessError:
            print(
                f"Container {container_name} not found or already removed during cleanup."
            )
        except Exception as e:
            print(f"Error during cleanup of {container_name}: {e}", file=sys.stderr)
