#!/usr/bin/env python3
"""
Build PlatformIO Docker images with pre-cached dependencies.

This script creates Docker images for specific PlatformIO platforms with
pre-downloaded and cached dependencies, dramatically speeding up compilation
times by eliminating the dependency download step.

Usage:
    # Mode A: Generate from board configuration
    uv run python ci/build_docker_image_pio.py --platform uno
    uv run python ci/build_docker_image_pio.py --platform esp32s3 --framework arduino

    # Mode B: Use existing platformio.ini
    uv run python ci/build_docker_image_pio.py --platformio-ini ./custom-config.ini

    # Optional: Custom image name and no-cache
    uv run python ci/build_docker_image_pio.py --platform uno --image-name my-custom-image --no-cache
"""

import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Dict, List, Optional

# Import board configuration
from ci.boards import create_board


def parse_arguments() -> argparse.Namespace:
    """Parse and validate command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Build PlatformIO Docker images with pre-cached dependencies",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Mode A: Build from board configuration
  %(prog)s --platform uno
  %(prog)s --platform esp32s3 --framework arduino

  # Mode B: Build from existing platformio.ini
  %(prog)s --platformio-ini ./custom-config.ini

  # With custom image name and no cache
  %(prog)s --platform uno --image-name my-fastled-uno --no-cache
        """,
    )

    # Create mutually exclusive group for input modes
    input_mode = parser.add_mutually_exclusive_group(required=True)

    # Mode A: Generate from board configuration
    input_mode.add_argument(
        "--platform",
        type=str,
        help="Platform name from ci/boards.py (e.g., uno, esp32s3)",
    )

    # Mode B: Use existing platformio.ini
    input_mode.add_argument(
        "--platformio-ini",
        type=str,
        metavar="PATH",
        help="Path to existing platformio.ini file",
    )

    # Optional args for Mode A only (framework override)
    parser.add_argument(
        "--framework", type=str, help="Framework override (only valid with --platform)"
    )

    # Common optional arguments
    parser.add_argument(
        "--image-name",
        type=str,
        help="Custom Docker image name (auto-generated if not specified)",
    )

    parser.add_argument(
        "--no-cache", action="store_true", help="Build Docker image without using cache"
    )

    args = parser.parse_args()

    # Validation: --framework requires --platform
    if args.framework and not args.platform:
        parser.error("--framework can only be used with --platform")

    return args


def get_platformio_version() -> str:
    """
    Get the installed PlatformIO version.

    Returns:
        Version string (e.g., '6.1.15')
    """
    try:
        result = subprocess.run(
            ["pio", "--version"],
            capture_output=True,
            text=True,
            check=True,
        )
        # Output format: "PlatformIO Core, version 6.1.15"
        version_line = result.stdout.strip()
        if "version" in version_line:
            return version_line.split("version")[-1].strip()
        return version_line
    except (subprocess.CalledProcessError, FileNotFoundError):
        # If PlatformIO is not installed, return a default version
        return "unknown"


def generate_config_hash(platform_name: str, framework: Optional[str] = None) -> str:
    """
    Generate deterministic hash from board configuration.

    The hash is based on:
    - platform: Platform name (e.g., 'espressif32')
    - framework: Framework name (e.g., 'arduino')
    - board: Board identifier (e.g., 'esp32-s3-devkitc-1')
    - platform_packages: Custom platform packages/URLs
    - platformio_version: PlatformIO version for reproducibility

    Args:
        platform_name: Platform/board name (e.g., 'uno', 'esp32s3')
        framework: Optional framework override

    Returns:
        8-character hash string
    """
    board = create_board(platform_name)
    if framework:
        board.framework = framework

    # Create deterministic hash from config
    config_data: Dict[str, Optional[str | List[str]]] = {
        "platform": board.platform,
        "framework": board.framework,
        "board": board.board_name,
        "platform_packages": sorted(board.platform_packages or [])
        if isinstance(board.platform_packages, list)
        else ([board.platform_packages] if board.platform_packages else []),
        # Include PlatformIO version for full reproducibility
        "platformio_version": get_platformio_version(),
    }

    config_json = json.dumps(config_data, sort_keys=True)
    return hashlib.sha256(config_json.encode()).hexdigest()[:8]


