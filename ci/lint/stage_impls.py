"""Implementation of individual lint stages."""

import os
import subprocess
import sys
import time
from pathlib import Path

from ci.cpp_lint_cache import (
    check_cpp_files_changed,
    invalidate_cpp_cache,
    mark_cpp_lint_success,
)
from ci.js_lint_cache import check_js_files_changed
from ci.python_lint_cache import (
    check_python_files_changed,
    invalidate_python_cache,
    mark_python_lint_success,
)


class Colors:
    """ANSI color codes."""

    RED = "\033[0;31m"
    GREEN = "\033[0;32m"
    BLUE = "\033[0;34m"
    NC = "\033[0m"  # No Color


def run_cpp_lint(no_fingerprint: bool, run_full: bool, run_iwyu: bool) -> bool:
    """
    Run C++ linting stage.

    Args:
        no_fingerprint: Skip fingerprint cache
        run_full: Run full linting including IWYU
        run_iwyu: Run IWYU only

    Returns:
        True if linting passed, False otherwise
    """
    print("")
    print("🔧 C++ LINTING")
    print("---------------")

    # Set clang-format style
    os.environ["CLANG_FORMAT_STYLE"] = "{SortIncludes: false}"

    # Check if linting is needed
    needs_cpp_lint = False
    skipped = False

    if no_fingerprint:
        print("Running C++ linting (--no-fingerprint flag set)")
        needs_cpp_lint = True
    else:
        if check_cpp_files_changed():
            needs_cpp_lint = True
        else:
            needs_cpp_lint = False

    if needs_cpp_lint:
        # Run clang-format on folders (currently empty list in bash script)
        folders: list[str] = [
            # "src/lib8tion",
            # "src/platforms/stub",
            # "src/platforms/apollo3",  # clang-format breaks apollo3
            # "src/platforms/esp/8266",  # clang-format breaks esp8266
            # "src/platforms/arm",  # clang-format breaks arm
            # "src/fx",
            # "src/fl",
            # "src/platforms/wasm",
        ]

        for folder in folders:
            print(f"Running clang-format on {folder}")
            result = subprocess.run(
                ["uv", "run", "ci/run-clang-format.py", "-i", "-r", folder],
                capture_output=False,
            )
            if result.returncode != 0:
                # Retry once (as bash script does)
                result = subprocess.run(
                    ["uv", "run", "ci/run-clang-format.py", "-i", "-r", folder],
                    capture_output=False,
                )

        # Run unified C++ checker
        print("")
        print("🔍 C++ CUSTOM LINTERS (UNIFIED)")
        print("-------------------------------")
        print(
            "Running unified C++ checker (all linters + unity build + cpp_lint in one pass)"
        )

        result = subprocess.run(
            ["uv", "run", "python", "ci/lint_cpp/run_all_checkers.py"],
            capture_output=False,
        )

        if result.returncode == 0:
            if not no_fingerprint:
                mark_cpp_lint_success()
            print("✅ C++ linting passed")
        else:
            if not no_fingerprint:
                invalidate_cpp_cache()
            print("❌ Unified C++ linter failed")
            return False
    else:
        skipped = True
        print("⏭️  C++ linting skipped (no changes)")

    return True


def run_iwyu_analysis(run_full: bool, run_iwyu: bool) -> bool:
    """
    Run IWYU (Include-What-You-Use) analysis.

    Args:
        run_full: Run full analysis
        run_iwyu: Run IWYU only

    Returns:
        True if analysis passed, False otherwise
    """
    print("")
    print("🔍 INCLUDE-WHAT-YOU-USE ANALYSIS")
    print("---------------------------------")

    if run_full or run_iwyu:
        print("Running IWYU on C++ test suite...")
        result = subprocess.run(
            ["uv", "run", "python", "ci/ci-iwyu.py", "--quiet"],
            capture_output=False,
        )

        if result.returncode == 0:
            print("✅ IWYU analysis passed")
            return True
        else:
            print("❌ IWYU analysis failed")
            return False
    else:
        print("⏭️  IWYU skipped (use --full or --iwyu to enable)")
        return True


