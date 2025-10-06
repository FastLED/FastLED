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
from ci.docker.build_image import extract_architecture, generate_config_hash


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

        # Run docker build with BuildKit enabled for cache mounts
        env = os.environ.copy()
        env["DOCKER_BUILDKIT"] = "1"
        try:
            subprocess.run(cmd, check=True, env=env)
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
        image_name: Name for the Docker image
        context_dir: Build context directory
        platform_name: Platform name for labeling and runtime generation of platformio.ini
        architecture: Architecture name for labeling
        no_cache: Whether to disable Docker cache

    Raises:
        subprocess.CalledProcessError: If docker build fails
    """
    # Build docker command
    cmd = ["docker", "build"]

    if no_cache:
        cmd.append("--no-cache")

    # Add build arguments for platform name (used during dependency caching)
    cmd.extend(["--build-arg", f"PLATFORM_NAME={platform_name}"])

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

    # Run docker build with BuildKit enabled for cache mounts
    env = os.environ.copy()
    env["DOCKER_BUILDKIT"] = "1"
    try:
        subprocess.run(cmd, check=True, env=env)
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

        # NOTE: We no longer write platformio.ini to the temp directory
        # The Dockerfile now generates it dynamically from PLATFORM_NAME using build.sh
        # This makes ci/boards.py the single source of truth

        print(
            "Platform configuration (will be generated in Docker from PLATFORM_NAME):"
        )
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

        # Copy entrypoint.sh and build.sh to temp directory
        import shutil

        entrypoint_src = Path(__file__).parent / "docker" / "entrypoint.sh"
        if entrypoint_src.exists():
            shutil.copy(entrypoint_src, temp_path / "entrypoint.sh")
        else:
            print(
                f"Warning: entrypoint.sh not found at {entrypoint_src}", file=sys.stderr
            )

        build_script_src = Path(__file__).parent / "docker" / "build.sh"
        if build_script_src.exists():
            shutil.copy(build_script_src, temp_path / "build.sh")
        else:
            print(f"Warning: build.sh not found at {build_script_src}", file=sys.stderr)

        print("Using Dockerfile template:")
        print("=" * 70)
        print(dockerfile_content)
        print("=" * 70)
        print()

        # Build Docker image
        try:
            build_docker_image(
                str(dockerfile_path),
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