def generate_platformio_ini(platform_name: str, framework: Optional[str] = None) -> str:
    """
    Generate platformio.ini content from board configuration.

    Args:
        platform_name: Platform/board name (e.g., 'uno', 'esp32s3')
        framework: Optional framework override

    Returns:
        String content for platformio.ini file

    Raises:
        ValueError: If platform is not found in boards.py
    """
    try:
        board = create_board(platform_name)
    except Exception as e:
        raise ValueError(f"Failed to create board '{platform_name}': {e}") from e

    # Override framework if specified
    if framework:
        board.framework = framework

    # Generate platformio.ini with project root context
    project_root = str(Path(__file__).parent.parent.absolute())

    # Create platformio.ini content
    ini_content = board.to_platformio_ini(
        include_platformio_section=True,
        project_root=project_root,
        additional_libs=["FastLED"],
    )

    return ini_content


def load_dockerfile_template() -> str:
    """
    Load Dockerfile template from ci/docker/Dockerfile.template.

    Returns:
        String content of Dockerfile template

    Raises:
        FileNotFoundError: If template file is not found
    """
    template_path = Path(__file__).parent / "docker" / "Dockerfile.template"

    if not template_path.exists():
        raise FileNotFoundError(
            f"Dockerfile template not found: {template_path}\n"
            "Expected location: ci/docker/Dockerfile.template"
        )

    with open(template_path, "r") as f:
        return f.read()


def extract_architecture(platform_name: str) -> str:
    """
    Extract architecture name from platform name.

    Args:
        platform_name: Platform name (e.g., 'uno', 'esp32s3')

    Returns:
        Architecture string (e.g., 'avr', 'esp32')
    """
    # Common platform to architecture mappings
    arch_map = {
        "uno": "avr",
        "yun": "avr",
        "nano_every": "avr",
        "attiny85": "avr",
        "attiny88": "avr",
        "attiny4313": "avr",
        "ATtiny1604": "megaavr",
        "ATtiny1616": "megaavr",
        "due": "sam",
        "digix": "sam",
        "bluepill": "stm32",
        "blackpill": "stm32",
        "maple_mini": "stm32",
        "giga_r1": "stm32",
        "hy_tinystm103tb": "stm32",
        "uno_r4_wifi": "renesas",
        "rpipico": "rp2040",
        "rpipico2": "rp2040",
        "teensy30": "teensy",
        "teensy31": "teensy",
        "teensy40": "teensy",
        "teensy41": "teensy",
        "teensylc": "teensy",
        "nrf52840_dk": "nrf52",
        "xiaoblesense": "nrf52",
        "xiaoblesense_adafruit": "nrf52",
        "adafruit_feather_nrf52840_sense": "nrf52",
        "esp01": "esp8266",
        "mgm240": "efm32",
        "sparkfun_xrp_controller": "rp2040",
        "apollo3_red": "apollo3",
        "apollo3_thing_explorable": "apollo3",
        "native": "native",
    }

    # Check if platform name starts with known ESP32 variants
    if platform_name.startswith("esp32"):
        return "esp32"

    # Look up in mapping
    return arch_map.get(platform_name, platform_name.split("_")[0])


def check_docker_image_exists(image_name: str) -> bool:
    """
    Check if a Docker image exists locally.

    Args:
        image_name: Name of the Docker image to check

    Returns:
        True if image exists, False otherwise
    """
    try:
        result = subprocess.run(
            ["docker", "image", "inspect", image_name],
            capture_output=True,
            text=True,
            check=False,
        )
        return result.returncode == 0
    except FileNotFoundError:
        print("Error: Docker is not installed or not in PATH", file=sys.stderr)
        raise


