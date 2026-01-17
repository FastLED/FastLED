from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
FastLED Docker Build Utilities

Extracted Python logic from build.sh for better maintainability and testability.
This module handles platformio.ini generation and platform-related operations.
"""

import logging
import subprocess
from pathlib import Path
from typing import Optional


# Setup logging
logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")
logger = logging.getLogger(__name__)


def generate_platformio_ini(
    board_name: str,
    project_root: str = "/fastled",
    include_platformio_section: bool = True,
) -> tuple[bool, str]:
    """
    Generate platformio.ini from board configuration.

    This is the source of truth - platformio.ini is derived from board configs,
    not canonical.

    Args:
        board_name: Board name (e.g., "uno", "esp32dev")
        project_root: Root directory for FastLED project
        include_platformio_section: Whether to include [platformio] section

    Returns:
        Tuple of (success: bool, message: str)
    """
    logger.info(f"Generating platformio.ini from board: {board_name}")

    try:
        # Import FastLED's board configuration
        from ci.boards import create_board

        board = create_board(board_name)
        ini_content = board.to_platformio_ini(
            include_platformio_section=include_platformio_section,
            project_root=project_root,
            additional_libs=["FastLED"],
        )

        # Write to platformio.ini in project root
        ini_path = Path(project_root) / "platformio.ini"
        with open(ini_path, "w") as f:
            f.write(ini_content)

        logger.info(f"Successfully generated platformio.ini at {ini_path}")
        return True, f"Generated platformio.ini for board: {board_name}"

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        error_msg = f"Failed to generate platformio.ini for {board_name}: {e}"
        logger.error(error_msg)
        return False, error_msg


def get_platform_from_ini(ini_path: str = "platformio.ini") -> Optional[str]:
    """
    Extract platform value from first environment in platformio.ini.

    Args:
        ini_path: Path to platformio.ini file

    Returns:
        Platform string or None if not found
    """
    try:
        with open(ini_path, "r") as f:
            for line in f:
                line = line.strip()
                if line.startswith("platform"):
                    # Extract value after '='
                    if "=" in line:
                        value = line.split("=", 1)[1].strip()
                        # Handle multi-line values (take first line)
                        return value.split("\n")[0].strip()
    except FileNotFoundError:
        logger.warning(f"platformio.ini not found at {ini_path}")
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        logger.error(f"Error reading platformio.ini: {e}")

    return None


def get_platform_packages_from_ini(ini_path: str = "platformio.ini") -> Optional[str]:
    """
    Extract platform_packages value from first environment in platformio.ini.

    Args:
        ini_path: Path to platformio.ini file

    Returns:
        platform_packages string or None if not found
    """
    try:
        with open(ini_path, "r") as f:
            for line in f:
                line = line.strip()
                if line.startswith("platform_packages"):
                    # Extract value after '='
                    if "=" in line:
                        value = line.split("=", 1)[1].strip()
                        # Handle multi-line values (take first line)
                        return value.split("\n")[0].strip()
    except FileNotFoundError:
        logger.warning(f"platformio.ini not found at {ini_path}")
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        logger.error(f"Error reading platformio.ini: {e}")

    return None


def run_command(cmd: list[str], description: str = "") -> bool:
    """
    Run a shell command and log output.

    Args:
        cmd: Command as list (e.g., ["pio", "pkg", "install"])
        description: Human-readable description of what's running

    Returns:
        True if successful, False otherwise
    """
    try:
        if description:
            logger.info(f"Running: {description}")
        logger.info(f"Command: {' '.join(cmd)}")

        result = subprocess.run(cmd, check=False, capture_output=False, text=True)

        if result.returncode != 0:
            logger.error(f"Command failed with return code {result.returncode}")
            return False

        return True

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        logger.error(f"Error running command: {e}")
        return False


def install_platform(platform: str, skip_dependencies: bool = True) -> tuple[bool, str]:
    """
    Install PlatformIO platform definition.

    Args:
        platform: Platform URL or name (e.g., "espressif32")
        skip_dependencies: Whether to skip dependency resolution

    Returns:
        Tuple of (success: bool, message: str)
    """
    logger.info(f"Installing PlatformIO platform: {platform}")

    try:
        cmd = ["pio", "pkg", "install"]
        if skip_dependencies:
            cmd.append("--skip-dependencies")
        cmd.extend(["--platform", platform])

        success = run_command(cmd, f"Installing platform {platform}")

        if success:
            return True, f"Platform {platform} installed successfully"
        else:
            # Non-fatal - platform might already be installed
            return True, "Platform installation completed (may already exist)"

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        error_msg = f"Failed to install platform {platform}: {e}"
        logger.error(error_msg)
        return False, error_msg


def install_platform_packages(
    platform: str, custom_packages: Optional[list[str]] = None
) -> tuple[bool, str]:
    """
    Install platform-specific packages (toolchains, compilers, etc.).

    Args:
        platform: Platform name or URL
        custom_packages: List of additional packages to install

    Returns:
        Tuple of (success: bool, message: str)
    """
    logger.info(f"Installing platform packages for: {platform}")

    try:
        # Install platform with dependencies (toolchains, etc.)
        cmd = ["pio", "pkg", "install", "--platform", platform]
        success = run_command(cmd, f"Installing platform packages for {platform}")

        if not success:
            logger.warning("Platform package installation completed with warnings")

        # Install custom packages if provided
        if custom_packages:
            logger.info(f"Installing custom packages: {custom_packages}")
            for pkg in custom_packages:
                pkg = pkg.strip()
                if not pkg:
                    continue

                logger.info(f"Installing package: {pkg}")
                cmd = ["pio", "pkg", "install", "-g", "--platform", pkg]
                if not run_command(cmd, f"Installing package {pkg}"):
                    logger.warning(
                        f"Failed to install package {pkg} (may already exist)"
                    )

        return True, "Platform packages installed successfully"

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        error_msg = f"Failed to install platform packages: {e}"
        logger.error(error_msg)
        return False, error_msg


def compile_board_example(
    board: str,
    example: str = "Blink",
    parallel: bool = False,
    project_root: str = "/fastled",
) -> tuple[bool, str]:
    """
    Compile a board with a specific example using bash compile script.

    Args:
        board: Board name (e.g., "uno", "esp32dev")
        example: Example name (default: "Blink")
        parallel: Whether to run in parallel mode
        project_root: Root directory of FastLED project

    Returns:
        Tuple of (success: bool, message: str)
    """
    logger.info(f"Compiling {board} with example: {example}")

    try:
        # Build compile command
        cmd: list[str] = []

        # Add NO_PARALLEL env var if not parallel
        if not parallel:
            cmd.extend(["env", "NO_PARALLEL=1"])

        # Run bash compile script
        cmd.extend(["bash", "compile", board, example])

        success = run_command(cmd, f"Compiling {board} with {example}")

        if success:
            return True, f"Successfully compiled {board} with {example}"
        else:
            return False, f"Failed to compile {board} with {example}"

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        error_msg = f"Error during compilation: {e}"
        logger.error(error_msg)
        return False, error_msg


def parse_board_list(boards_str: str) -> list[str]:
    """
    Parse comma-delimited board list into individual boards.

    Args:
        boards_str: Comma-separated board names (e.g., "uno,esp32dev,teensy41")

    Returns:
        List of trimmed board names
    """
    return [board.strip() for board in boards_str.split(",") if board.strip()]


def log_compilation_summary(failed_boards: list[str], total_boards: int) -> None:
    """
    Log a summary of compilation results.

    Args:
        failed_boards: List of boards that failed to compile
        total_boards: Total number of boards attempted
    """
    if not failed_boards:
        logger.info(f"✓ All {total_boards} board(s) compiled successfully")
        return

    logger.warning(f"⚠ Summary: {len(failed_boards)}/{total_boards} board(s) failed:")
    for board in failed_boards:
        logger.warning(f"  ✗ {board}")
    logger.info(f"✓ {total_boards - len(failed_boards)} board(s) compiled successfully")


if __name__ == "__main__":
    # Example usage / testing
    import argparse

    parser = argparse.ArgumentParser(description="FastLED Docker Build Utilities")
    parser.add_argument("--test", action="store_true", help="Run basic tests")

    args = parser.parse_args()

    if args.test:
        logger.info("Running basic tests...")
        logger.info(f"Module loaded successfully from: {__file__}")
