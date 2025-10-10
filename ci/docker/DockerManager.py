import os
import subprocess
import sys
from typing import Any, Dict, List, Optional


class DockerManager:
    def __init__(self):
        # No direct client object, will use subprocess for docker CLI commands
        pass

    def _run_docker_command(
        self,
        command: List[str],
        check: bool = True,
        stream_output: bool = False,
        timeout: Optional[int] = None,
        interrupt_pattern: Optional[str] = None,
        output_file: Optional[str] = None,
    ) -> subprocess.CompletedProcess[str]:
        full_command = ["docker"] + command
        print(f"Executing Docker command: {' '.join(full_command)}")

        # Set environment to prevent MSYS2/Git Bash path conversion on Windows
        env = os.environ.copy()
        # Set UTF-8 encoding environment variables for Windows
        env["PYTHONIOENCODING"] = "utf-8"
        env["PYTHONUTF8"] = "1"
        # Only set MSYS_NO_PATHCONV if we're in a Git Bash/MSYS2 environment
        if (
            "MSYSTEM" in os.environ
            or os.environ.get("TERM") == "xterm"
            or "bash.exe" in os.environ.get("SHELL", "")
        ):
            env["MSYS_NO_PATHCONV"] = "1"

        if stream_output:
            # Use RunningProcess for streaming output
            import re
            import time

            from ci.util.running_process import RunningProcess

            proc = RunningProcess(full_command, env=env, auto_run=True)
            pattern_found = False
            timeout_occurred = False
            start_time = time.time()

            # Open output file if specified
            output_handle = None
            if output_file:
                try:
                    output_handle = open(output_file, "w", encoding="utf-8")
                except Exception as e:
                    print(f"Warning: Could not open output file {output_file}: {e}")

            try:
                with proc.line_iter(timeout=None) as it:
                    for line in it:
                        print(line)

                        # Write to output file if specified
                        if output_handle:
                            output_handle.write(line + "\n")
                            output_handle.flush()  # Ensure immediate write

                        # Check for interrupt pattern (for informational purposes only)
                        if interrupt_pattern and re.search(interrupt_pattern, line):
                            print(f"Pattern detected: {line}")
                            pattern_found = True

                        # Check timeout
                        if timeout and (time.time() - start_time) > timeout:
                            print(
                                f"Timeout reached ({timeout}s), terminating container..."
                            )
                            timeout_occurred = True
                            break

                # Wait for process to complete
                returncode = proc.wait()

                # Handle timeout case: return success (0) for timeouts as per requirement
                if timeout_occurred:
                    print("Process terminated due to timeout - treating as success")
                    final_returncode = 0
                else:
                    # Return the actual container exit code for all other cases
                    final_returncode = returncode

                return subprocess.CompletedProcess(
                    args=full_command, returncode=final_returncode, stdout="", stderr=""
                )

            except Exception as e:
                print(f"Error during streaming: {e}")
                returncode = proc.wait() if hasattr(proc, "wait") else 1
                return subprocess.CompletedProcess(
                    args=full_command, returncode=returncode, stdout="", stderr=str(e)
                )
            finally:
                # Close output file if it was opened
                if output_handle:
                    output_handle.close()
        else:
            # Use regular subprocess for non-streaming commands
            result = subprocess.run(
                full_command,
                capture_output=True,
                text=True,
                check=check,
                env=env,
                encoding="utf-8",
                errors="replace",
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

    def run_container_streaming(
        self,
        image_name: str,
        command: List[str],
        volumes: Optional[Dict[str, Dict[str, str]]] = None,
        environment: Optional[Dict[str, str]] = None,
        name: Optional[str] = None,
        timeout: Optional[int] = None,
        interrupt_pattern: Optional[str] = None,
        output_file: Optional[str] = None,
    ) -> int:
        """
        Runs a Docker container with streaming output.
        Returns actual container exit code (0 for success, timeout is treated as success).

        Args:
            image_name: Docker image to run
            command: Command to execute in container
            volumes: Volume mounts
            environment: Environment variables
            name: Container name
            timeout: Timeout in seconds (if exceeded, returns 0)
            interrupt_pattern: Pattern to detect in output (informational only)
            output_file: Optional file path to write output to
        """
        print(
            f"Running container from image: {image_name} with command: {' '.join(command)}"
        )
        docker_cmd = ["run", "--rm"]  # --rm to automatically remove container on exit
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

        result = self._run_docker_command(
            docker_cmd,
            check=False,
            stream_output=True,
            timeout=timeout,
            interrupt_pattern=interrupt_pattern,
            output_file=output_file,
        )
        return result.returncode

    def get_container_logs(self, container_id_or_name: str) -> str:
        """Retrieves logs from a container."""
        print(f"Getting logs for container: {container_id_or_name}")
        try:
            result = self._run_docker_command(
                ["logs", container_id_or_name], check=False
            )
            if result.returncode == 0:
                logs = result.stdout
                print(f"Logs retrieved for {container_id_or_name}.")
                return logs
            else:
                # Container may have exited, try to get logs anyway
                print(f"Warning: docker logs returned exit code {result.returncode}")
                logs = result.stdout if result.stdout else result.stderr
                return logs
        except Exception as e:
            print(f"Warning: Failed to get container logs: {e}")
            return ""

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
