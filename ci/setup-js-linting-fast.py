#!/usr/bin/env -S uv run --script


"""
Fast JavaScript linting setup for FastLED using Node.js + ESLint
Downloads Node.js binary and sets up fast linting - much faster than Deno
Uses uv run --script for dynamic package management
"""

import io
import json
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import zipfile
from pathlib import Path

# Import httpx for HTTP requests (dynamically managed by uv)
import httpx


# Force UTF-8 output for Windows consoles
if sys.platform.startswith("win"):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

# Configuration
NODE_VERSION = "20.11.0"  # LTS version
TOOLS_DIR = Path(".cache/js-tools")
NODE_DIR = TOOLS_DIR / "node"


def get_node_download_info():
    """Get Node.js download URL and filename based on platform"""
    system = platform.system().lower()
    machine = platform.machine().lower()

    # Map architecture names
    if machine in ["x86_64", "amd64"]:
        arch = "x64"
    elif machine in ["aarch64", "arm64"]:
        arch = "arm64"
    else:
        raise ValueError(f"Unsupported architecture: {machine}")

    if system == "windows":
        filename = f"node-v{NODE_VERSION}-win-{arch}.zip"
        is_zip = True
    elif system == "darwin":
        filename = f"node-v{NODE_VERSION}-darwin-{arch}.tar.gz"
        is_zip = False
    elif system == "linux":
        filename = f"node-v{NODE_VERSION}-linux-{arch}.tar.xz"
        is_zip = False
    else:
        raise ValueError(f"Unsupported platform: {system}")

    url = f"https://nodejs.org/dist/v{NODE_VERSION}/{filename}"
    return url, filename, is_zip


def download_and_extract_node():
    """Download and extract Node.js binary"""
    url, filename, is_zip = get_node_download_info()
    download_path = TOOLS_DIR / filename

    # Get architecture for extraction path
    machine = platform.machine().lower()
    if machine in ["x86_64", "amd64"]:
        arch = "x64"
    elif machine in ["aarch64", "arm64"]:
        arch = "arm64"
    else:
        raise ValueError(f"Unsupported architecture: {machine}")

    print(f"Downloading Node.js v{NODE_VERSION}...")
    TOOLS_DIR.mkdir(exist_ok=True)
    NODE_DIR.mkdir(exist_ok=True)

    if not download_path.exists():
        print(f"Downloading from: {url}")
        with httpx.stream("GET", url, follow_redirects=True) as response:
            response.raise_for_status()
            with open(download_path, "wb") as f:
                for chunk in response.iter_bytes(chunk_size=8192):
                    f.write(chunk)
        print(f"SUCCESS: Downloaded {filename}")

    # Extract Node.js
    if platform.system() == "Windows":
        node_exe = NODE_DIR / "node.exe"
    else:
        node_exe = NODE_DIR / "bin" / "node"

    if not node_exe.exists():
        print("Extracting Node.js...")

        if is_zip:
            with zipfile.ZipFile(download_path, "r") as zip_ref:
                zip_ref.extractall(NODE_DIR)
            # Move contents from nested folder to NODE_DIR
            nested_dir = NODE_DIR / f"node-v{NODE_VERSION}-win-{arch}"
            if nested_dir.exists():
                for item in nested_dir.iterdir():
                    if item.is_dir():
                        # For directories, move contents recursively
                        target_dir = NODE_DIR / item.name
                        if target_dir.exists():
                            shutil.rmtree(target_dir)
                        shutil.move(str(item), str(target_dir))
                    else:
                        # For files, move directly
                        target_file = NODE_DIR / item.name
                        if target_file.exists():
                            target_file.unlink()
                        item.rename(target_file)
                nested_dir.rmdir()
        else:
            with tarfile.open(download_path, "r:*") as tar_ref:
                tar_ref.extractall(NODE_DIR)
            # Move contents from nested folder to NODE_DIR
            nested_dir = (
                NODE_DIR / f"node-v{NODE_VERSION}-{platform.system().lower()}-x64"
            )
            if nested_dir.exists():
                for item in nested_dir.iterdir():
                    item.rename(NODE_DIR / item.name)
                nested_dir.rmdir()

        print("SUCCESS: Node.js extracted")

    # Clean up download file
    if download_path.exists():
        download_path.unlink()

    return node_exe


