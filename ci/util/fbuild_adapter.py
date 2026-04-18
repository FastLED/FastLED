"""
fbuild Adapter Module for FastLED CI.

This module provides a clean interface to the fbuild build system,
wrapping the fbuild DaemonConnection API for use in FastLED's validation
and debugging workflows.

The fbuild daemon provides faster builds by:
- Caching compiled objects across builds
- Managing toolchain installation automatically
- Running as a background process for quick operations

Key functions:
    - fbuild_build_and_upload: Build and upload firmware to device
    - fbuild_build_only: Build firmware without uploading
    - is_fbuild_available: Check if fbuild is available
    - get_fbuild_version: Get fbuild version string

For daemon management, use the fbuild 2.0 API:
    from fbuild import Daemon, connect_daemon

    Daemon.ensure_running()
    with connect_daemon(project_dir, environment) as conn:
        conn.build()
        conn.deploy(port=port)
"""

from pathlib import Path

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


def get_compile_commands(board: str, build_root: Path | None = None) -> Path | None:
    """Return the path to ``compile_commands.json`` for ``board``.

    Thin wrapper around :func:`ci.util.fbuild_compiledb.ensure_compile_commands`
    that serves as the public, stable entry point for static-analysis tools
    (``ci/ci-iwyu.py``, ``ci/ci-cppcheck.py``, future ``clang-tidy`` /
    ``clang-format-check`` front-ends) to obtain an fbuild-produced compile
    database without reaching into the implementation module directly.

    If fbuild has already emitted the database for this board, its existing
    path is returned. Otherwise fbuild is invoked with
    ``fbuild <project> build -e <board> --target compiledb`` to materialize it.

    Args:
        board: Canonical board / fbuild environment name (e.g. ``"esp32c2"``).
        build_root: Optional ``.build/`` directory. Defaults to
            ``<repo-root>/.build/`` — where fbuild writes its artifacts under
            ``.fbuild/build/<env>/``.

    Returns:
        Path to ``compile_commands.json`` on success, ``None`` if fbuild
        couldn't produce one (binary missing, build failure, etc.).
    """
    # Local import: keep this module importable even when fbuild isn't
    # installed, so callers that only need the identity-probe helpers
    # (``is_fbuild_available``) stay lightweight.
    from ci.util.fbuild_compiledb import ensure_compile_commands

    project_root = Path(__file__).resolve().parent.parent.parent
    if build_root is None:
        build_root = project_root / ".build"
    return ensure_compile_commands(project_root, build_root, board)


def is_fbuild_available() -> bool:
    """Check if fbuild is available in the environment.

    Returns:
        True if fbuild can be imported, False otherwise.
    """
    try:
        import fbuild  # noqa: F401

        return True
    except ImportError:
        return False


def get_fbuild_version() -> str | None:
    """Get the installed fbuild version.

    Returns:
        Version string if available, None otherwise.
    """
    try:
        import fbuild

        return fbuild.__version__
    except ImportError:
        return None


def fbuild_build_and_upload(
    project_dir: Path,
    environment: str,
    port: str | None = None,
    clean_build: bool = False,
    timeout: float = 1800,
) -> tuple[bool, str]:
    """Build and upload firmware using fbuild.

    This function uses the fbuild DaemonConnection to compile and upload firmware
    to an embedded device. It does NOT start monitoring - the caller
    should use the existing run_monitor function for that.

    Args:
        project_dir: Path to project directory containing platformio.ini
        environment: Build environment name (e.g., 'esp32c6')
        port: Serial port for upload (auto-detect if None)
        clean_build: Whether to perform a clean build
        timeout: Maximum wait time in seconds (default: 30 minutes)

    Returns:
        Tuple of (success: bool, message: str)
    """
    try:
        from fbuild import Daemon, connect_daemon
    except ImportError as e:
        return False, f"fbuild not available: {e}"

    try:
        Daemon.ensure_running()

        with connect_daemon(project_dir, environment) as conn:
            success: bool = conn.deploy(
                port=port,
                clean=clean_build,
                monitor_after=False,  # We'll handle monitoring separately via run_monitor
                timeout=timeout,
            )

        if success:
            return True, "Build and upload completed successfully"
        else:
            return False, "Build or upload failed"

    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        return False, f"fbuild error: {e}"


def fbuild_build_only(
    project_dir: Path,
    environment: str,
    clean_build: bool = False,
    verbose: bool = False,
    timeout: float = 1800,
) -> tuple[bool, str]:
    """Build firmware using fbuild without uploading.

    Args:
        project_dir: Path to project directory containing platformio.ini
        environment: Build environment name (e.g., 'esp32c6')
        clean_build: Whether to perform a clean build
        verbose: Enable verbose build output
        timeout: Maximum wait time in seconds (default: 30 minutes)

    Returns:
        Tuple of (success: bool, message: str)
    """
    try:
        from fbuild import Daemon, connect_daemon
    except ImportError as e:
        return False, f"fbuild not available: {e}"

    try:
        Daemon.ensure_running()

        with connect_daemon(project_dir, environment) as conn:
            success: bool = conn.build(
                clean=clean_build,
                verbose=verbose,
                timeout=timeout,
            )

        if success:
            return True, "Build completed successfully"
        else:
            return False, "Build failed"

    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        return False, f"fbuild error: {e}"
