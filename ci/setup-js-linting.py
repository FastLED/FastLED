#!/usr/bin/env python3
"""
Simple JavaScript linting setup for FastLED using Deno
Downloads Deno binary and sets up basic linting - no Node.js/npm complexity
"""
import os
import platform
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

# Configuration
DENO_VERSION = "1.38.3"  # Latest stable version
TOOLS_DIR = Path(".js-tools")
DENO_DIR = TOOLS_DIR / "deno"


def get_deno_download_info():
    """Get Deno download URL and filename based on platform"""
    system = platform.system().lower()
    machine = platform.machine().lower()

    # Map platform names
    if system == "darwin":
        os_name = "apple-darwin"
    elif system == "linux":
        os_name = "unknown-linux-gnu"
    elif system == "windows":
        os_name = "pc-windows-msvc"
    else:
        raise ValueError(f"Unsupported platform: {system}")

    # Map architecture names
    if machine in ["x86_64", "amd64"]:
        arch = "x86_64"
    elif machine in ["aarch64", "arm64"]:
        arch = "aarch64"
    else:
        raise ValueError(f"Unsupported architecture: {machine}")

    filename = f"deno-{arch}-{os_name}.zip"
    url = (
        f"https://github.com/denoland/deno/releases/download/v{DENO_VERSION}/{filename}"
    )
    return url, filename


def download_and_extract_deno():
    """Download and extract Deno binary"""
    url, filename = get_deno_download_info()
    download_path = TOOLS_DIR / filename

    print(f"📦 Downloading Deno v{DENO_VERSION}...")
    TOOLS_DIR.mkdir(exist_ok=True)
    DENO_DIR.mkdir(exist_ok=True)

    if not download_path.exists():
        urllib.request.urlretrieve(url, download_path)
        print(f"✅ Downloaded {filename}")

    # Extract Deno binary
    deno_binary = DENO_DIR / ("deno.exe" if platform.system() == "Windows" else "deno")

    if not deno_binary.exists():
        print("📂 Extracting Deno...")
        with zipfile.ZipFile(download_path, "r") as zip_ref:
            zip_ref.extractall(DENO_DIR)

        # Make executable on Unix systems
        if platform.system() != "Windows":
            os.chmod(deno_binary, 0o755)

        print("✅ Deno extracted")

    # Clean up download file
    if download_path.exists():
        download_path.unlink()

    return deno_binary


def create_deno_config():
    """Create deno.json configuration"""
    config_content = {
        "lint": {
            "rules": {
                "tags": ["recommended"],
                "exclude": [
                    "no-unused-vars",  # Too noisy for existing code
                    "no-console",  # Console is needed for debugging
                ],
            },
            "include": ["src/platforms/wasm/"],
            "exclude": [],
        },
        "fmt": {
            "useTabs": False,
            "lineWidth": 100,
            "indentWidth": 2,
            "semiColons": True,
            "singleQuote": True,
            "proseWrap": "preserve",
            "include": ["src/platforms/wasm/"],
            "exclude": [],
        },
    }

    import json

    config_path = Path("deno.json")
    with open(config_path, "w") as f:
        json.dump(config_content, f, indent=2)

    print("✅ Created deno.json configuration")


def create_lint_script():
    """Create a simple lint script"""
    deno_binary = DENO_DIR / ("deno.exe" if platform.system() == "Windows" else "deno")

    lint_script_content = f"""#!/bin/bash
# FastLED JavaScript Linting Script (Deno-based)

# Colors for output
RED='\\033[0;31m'
GREEN='\\033[0;32m'
YELLOW='\\033[1;33m'
BLUE='\\033[0;34m'
NC='\\033[0m' # No Color

echo -e "${{BLUE}}🔍 FastLED JavaScript Linting (Deno)${{NC}}"

# Check if Deno is installed
if [ ! -f "{deno_binary}" ]; then
    echo -e "${{RED}}❌ Deno not found. Run: python3 ci/setup-js-linting.py${{NC}}"
    exit 1
fi

# Find JavaScript files in WASM platform
JS_FILES=$(find src/platforms/wasm -name "*.js" -type f 2>/dev/null)

if [ -z "$JS_FILES" ]; then
    echo -e "${{YELLOW}}⚠️  No JavaScript files found in src/platforms/wasm/${{NC}}"
    exit 0
fi

echo -e "${{BLUE}}Found JavaScript files:${{NC}}"
echo "$JS_FILES" | sed 's/^/  /'

# Run Deno linting
echo -e "${{BLUE}}Running Deno lint...${{NC}}"
if "{deno_binary}" lint --config deno.json; then
    echo -e "${{GREEN}}✅ JavaScript linting completed successfully${{NC}}"
else
    echo -e "${{RED}}❌ JavaScript linting failed${{NC}}"
    exit 1
fi

# Optionally run formatting check
echo -e "${{BLUE}}Checking JavaScript formatting...${{NC}}"
if "{deno_binary}" fmt --config deno.json --check; then
    echo -e "${{GREEN}}✅ JavaScript formatting is correct${{NC}}"
else
    echo -e "${{YELLOW}}⚠️  JavaScript formatting issues found. Run: {deno_binary} fmt --config deno.json${{NC}}"
fi
"""

    lint_script_path = Path("lint-js")
    with open(lint_script_path, "w") as f:
        f.write(lint_script_content)

    # Make executable
    os.chmod(lint_script_path, 0o755)
    print("✅ Created lint-js script")


