#!/usr/bin/env -S uv run --script
#
# /// script
# requires-python = ">=3.11"
# dependencies = ["httpx", "pathspec"]
# ///

"""
Simple JavaScript linting setup for FastLED using Deno
Downloads Deno binary and sets up basic linting - no Node.js/npm complexity
Uses uv run --script for dynamic package management
"""
import io
import os
import platform
import subprocess
import sys
import zipfile
from pathlib import Path

# Import httpx for HTTP requests (dynamically managed by uv)
import httpx

# Force UTF-8 output for Windows consoles
if sys.platform.startswith("win"):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

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
    """Download and extract Deno binary using httpx"""
    url, filename = get_deno_download_info()
    download_path = TOOLS_DIR / filename

    print(f"📦 Downloading Deno v{DENO_VERSION}...")
    TOOLS_DIR.mkdir(exist_ok=True)
    DENO_DIR.mkdir(exist_ok=True)

    if not download_path.exists():
        print(f"🌐 Downloading from: {url}")
        with httpx.stream("GET", url, follow_redirects=True) as response:
            response.raise_for_status()
            with open(download_path, "wb") as f:
                for chunk in response.iter_bytes(chunk_size=8192):
                    f.write(chunk)
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
        "compilerOptions": {
            "allowJs": True,
            "checkJs": False,
            "strict": False,
            "noImplicitAny": False,
            "noImplicitReturns": False,
            "noUnusedLocals": False,
            "noUnusedParameters": False,
            "exactOptionalPropertyTypes": False,
            "noImplicitOverride": False,
            "noPropertyAccessFromIndexSignature": False,
            "noUncheckedIndexedAccess": False,
            "strictNullChecks": False,
            "strictFunctionTypes": False,
            "noImplicitThis": False,
            "suppressExcessPropertyErrors": True,
            "ignoreDeprecations": "5.0",
            "skipLibCheck": True,
            "lib": ["dom", "dom.asynciterable", "es2022", "webworker"],
        },
        "lint": {
            "files": {
                "include": [
                    "src/platforms/wasm/compiler/*.js",
                    "src/platforms/wasm/compiler/modules/*.js",
                ],
                "exclude": [
                    "**/*.min.js",
                    "**/vendor/**",
                    "**/node_modules/**",
                    "**/.build/**",
                    "**/build/**",
                    "**/dist/**",
                    "**/tmp/**",
                    "**/three.min.js",
                    "**/codemirror.min.js",
                    "**/bootstrap.min.js",
                ],
            },
            "rules": {
                "tags": ["recommended"],
                "include": [
                    "eqeqeq",
                    "no-eval",
                    "no-throw-literal",
                    "prefer-const",
                    "no-await-in-loop",
                    "default-param-last",
                    "guard-for-in",
                    "no-explicit-any",
                    "no-implicit-coercion",
                    "no-non-null-assertion",
                    "no-prototype-builtins",
                    "single-var-declarator",
                    "adjacent-overload-signatures",
                    "ban-untagged-todo",
                    "for-direction",
                    "getter-return",
                    "no-array-constructor",
                    "no-async-promise-executor",
                    "no-case-declarations",
                    "no-class-assign",
                    "no-compare-neg-zero",
                    "no-cond-assign",
                    "no-const-assign",
                    "no-constant-condition",
                    "no-control-regex",
                    "no-debugger",
                    "no-delete-var",
                    "no-dupe-args",
                    "no-dupe-keys",
                    "no-duplicate-case",
                    "no-empty",
                    "no-empty-character-class",
                    "no-empty-interface",
                    "no-empty-pattern",
                    "no-ex-assign",
                    "no-extra-boolean-cast",
                    "no-fallthrough",
                    "no-func-assign",
                    "no-global-assign",
                    "no-import-assign",
                    "no-inner-declarations",
                    "no-invalid-regexp",
                    "no-irregular-whitespace",
                    "no-misleading-character-class",
                    "no-new-symbol",
                    "no-obj-calls",
                    "no-octal",
                    "no-redeclare",
                    "no-regex-spaces",
                    "no-setter-return",
                    "no-shadow-restricted-names",
                    "no-sparse-arrays",
                    "no-this-before-super",
                    "no-unexpected-multiline",
                    "no-unreachable",
                    "no-unsafe-finally",
                    "no-unsafe-negation",
                    "no-unused-labels",
                    "no-useless-catch",
                    "no-with",
                    "require-yield",
                    "use-isnan",
                    "valid-typeof",
                ],
                "exclude": [
                    "no-console",
                    "prefer-primordials",
                    "no-window-prefix",
                    "camelcase",
                    "no-undef",
                    "no-unused-vars",
                ],
            },
        },
        "fmt": {
            "files": {
                "include": [
                    "src/platforms/wasm/compiler/*.js",
                    "src/platforms/wasm/compiler/modules/*.js",
                ],
                "exclude": [
                    "**/*.min.js",
                    "**/vendor/**",
                    "**/node_modules/**",
                    "**/.build/**",
                    "**/build/**",
                    "**/dist/**",
                    "**/tmp/**",
                ],
            },
            "useTabs": False,
            "lineWidth": 100,
            "indentWidth": 2,
            "semiColons": True,
            "singleQuote": True,
            "proseWrap": "preserve",
        },
        "exclude": [
            "**/*.min.js",
            "**/vendor/**",
            "**/node_modules/**",
            "**/.build/**",
            "**/build/**",
            "**/dist/**",
            "**/tmp/**",
            "**/three.min.js",
            "**/codemirror.min.js",
            "**/bootstrap.min.js",
        ],
    }

    import json

    config_path = Path("deno.json")
    with open(config_path, "w", encoding="utf-8") as f:
        json.dump(config_content, f, indent=2, ensure_ascii=False)

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
    echo -e "${{RED}}❌ Deno not found. Run: uv run ci/setup-js-linting.py${{NC}}"
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
    with open(lint_script_path, "w", encoding="utf-8") as f:
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
    with open(format_script_path, "w", encoding="utf-8") as f:
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
        with open(gitignore_path, "r", encoding="utf-8") as f:
            content = f.read()

        if ".js-tools/" not in content:
            with open(gitignore_path, "a", encoding="utf-8") as f:
                f.write(gitignore_entry)
            print("✅ Updated .gitignore")
    else:
        with open(gitignore_path, "w", encoding="utf-8") as f:
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
    print("📦 Using uv run --script for dynamic package management")

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
        print("📦 Dependencies managed by uv run --script")

    except Exception as e:
        print(f"\n❌ Setup failed: {e}")
        import traceback

        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
