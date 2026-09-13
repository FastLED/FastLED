import os
import sys
import time

import httpx
from running_process import STDOUT, RunningProcess


# Get absolute paths
cwd = os.getcwd()
runner_path = os.path.join(
    cwd, ".build", "meson-quick", "examples", "example_runner.exe"
)
dll_path = os.path.join(cwd, ".build", "meson-quick", "examples", "example-Server.dll")

print(f"Starting server...")
print(f"  Runner: {runner_path}")
print(f"  DLL: {dll_path}")

# Start server in background
proc = RunningProcess(
    [runner_path, dll_path],
    auto_run=True,
    capture=True,
    stderr=STDOUT,
    encoding="utf-8",
    errors="replace",
)

# Wait for server to start
time.sleep(1.5)

# Try to connect
try:
    print("\nTesting GET /")
    response = httpx.get("http://localhost:8080/", timeout=5.0)
    print(f"✓ Status: {response.status_code}")
    print(f"  Response: {response.text!r}\n")

    print("Testing GET /ping")
    response = httpx.get("http://localhost:8080/ping", timeout=5.0)
    print(f"✓ Status: {response.status_code}")
    print(f"  Response: {response.text!r}\n")

    print("Testing GET /status")
    response = httpx.get("http://localhost:8080/status", timeout=5.0)
    print(f"✓ Status: {response.status_code}")
    print(f"  Response (JSON): {response.json()}\n")

    print("✓ All tests passed!")
    sys.exit(0)
except Exception as e:
    print(f"\n✗ Error: {e}")
    # Print server output for debugging
    proc.terminate()
    try:
        proc.wait(timeout=1)
    except TimeoutError:
        pass
    print(f"\nServer output:\n{proc.stdout}")
    sys.exit(1)
finally:
    proc.terminate()
    try:
        proc.wait(timeout=1)
    except:
        proc.kill()