def create_format_script():
    """Create a format script"""
    deno_binary = DENO_DIR / ("deno.exe" if platform.system() == "Windows" else "deno")

    format_script_content = f"""#!/bin/bash
# FastLED JavaScript Formatting Script (Deno-based)

# Colors for output
GREEN='\\033[0;32m'
BLUE='\\033[0;34m'
NC='\\033[0m' # No Color

echo -e "${{BLUE}}🎨 FastLED JavaScript Formatting (Deno)${{NC}}"

# Run Deno formatting
if "{deno_binary}" fmt --config deno.json; then
    echo -e "${{GREEN}}✅ JavaScript files formatted successfully${{NC}}"
else
    echo -e "${{RED}}❌ JavaScript formatting failed${{NC}}"
    exit 1
fi
"""

    format_script_path = Path("format-js")
    with open(format_script_path, "w") as f:
        f.write(format_script_content)

    # Make executable
    os.chmod(format_script_path, 0o755)
    print("✅ Created format-js script")


def update_gitignore():
    """Update .gitignore to exclude JS tools"""
    gitignore_path = Path(".gitignore")

    gitignore_entry = """
# JavaScript tools (generated by ci/setup-js-linting.py)
.js-tools/
"""

    if gitignore_path.exists():
        with open(gitignore_path, "r") as f:
            content = f.read()

        if ".js-tools/" not in content:
            with open(gitignore_path, "a") as f:
                f.write(gitignore_entry)
            print("✅ Updated .gitignore")
    else:
        with open(gitignore_path, "w") as f:
            f.write(gitignore_entry.strip())
        print("✅ Created .gitignore")


def test_deno_installation():
    """Test that Deno is working"""
    deno_binary = DENO_DIR / ("deno.exe" if platform.system() == "Windows" else "deno")

    print("🧪 Testing Deno installation...")

    # Test Deno version
    result = subprocess.run(
        [str(deno_binary), "--version"], capture_output=True, text=True
    )

    if result.returncode == 0:
        print("✅ Deno is working correctly")
        print(f"   Version: {result.stdout.split()[1]}")
    else:
        print("❌ Deno test failed")
        print(f"Error: {result.stderr}")


def main():
    """Main setup function"""
    print("🚀 Setting up simple JavaScript linting for FastLED (Deno-based)...")

    try:
        # Step 1: Download and extract Deno
        download_and_extract_deno()

        # Step 2: Create configuration
        create_deno_config()

        # Step 3: Create lint script
        create_lint_script()

        # Step 4: Create format script
        create_format_script()

        # Step 5: Update .gitignore
        update_gitignore()

        # Step 6: Test installation
        test_deno_installation()

        print("\n🎉 Simple JavaScript linting setup complete!")
        print("\nUsage:")
        print("  ./lint-js              # Lint JavaScript files")
        print("  ./format-js            # Format JavaScript files")
        print("  bash lint               # Run all linting (includes JS)")
        print("\nFiles created:")
        print("  .js-tools/deno/         # Deno binary (single file!)")
        print("  deno.json               # Deno configuration")
        print("  lint-js                 # JavaScript linting script")
        print("  format-js               # JavaScript formatting script")
        print("\nNo Node.js, npm, or complex setup required! 🎊")

    except Exception as e:
        print(f"\n❌ Setup failed: {e}")
        import traceback

        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