def build_base_image(no_cache: bool = False) -> None:
    """
    Build the fastled-platformio base image if it doesn't exist.

    Args:
        no_cache: Whether to disable Docker cache

    Raises:
        subprocess.CalledProcessError: If docker build fails
    """
    base_image_name = "fastled-platformio:latest"

    # Check if base image exists
    if check_docker_image_exists(base_image_name) and not no_cache:
        print(f"Base image {base_image_name} already exists, skipping build")
        print()
        return

    print(f"Building base image: {base_image_name}")
    print()

    # Find the base Dockerfile and project root
    base_dockerfile_path = Path(__file__).parent / "docker" / "Dockerfile.base"
    project_root = Path(__file__).parent.parent

    if not base_dockerfile_path.exists():
        raise FileNotFoundError(
            f"Base Dockerfile not found: {base_dockerfile_path}\n"
            "Expected location: ci/docker/Dockerfile.base"
        )

    # Create temporary directory for base image build context
    import tempfile

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        # Copy Dockerfile.base to temp directory
        import shutil

        shutil.copy(base_dockerfile_path, temp_path / "Dockerfile")

        # Copy pyproject.toml and uv.lock for Python dependency installation
        pyproject_path = project_root / "pyproject.toml"
        if pyproject_path.exists():
            shutil.copy(pyproject_path, temp_path / "pyproject.toml")

        uv_lock_path = project_root / "uv.lock"
        if uv_lock_path.exists():
            shutil.copy(uv_lock_path, temp_path / "uv.lock")

        # Build docker command
        cmd = ["docker", "build"]

        if no_cache:
            cmd.append("--no-cache")

        cmd.extend(["-t", base_image_name, str(temp_path)])

        print(f"Command: {' '.join(cmd)}")
        print()

        # Run docker build
        try:
            subprocess.run(cmd, check=True)
            print()
            print(f"Successfully built base image: {base_image_name}")
            print()
        except subprocess.CalledProcessError as e:
            print(f"Error building base image: {e}", file=sys.stderr)
            raise
        except KeyboardInterrupt:
            print("\n\nBuild interrupted by user (Ctrl+C)", file=sys.stderr)
            raise


def build_docker_image(
    dockerfile_path: str,
    platformio_ini_path: str,
    image_name: str,
    context_dir: str,
    platform_name: str,
    architecture: str,
    no_cache: bool = False,
) -> None:
    """
    Build Docker image using docker build command.

    Args:
        dockerfile_path: Path to Dockerfile
        platformio_ini_path: Path to platformio.ini
        image_name: Name for the Docker image
        context_dir: Build context directory
        platform_name: Platform name for labeling
        architecture: Architecture name for labeling
        no_cache: Whether to disable Docker cache

    Raises:
        subprocess.CalledProcessError: If docker build fails
    """
    # Build docker command
    cmd = ["docker", "build"]

    if no_cache:
        cmd.append("--no-cache")

    # Add metadata labels
    cmd.extend(
        [
            "--label",
            f"platform={platform_name}",
            "--label",
            f"architecture={architecture}",
        ]
    )

    cmd.extend(["-t", image_name, "-f", dockerfile_path, context_dir])

    print(f"Building Docker image: {image_name}")
    print(f"Platform: {platform_name}")
    print(f"Architecture: {architecture}")
    print(f"Command: {' '.join(cmd)}")
    print()

    # Run docker build
    try:
        subprocess.run(cmd, check=True)
        print()
        print(f"Successfully built Docker image: {image_name}")
    except subprocess.CalledProcessError as e:
        print(f"Error building Docker image: {e}", file=sys.stderr)
        raise
    except KeyboardInterrupt:
        print("\n\nBuild interrupted by user (Ctrl+C)", file=sys.stderr)
        raise


def main() -> int:
    """Main entry point for the script."""
    try:
        return _main_impl()
    except KeyboardInterrupt:
        print("\n\nOperation cancelled by user (Ctrl+C)", file=sys.stderr)
        return 130  # Standard exit code for SIGINT