def run_js_lint(no_fingerprint: bool) -> bool:
    """
    Run JavaScript linting stage.

    Args:
        no_fingerprint: Skip fingerprint cache

    Returns:
        True if linting passed, False otherwise
    """
    print("")
    print("🌐 JAVASCRIPT LINTING & TYPE CHECKING")
    print("--------------------------------------")

    # Set environment variable for fingerprint bypass
    if no_fingerprint:
        os.environ["FASTLED_NO_FINGERPRINT"] = "1"

    # Determine ESLint executable path
    eslint_exe = ""
    if sys.platform in ("win32", "cygwin", "msys"):
        eslint_exe = ".cache/js-tools/node_modules/.bin/eslint.cmd"
    else:
        eslint_exe = ".cache/js-tools/node_modules/.bin/eslint"

    # Check if fast linting is available
    lint_script = Path("ci/lint-js-fast")
    eslint_path = Path(eslint_exe)

    if lint_script.exists() and eslint_path.exists():
        print(
            f"{Colors.BLUE}🚀 Using fast JavaScript linting (Node.js + ESLint){Colors.NC}"
        )

        # On Windows, use shell=True to properly invoke bash scripts
        if sys.platform in ("win32", "cygwin", "msys"):
            result = subprocess.run(
                "bash ci/lint-js-fast",
                shell=True,
                capture_output=False,
            )
        else:
            result = subprocess.run(
                ["bash", "ci/lint-js-fast"],
                capture_output=False,
            )

        if result.returncode != 0:
            print(f"{Colors.RED}❌ Fast JavaScript linting failed{Colors.NC}")
            return False
    else:
        print(
            f"{Colors.BLUE}⚠️  Fast JavaScript linting not available. Setting up now...{Colors.NC}"
        )
        result = subprocess.run(
            ["uv", "run", "ci/setup-js-linting-fast.py"],
            capture_output=False,
        )

        if result.returncode == 0:
            print(f"{Colors.GREEN}✅ JavaScript linting setup complete{Colors.NC}")

            if lint_script.exists():
                print(f"{Colors.BLUE}🚀 Running JavaScript linting...{Colors.NC}")

                # On Windows, use shell=True to properly invoke bash scripts
                if sys.platform in ("win32", "cygwin", "msys"):
                    result = subprocess.run(
                        "bash ci/lint-js-fast",
                        shell=True,
                        capture_output=False,
                    )
                else:
                    result = subprocess.run(
                        ["bash", "ci/lint-js-fast"],
                        capture_output=False,
                    )

                if result.returncode != 0:
                    print(f"{Colors.RED}❌ JavaScript linting failed{Colors.NC}")
                    return False
            else:
                print(f"{Colors.RED}❌ Failed to create lint script{Colors.NC}")
                return False
        else:
            print(f"{Colors.RED}❌ Failed to setup JavaScript linting{Colors.NC}")
            print(
                f"{Colors.BLUE}💡 You can manually run: uv run ci/setup-js-linting-fast.py{Colors.NC}"
            )
            return False

    return True


def run_ruff() -> bool:
    """
    Run ruff linting and formatting.

    Returns:
        True if linting passed, False otherwise
    """
    print("Running ruff check (linting + import sorting)")

    # Run ruff check
    result = subprocess.run(
        ["uv", "run", "ruff", "check", "--fix", "test.py"],
        capture_output=False,
    )
    if result.returncode != 0:
        return False

    result = subprocess.run(
        [
            "uv",
            "run",
            "ruff",
            "check",
            "--fix",
            "ci",
            "--exclude",
            "ci/tmp/",
            "--exclude",
            "ci/wasm/",
        ],
        capture_output=False,
    )
    if result.returncode != 0:
        return False

    print("Running ruff format (formatting)")

    # Run ruff format
    result = subprocess.run(
        ["uv", "run", "ruff", "format", "test.py"],
        capture_output=False,
    )
    if result.returncode != 0:
        return False

    result = subprocess.run(
        [
            "uv",
            "run",
            "ruff",
            "format",
            "ci",
            "--exclude",
            "ci/tmp/",
            "--exclude",
            "ci/wasm/",
        ],
        capture_output=False,
    )
    if result.returncode != 0:
        return False

    print("Running KeyboardInterrupt handler checks")

    # Run keyboard interrupt checker
    result = subprocess.run(
        [
            "uv",
            "run",
            "python",
            "ci/lint_python/keyboard_interrupt_checker.py",
            "test.py",
            "ci",
            "--exclude",
            "ci/tmp",
            "ci/wasm",
            ".pio",
        ],
        capture_output=False,
    )
    if result.returncode != 0:
        return False

    print("Running sys.path.insert/append checks")

    # Run sys.path checker
    result = subprocess.run(
        [
            "uv",
            "run",
            "python",
            "ci/lint_python/sys_path_checker.py",
            "test.py",
            "ci",
            "--exclude",
            "ci/tmp",
            "ci/wasm",
            ".pio",
        ],
        capture_output=False,
    )
    if result.returncode != 0:
        return False

    print("✅ ruff passed")
    return True


def run_ty() -> bool:
    """
    Run ty type checking.

    Returns:
        True if type checking passed, False otherwise
    """
    print("Running ty (fast type check)")

    result = subprocess.run(
        ["uv", "run", "ty", "check", "--output-format", "concise"],
        capture_output=False,
    )

    if result.returncode == 0:
        print("✅ ty passed")
        return True
    else:
        print("❌ ty failed")
        return False


