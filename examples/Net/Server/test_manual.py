import httpx
import time
import subprocess
import sys
import os

# Get absolute paths
cwd = os.getcwd()
runner_path = os.path.join(cwd, ".build", "meson-quick", "examples", "example_runner.exe")
dll_path = os.path.join(cwd, ".build", "meson-quick", "examples", "example-Network.dll")

print("Starting server (will run in foreground)...")
print("Open another terminal and run:")
print("  curl http://localhost:8080/")
print("  curl http://localhost:8080/ping")
print("  curl http://localhost:8080/status")
print()

# Run server in foreground (no stdout capture)
subprocess.run([runner_path, dll_path])
