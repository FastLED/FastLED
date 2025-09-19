#!/usr/bin/env python3
"""
TypeScript checking for FastLED JavaScript/WASM compiler modules.

This script runs TypeScript compiler with JSDoc checking enabled
to catch type errors in JavaScript files that would be caught
at compile time in TypeScript.
"""

import subprocess
import sys
from pathlib import Path


def check_js_types():
    """Run TypeScript checking on JavaScript files."""
    wasm_compiler_dir = (
        Path(__file__).parent.parent / "src" / "platforms" / "wasm" / "compiler"
    )

    if not wasm_compiler_dir.exists():
        print(f"❌ WASM compiler directory not found: {wasm_compiler_dir}")
        return 1

    jsconfig_path = wasm_compiler_dir / "jsconfig.json"
    if not jsconfig_path.exists():
        print(f"❌ jsconfig.json not found: {jsconfig_path}")
        return 1

    print(f"🔍 Checking JavaScript types in: {wasm_compiler_dir}")

    try:
        # Run TypeScript compiler with noEmit to only check types
        result = subprocess.run(
            ["npx", "tsc", "--noEmit", "--project", str(jsconfig_path)],
            cwd=wasm_compiler_dir,
            capture_output=True,
            text=True,
        )

        if result.returncode == 0:
            print("✅ JavaScript type checking passed!")
            return 0
        else:
            print("❌ JavaScript type checking failed:")
            print(result.stdout)
            print(result.stderr)
            return 1

    except FileNotFoundError:
        print(
            "❌ TypeScript (tsc) not found. Please install: npm install -g typescript"
        )
        return 1
    except Exception as e:
        print(f"❌ Error running type check: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(check_js_types())