def run_pyright(no_fingerprint: bool) -> bool:
    """
    Run pyright strict type checking.

    Args:
        no_fingerprint: Skip fingerprint cache

    Returns:
        True if type checking passed, False otherwise
    """
    needs_pyright = False

    if no_fingerprint:
        print("Running pyright (--no-fingerprint flag set)")
        needs_pyright = True
    else:
        if check_python_files_changed():
            needs_pyright = True
        else:
            needs_pyright = False

    if needs_pyright:
        result = subprocess.run(
            ["uv", "run", "pyright", "--threads"],
            capture_output=False,
        )

        if result.returncode == 0:
            if not no_fingerprint:
                mark_python_lint_success()
            print("✅ pyright passed")
            return True
        else:
            if not no_fingerprint:
                invalidate_python_cache()
            print("❌ pyright failed")
            return False
    else:
        print("⏭️  pyright skipped (no changes)")
        return True


def run_python_pipeline(no_fingerprint: bool, run_pyright_flag: bool) -> bool:
    """
    Run Python linting pipeline: ruff -> ty -> pyright (optional).

    This runs as a single stage to match bash script behavior.

    Args:
        no_fingerprint: Skip fingerprint cache
        run_pyright_flag: Whether to run pyright

    Returns:
        True if all stages passed, False otherwise
    """
    # Phase A: ruff
    if not run_ruff():
        return False

    # Phase B: ty
    if not run_ty():
        return False

    # Phase C: pyright (only with --strict flag)
    if run_pyright_flag:
        if not run_pyright(no_fingerprint):
            return False
    else:
        print("⏭️  pyright skipped (use --strict to enable)")

    return True


def run_cpp_lint_single_file(file_path: str) -> bool:
    """Run C++ linting on a single file.

    Delegates to run_all_checkers.py which already supports single-file mode.

    Args:
        file_path: Absolute path to the C++ file

    Returns:
        True if linting passed, False otherwise
    """
    print(f"🔧 C++ lint: {os.path.relpath(file_path)}")

    result = subprocess.run(
        ["uv", "run", "python", "ci/lint_cpp/run_all_checkers.py", file_path],
        capture_output=False,
    )

    if result.returncode == 0:
        print(f"  ✅ passed")
        return True
    else:
        print(f"  ❌ failed")
        return False


def run_python_lint_single_file(file_path: str, strict: bool = False) -> bool:
    """Run Python linting on a single file (ruff check + format + optional pyright).

    Args:
        file_path: Absolute path to the Python file
        strict: If True, also run pyright type checking on the file

    Returns:
        True if linting passed, False otherwise
    """
    print(f"🐍 Python lint: {os.path.relpath(file_path)}")

    # Run ruff check with auto-fix
    result = subprocess.run(
        ["uv", "run", "ruff", "check", "--fix", file_path],
        capture_output=False,
    )
    if result.returncode != 0:
        print("  ❌ ruff check failed")
        return False

    # Run ruff format
    result = subprocess.run(
        ["uv", "run", "ruff", "format", file_path],
        capture_output=False,
    )
    if result.returncode != 0:
        print("  ❌ ruff format failed")
        return False

    # Run pyright strict type checking (optional)
    if strict:
        result = subprocess.run(
            ["uv", "run", "pyright", file_path],
            capture_output=False,
        )
        if result.returncode != 0:
            print("  ❌ pyright failed")
            return False

    print("  ✅ passed")
    return True


def run_js_lint_single_file(file_path: str) -> bool:
    """Run JavaScript/TypeScript linting on a single file (ESLint).

    ESLint must be run from .cache/js-tools/ with its local config,
    matching the behavior of ci/lint-js-fast.

    Args:
        file_path: Absolute path to the JS/TS file

    Returns:
        True if linting passed, False otherwise
    """
    print(f"🌐 JS lint: {os.path.relpath(file_path)}")

    # ESLint runs from .cache/js-tools/ with its local config
    js_tools_dir = Path(".cache/js-tools").resolve()
    if sys.platform in ("win32", "cygwin", "msys"):
        eslint_exe = js_tools_dir / "node_modules" / ".bin" / "eslint.cmd"
    else:
        eslint_exe = js_tools_dir / "node_modules" / ".bin" / "eslint"

    if not eslint_exe.exists():
        print(
            "  ⚠️  ESLint not available (run 'uv run ci/setup-js-linting-fast.py' to set up)"
        )
        print("  ⏭️  skipped")
        return True  # Don't fail if eslint isn't set up

    eslint_config = js_tools_dir / ".eslintrc.js"
    if not eslint_config.exists():
        print("  ⚠️  ESLint config not found (.cache/js-tools/.eslintrc.js)")
        print("  ⏭️  skipped")
        return True

    # Compute path relative to .cache/js-tools/ (where eslint runs from)
    abs_file = Path(file_path).resolve()
    rel_file = os.path.relpath(str(abs_file), str(js_tools_dir))

    result = subprocess.run(
        [
            str(eslint_exe),
            "--no-eslintrc",
            "--no-inline-config",
            "-c",
            ".eslintrc.js",
            rel_file,
        ],
        capture_output=False,
        cwd=str(js_tools_dir),
    )

    if result.returncode == 0:
        print("  ✅ passed")
        return True
    else:
        print("  ❌ eslint failed")
        return False
