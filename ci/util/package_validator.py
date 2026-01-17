from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
Package Validator for PlatformIO Packages

Validates that PlatformIO packages are correctly installed and not corrupted.
Detects missing files, corrupted package.json, and incomplete installations.
"""

import json
import re
import sys
from pathlib import Path


def validate_package(package_dir: Path) -> tuple[bool, str]:
    """Validate that package is correctly installed using exception-style recovery.

    This uses exception-based validation - we only fail if we encounter actual
    exceptions when reading package metadata. No arbitrary prechecks (like file
    counts) are performed, as these cause false positives with wrapper packages.

    Args:
        package_dir: Path to package directory

    Returns:
        (is_valid, error_message)
    """
    try:
        # Try to read package.json - this is the critical test
        # If we can successfully parse it, the package is usable
        package_json = package_dir / "package.json"

        with open(package_json, "r") as f:
            data = json.load(f)

        # Verify essential fields are present
        required_fields = ["name", "version"]
        for field in required_fields:
            if field not in data:
                return False, f"Missing '{field}' in {package_dir.name}/package.json"

        # Success - package.json is readable and valid
        return True, ""

    except FileNotFoundError:
        return False, f"Missing package.json in {package_dir.name}"
    except json.JSONDecodeError as e:
        return False, f"Corrupted package.json in {package_dir.name}: {e}"
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        return False, f"Cannot read package.json in {package_dir.name}: {e}"


def get_required_packages(project_dir: Path, environment: str) -> list[str]:
    """Extract required packages from platformio.ini for environment.

    Args:
        project_dir: PlatformIO project directory
        environment: Environment name

    Returns:
        List of required package directory names (e.g., ["framework-arduinoespressif32@3.20014.231124"])
    """
    platformio_ini = project_dir / "platformio.ini"
    if not platformio_ini.exists():
        return []

    # Parse platformio.ini (simple regex-based approach)
    # This is not a full INI parser but works for our needs
    try:
        content = platformio_ini.read_text()

        # Find environment section
        env_section_pattern = rf"\[env:{re.escape(environment)}\]"
        env_match = re.search(env_section_pattern, content)
        if not env_match:
            return []

        # Extract platform from environment section
        # Format: platform = espressif32
        platform_pattern = r"platform\s*=\s*(\S+)"
        platform_match = re.search(platform_pattern, content[env_match.start() :])

        if not platform_match:
            return []

        platform = platform_match.group(1)

        # Map platform to package names (heuristic-based)
        # PlatformIO packages follow patterns like:
        # - toolchain-<platform>
        # - framework-<platform>
        # - tool-<name>
        package_prefixes: list[str] = []

        if "espressif32" in platform:
            package_prefixes.extend(
                [
                    "framework-arduinoespressif32",
                    "toolchain-xtensa-esp32",
                    "tool-esptoolpy",
                ]
            )
        elif "espressif8266" in platform:
            package_prefixes.extend(
                [
                    "framework-arduinoespressif8266",
                    "toolchain-xtensa",
                    "tool-esptoolpy",
                ]
            )
        elif "atmelavr" in platform:
            package_prefixes.extend(
                [
                    "framework-arduino-avr",
                    "toolchain-atmelavr",
                ]
            )
        elif "teensy" in platform:
            package_prefixes.extend(
                [
                    "framework-arduinoteensy",
                    "toolchain-gccarmnoneeabi",
                ]
            )
        # Add more platform mappings as needed

        # Find actual package directories matching prefixes
        pio_packages_dir = Path.home() / ".platformio" / "packages"
        if not pio_packages_dir.exists():
            return []

        packages: list[str] = []
        for pkg_dir in pio_packages_dir.iterdir():
            if pkg_dir.is_dir():
                for prefix in package_prefixes:
                    if pkg_dir.name.startswith(prefix):
                        packages.append(pkg_dir.name)
                        break

        return packages

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception:
        # If parsing fails, return empty list (fast-path check will trigger)
        return []


def check_all_packages(project_dir: Path, environment: str) -> tuple[bool, list[str]]:
    """Check all packages for environment are valid.

    Args:
        project_dir: PlatformIO project directory
        environment: Environment name

    Returns:
        (all_valid, list_of_errors)
    """
    errors: list[str] = []

    # Get list of required packages
    required_packages = get_required_packages(project_dir, environment)

    if not required_packages:
        # Could not determine packages - skip validation
        # This will fall through to the fast-path check in client
        return True, []

    pio_packages_dir = Path.home() / ".platformio" / "packages"

    for package in required_packages:
        package_dir = pio_packages_dir / package
        if not package_dir.exists():
            errors.append(f"Missing package: {package}")
            continue

        is_valid, error_msg = validate_package(package_dir)
        if not is_valid:
            errors.append(error_msg)

    return len(errors) == 0, errors


def main() -> int:
    """Command-line interface for package validator."""
    import argparse

    parser = argparse.ArgumentParser(description="Validate PlatformIO packages")
    parser.add_argument(
        "--package-dir",
        type=Path,
        help="Validate specific package directory",
    )
    parser.add_argument(
        "--project-dir",
        type=Path,
        help="PlatformIO project directory",
    )
    parser.add_argument(
        "--environment",
        help="PlatformIO environment to check",
    )

    args = parser.parse_args()

    if args.package_dir:
        is_valid, error_msg = validate_package(args.package_dir)
        if is_valid:
            print(f"✅ Package {args.package_dir.name} is valid")
            return 0
        else:
            print(f"❌ {error_msg}")
            return 1

    if args.project_dir and args.environment:
        is_valid, errors = check_all_packages(args.project_dir, args.environment)
        if is_valid:
            print(f"✅ All packages for {args.environment} are valid")
            return 0
        else:
            print(f"❌ Found {len(errors)} package errors:")
            for error in errors:
                print(f"  - {error}")
            return 1

    parser.print_help()
    return 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
        print("\nInterrupted by user")
        sys.exit(130)
