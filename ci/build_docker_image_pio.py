from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


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
import os
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Optional

# Import board configuration
from ci.boards import create_board
from ci.docker_utils.build_image import generate_config_hash
from ci.util.docker_command import get_docker_command
from ci.util.docker_helper import (
    attempt_start_docker,
    is_docker_available,
)


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

    parser.add_argument(
        "--build",
        action="store_true",
        help="Force rebuild of base and target images (uses cache unless --no-cache specified)",
    )

    parser.add_argument(
        "--tag",
        type=str,
        default=None,
        help="Docker image tag (default: auto-generated from platform, e.g., 'idf5.4', 'latest')",
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
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
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
    Load Dockerfile template from ci/docker_utils/Dockerfile.template.

    Returns:
        String content of Dockerfile template

    Raises:
        FileNotFoundError: If template file is not found
    """
    template_path = Path(__file__).parent / "docker_utils" / "Dockerfile.template"

    if not template_path.exists():
        raise FileNotFoundError(
            f"Dockerfile template not found: {template_path}\n"
            "Expected location: ci/docker_utils/Dockerfile.template"
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
            [get_docker_command(), "image", "inspect", image_name],
            capture_output=True,
            text=True,
            check=False,
        )
        return result.returncode == 0
    except FileNotFoundError:
        print("Error: Docker is not installed or not in PATH", file=sys.stderr)
        raise


def build_base_image(no_cache: bool = False, force_rebuild: bool = False) -> None:
    """
    Build the fastled-compiler-base base image if it doesn't exist.

    Args:
        no_cache: Whether to disable Docker cache
        force_rebuild: Whether to force rebuild even if image exists

    Raises:
        subprocess.CalledProcessError: If docker build fails
    """
    base_image_name = "niteris/fastled-compiler-base:latest"

    # Check if base image exists
    if (
        check_docker_image_exists(base_image_name)
        and not no_cache
        and not force_rebuild
    ):
        print(f"Base image {base_image_name} already exists, skipping build")
        print()
        return

    print(f"Building base image: {base_image_name}")
    print()

    # Ensure Docker is running before attempting to build
    if not is_docker_available():
        print("⚠️  Docker is not running. Attempting to start Docker Desktop...")
        print()
        success, message = attempt_start_docker()

        if success:
            print(f"✓ {message}")
            print()
        else:
            print(f"❌ Failed to start Docker: {message}")
            print()
            print("Please start Docker Desktop manually and try again.")
            print(
                "Download Docker Desktop: https://www.docker.com/products/docker-desktop"
            )
            raise RuntimeError(
                f"Docker is not available and could not be started automatically. {message}"
            )

    # Find the base Dockerfile and project root
    base_dockerfile_path = Path(__file__).parent / "docker_utils" / "Dockerfile.base"
    project_root = Path(__file__).parent.parent

    if not base_dockerfile_path.exists():
        raise FileNotFoundError(
            f"Base Dockerfile not found: {base_dockerfile_path}\n"
            "Expected location: ci/docker_utils/Dockerfile.base"
        )

    # Create temporary directory for base image build context
    import tempfile

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        # Copy Dockerfile.base to temp directory
        import shutil

        # Read the Dockerfile and remove syntax header for Windows compatibility
        with open(base_dockerfile_path, "r") as f:
            dockerfile_content = f.read()

        # Remove the syntax header if on Windows (Docker Desktop has issues with it)
        if sys.platform == "win32" and dockerfile_content.startswith("# syntax="):
            lines = dockerfile_content.split("\n")
            # Remove first line if it's the syntax header
            if lines[0].startswith("# syntax="):
                dockerfile_content = "\n".join(lines[1:])

        with open(temp_path / "Dockerfile", "w") as f:
            f.write(dockerfile_content)

        # Copy pyproject.toml and uv.lock for Python dependency installation
        pyproject_path = project_root / "pyproject.toml"
        if pyproject_path.exists():
            shutil.copy(pyproject_path, temp_path / "pyproject.toml")

        uv_lock_path = project_root / "uv.lock"
        if uv_lock_path.exists():
            shutil.copy(uv_lock_path, temp_path / "uv.lock")

        # Build docker command
        cmd = [get_docker_command(), "build"]

        if no_cache:
            cmd.append("--no-cache")

        cmd.extend(["-t", base_image_name, str(temp_path)])

        print(f"Command: {' '.join(cmd)}")
        print()

        # Run docker build with BuildKit enabled for cache mounts
        # On Windows, disable BuildKit if it causes credential issues
        env = os.environ.copy()
        if sys.platform == "win32":
            # Disable BuildKit on Windows to avoid credential-related issues
            env["DOCKER_BUILDKIT"] = "0"
        else:
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
            handle_keyboard_interrupt_properly()
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
    # Read the Dockerfile and remove syntax header for Windows compatibility
    with open(dockerfile_path, "r") as f:
        dockerfile_content = f.read()

    # Remove the syntax header if on Windows (Docker Desktop has issues with it)
    if sys.platform == "win32" and dockerfile_content.startswith("# syntax="):
        lines = dockerfile_content.split("\n")
        # Remove first line if it's the syntax header
        if lines[0].startswith("# syntax="):
            dockerfile_content = "\n".join(lines[1:])

        # Rewrite the Dockerfile without the syntax header
        with open(dockerfile_path, "w") as f:
            f.write(dockerfile_content)

    # Build docker command
    cmd = [get_docker_command(), "build"]

    if no_cache:
        cmd.append("--no-cache")

    # Add build arguments for platform name (used during dependency caching)
    # The Dockerfile template expects PLATFORMS (plural) argument
    cmd.extend(["--build-arg", f"PLATFORMS={platform_name}"])

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
    # On Windows, disable BuildKit if it causes credential issues
    env = os.environ.copy()
    if sys.platform == "win32":
        # Disable BuildKit on Windows to avoid credential-related issues
        env["DOCKER_BUILDKIT"] = "0"
    else:
        env["DOCKER_BUILDKIT"] = "1"

    try:
        subprocess.run(cmd, check=True, env=env)
        print()
        print(f"Successfully built Docker image: {image_name}")
    except subprocess.CalledProcessError as e:
        print(f"Error building Docker image: {e}", file=sys.stderr)
        raise
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        print("\n\nBuild interrupted by user (Ctrl+C)", file=sys.stderr)
        raise


def main() -> int:
    """Main entry point for the script."""
    try:
        return _main_impl()
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
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

        # Use board name directly - no architecture mapping needed
        architecture = platform_name

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
                # Use board name directly - no architecture mapping needed
                architecture = platform_name
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
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"Warning: Could not generate config hash: {e}", file=sys.stderr)
            config_hash = "custom"

    # Generate image name if not provided
    if args.image_name:
        # User provided custom name - use as-is
        image_name = args.image_name
    else:
        # Try to use platform mapping first (preferred for grouped/flat platforms)
        from ci.docker_utils.build_image import generate_docker_tag
        from ci.docker_utils.build_platforms import (
            get_docker_image_name,
            get_platform_for_board,
        )

        try:
            # Check if board is in platform mapping
            platform = get_platform_for_board(platform_name)

            if platform:
                # Use platform-based naming (e.g., niteris/fastled-compiler-esp-32s3)
                # This handles both grouped platforms (avr, teensy) and flat platforms (esp-32s3)
                tag = args.tag if args.tag else generate_docker_tag(platform_name)
                image_name = f"{get_docker_image_name(platform)}:{tag}"
            else:
                # Fallback to architecture-based naming for unmapped boards
                from ci.docker_utils.build_image import extract_architecture

                arch = extract_architecture(platform_name)
                tag = args.tag if args.tag else generate_docker_tag(platform_name)
                image_name = f"fastled-compiler-{arch}-{platform_name}:{tag}"

        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            # Final fallback if all lookups fail
            print(f"Warning: Could not generate semantic name: {e}", file=sys.stderr)
            tag = args.tag if args.tag else "latest"
            image_name = f"fastled-compiler-{platform_name}:{tag}"

    print(f"Image name: {image_name}")
    if config_hash and not args.image_name:
        print(f"Config hash: {config_hash} (for reference)")
    print()

    # Build base image first if it doesn't exist
    try:
        force_rebuild = hasattr(args, "build") and args.build
        build_base_image(args.no_cache, force_rebuild=force_rebuild)
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

        # Copy ci/ directory structure to temp directory
        # The Dockerfile expects ci/docker_utils/build.sh and ci/ for Python imports
        import shutil

        # Create ci/ directory structure in temp directory
        ci_temp_dir = temp_path / "ci"
        ci_temp_dir.mkdir(exist_ok=True)

        # Copy entire ci directory to maintain structure
        ci_src_dir = Path(__file__).parent

        # Copy all necessary files from ci/ to temp/ci/
        # Include Python files, JSON config files, shell scripts, and other build files
        file_patterns = ["**/*.py", "**/*.json", "**/*.sh", "**/*.toml", "**/*.ini"]
        for pattern in file_patterns:
            for item in ci_src_dir.glob(pattern):
                # Skip __pycache__ directories and .pyc files
                if "__pycache__" in str(item) or item.suffix == ".pyc":
                    continue

                relative_path = item.relative_to(ci_src_dir)
                dest_path = ci_temp_dir / relative_path
                dest_path.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy(item, dest_path)

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
    print("  docker run --rm \\")
    print("    -v $(pwd)/src:/fastled/src:ro \\")
    print("    -v $(pwd)/examples:/fastled/examples:ro \\")
    print(f"    {image_name} \\")
    print("    'pio run'")
    print("=" * 70)

    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
        # Double interrupt or interrupt during shutdown
        print("\n\nForced exit", file=sys.stderr)
        sys.exit(130)