def setup_eslint():
    """Install ESLint and create configuration"""
    if platform.system() == "Windows":
        node_exe = NODE_DIR / "node.exe"
        npm_exe = NODE_DIR / "npm.cmd"
    else:
        node_exe = NODE_DIR / "bin" / "node"
        npm_exe = NODE_DIR / "bin" / "npm"

    print("Installing ESLint...")

    # Create package.json
    package_json = {
        "name": "fastled-js-linting",
        "version": "1.0.0",
        "private": True,
        "dependencies": {"eslint": "^8.56.0"},
    }

    with open(TOOLS_DIR / "package.json", "w") as f:
        json.dump(package_json, f, indent=2)

    # Install ESLint
    try:
        # Use absolute path for npm executable
        npm_exe_abs = (
            (TOOLS_DIR / "node" / "npm.cmd").resolve()
            if platform.system() == "Windows"
            else (TOOLS_DIR / "node" / "bin" / "npm").resolve()
        )

        # On Windows, use shell=True to properly execute .cmd files
        if platform.system() == "Windows":
            result = subprocess.run(
                [str(npm_exe_abs), "install"],
                cwd=TOOLS_DIR,
                check=True,
                capture_output=True,
                text=True,
                shell=True,
            )
        else:
            result = subprocess.run(
                [str(npm_exe_abs), "install"],
                cwd=TOOLS_DIR,
                check=True,
                capture_output=True,
                text=True,
            )
        # npm install succeeded
    except subprocess.CalledProcessError as e:
        # Show stderr output for debugging
        print(f"npm install stderr: {e.stderr}")
        print(f"npm install stdout: {e.stdout}")
        raise

    # ESLint config is now in ci/.eslintrc.js (tracked in git)
    # No need to create it here since it's already in version control

    print("SUCCESS: ESLint configured")


def create_fast_lint_script():
    """Create fast linting script"""
    if platform.system() == "Windows":
        node_exe = NODE_DIR / "node.exe"
        eslint_exe = TOOLS_DIR / "node_modules" / ".bin" / "eslint.cmd"
    else:
        node_exe = NODE_DIR / "bin" / "node"
        eslint_exe = TOOLS_DIR / "node_modules" / ".bin" / "eslint"

    script_content = f"""#!/bin/bash
# FastLED JavaScript Linting Script (Node.js + ESLint - FAST!)

# Colors for output
RED='\\033[0;31m'
GREEN='\\033[0;32m'
YELLOW='\\033[1;33m'
BLUE='\\033[0;34m'
NC='\\033[0m' # No Color

echo -e "${{BLUE}}FAST FastLED JavaScript Linting (Node.js + ESLint - FAST!)${{NC}}"

# Check if ESLint is installed
if [ ! -f ".cache/js-tools/node_modules/.bin/eslint{".cmd" if platform.system() == "Windows" else ""}" ]; then
    echo -e "${{RED}}ERROR: ESLint not found. Run: uv run ci/setup-js-linting-fast.py${{NC}}"
    exit 1
fi

# Find JavaScript files in WASM platform
JS_FILES=$(find src/platforms/wasm -name "*.js" -type f 2>/dev/null)

if [ -z "$JS_FILES" ]; then
    echo -e "${{YELLOW}}WARNING: No JavaScript files found in src/platforms/wasm/${{NC}}"
    exit 0
fi

echo -e "${{BLUE}}Found JavaScript files:${{NC}}"
echo "$JS_FILES" | sed 's/^/  /'

# Run ESLint
echo -e "${{BLUE}}Running ESLint...${{NC}}"
cd .cache/js-tools
if "./node_modules/.bin/eslint{".cmd" if platform.system() == "Windows" else ""}" --no-eslintrc --no-inline-config -c .eslintrc.js ../../src/platforms/wasm/compiler/*.js ../../src/platforms/wasm/compiler/modules/*.js; then
    echo -e "${{GREEN}}SUCCESS: JavaScript linting completed successfully${{NC}}"
else
    echo -e "${{RED}}ERROR: JavaScript linting failed${{NC}}"
    exit 1
fi
"""

    # Lint script is now in ci/lint-js-fast (tracked in git)
    # No need to create it here since it's already in version control
    script_path = Path("ci/lint-js-fast")
    # Script already exists in ci/ and is tracked in git
    # Just ensure it's executable
    if platform.system() != "Windows" and script_path.exists():
        os.chmod(script_path, 0o755)

    print("SUCCESS: Fast lint script created")


def main():
    """Main setup function"""
    print("Setting up fast JavaScript linting (Node.js + ESLint)...")

    try:
        node_exe = download_and_extract_node()
        setup_eslint()
        create_fast_lint_script()

        print("\\nFast JavaScript linting setup complete!")
        print("\\nUsage:")
        print("  bash ci/lint-js-fast                 # Fast linting with ESLint")
        print("  For more info: ci/js/README.md")

    except Exception as e:
        print(f"Setup failed: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