def _main_impl() -> int:
    """Main implementation - separated for cleaner interrupt handling."""
    args = parse_arguments()

    # Initialize variables that will be set in one of the branches
    platform_name: str
    architecture: str
    platformio_ini_content: str

    # Determine which mode we're in
    if args.platform:
        print(f"Mode A: Building from board configuration (platform={args.platform})")
        platform_name = args.platform

        # Generate platformio.ini from board configuration
        try:
            platformio_ini_content = generate_platformio_ini(
                args.platform, args.framework
            )
        except ValueError as e:
            print(f"Error: {e}", file=sys.stderr)
            return 1

        # Extract architecture
        architecture = extract_architecture(platform_name)

    elif args.platformio_ini:
        print(f"Mode B: Building from existing INI file ({args.platformio_ini})")

        # Read existing platformio.ini
        platformio_ini_path = Path(args.platformio_ini)
        if not platformio_ini_path.exists():
            print(
                f"Error: platformio.ini file not found: {args.platformio_ini}",
                file=sys.stderr,
            )
            return 1

        with open(platformio_ini_path, "r") as f:
            platformio_ini_content = f.read()

        # For Mode B, try to extract platform name from ini file
        # Simple heuristic: look for [env:xxx] section
        platform_name = "custom"
        architecture = "custom"
        for line in platformio_ini_content.split("\n"):
            if line.strip().startswith("[env:"):
                env_name = line.strip()[5:-1]  # Extract name between [env: and ]
                platform_name = env_name
                architecture = extract_architecture(platform_name)
                break
    else:
        # This should never happen due to argparse validation, but helps type checker
        print("Error: No input mode specified", file=sys.stderr)
        return 1

    # Generate config hash for cache invalidation
    config_hash = ""
    if args.platform:
        # Only generate hash for Mode A (board configuration)
        # Mode B uses custom ini, so hash would be unstable
        try:
            config_hash = generate_config_hash(platform_name, args.framework)
        except Exception as e:
            print(f"Warning: Could not generate config hash: {e}", file=sys.stderr)
            config_hash = "custom"

    # Generate image name if not provided
    if args.image_name:
        image_name = args.image_name
    else:
        if config_hash:
            image_name = (
                f"fastled-platformio-{architecture}-{platform_name}-{config_hash}"
            )
        else:
            image_name = f"fastled-platformio-{architecture}-{platform_name}"

    print(f"Image name: {image_name}")
    if config_hash and not args.image_name:
        print(f"Config hash: {config_hash}")
    print()

    # Build base image first if it doesn't exist
    try:
        build_base_image(args.no_cache)
    except (subprocess.CalledProcessError, FileNotFoundError):
        return 1

    # Create temporary directory for Docker build context
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        # Write platformio.ini to temp directory
        platformio_ini_path = temp_path / "platformio.ini"
        with open(platformio_ini_path, "w") as f:
            f.write(platformio_ini_content)

        print("Generated platformio.ini:")
        print("=" * 70)
        print(platformio_ini_content)
        print("=" * 70)
        print()

        # Load Dockerfile template
        try:
            dockerfile_content = load_dockerfile_template()
        except FileNotFoundError as e:
            print(f"Error: {e}", file=sys.stderr)
            return 1

        # Write Dockerfile to temp directory
        dockerfile_path = temp_path / "Dockerfile"
        with open(dockerfile_path, "w") as f:
            f.write(dockerfile_content)

        print("Using Dockerfile template:")
        print("=" * 70)
        print(dockerfile_content)
        print("=" * 70)
        print()

        # Build Docker image
        try:
            build_docker_image(
                str(dockerfile_path),
                str(platformio_ini_path),
                image_name,
                str(temp_path),
                platform_name,
                architecture,
                args.no_cache,
            )
        except subprocess.CalledProcessError:
            return 1

    print()
    print("=" * 70)
    print("Docker image built successfully!")
    print(f"Image name: {image_name}")
    print()
    print("Usage example:")
    print(f"  docker run --rm \\")
    print(f"    -v $(pwd)/src:/fastled/src:ro \\")
    print(f"    -v $(pwd)/examples:/fastled/examples:ro \\")
    print(f"    {image_name} \\")
    print(f"    'pio run'")
    print("=" * 70)

    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        # Double interrupt or interrupt during shutdown
        print("\n\nForced exit", file=sys.stderr)
        sys.exit(130)
