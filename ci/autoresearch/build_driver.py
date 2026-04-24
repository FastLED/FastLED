"""Build driver abstraction for autoresearch — fbuild default with legacy PlatformIO."""

from __future__ import annotations

import subprocess
from pathlib import Path
from typing import IO, Protocol, runtime_checkable


@runtime_checkable
class BuildDriver(Protocol):
    """Abstract build system driver for compile + deploy.

    Two implementations:
      - FbuildDriver (default for all board builds)
      - PlatformIODriver (deprecated legacy implementation)
    """

    @property
    def name(self) -> str: ...

    def install_packages(
        self,
        build_dir: Path,
        environment: str | None,
        timeout: float = 1800,
    ) -> bool: ...

    def deploy(
        self,
        build_dir: Path,
        environment: str | None,
        upload_port: str | None,
        verbose: bool = False,
        clean: bool = False,
        quiet: bool = False,
        log_file: IO[str] | None = None,
    ) -> bool: ...

    def firmware_path(self, build_dir: Path, environment: str) -> Path: ...


class FbuildDriver:
    """Build driver using fbuild (Rust-based daemon, combined build+flash)."""

    @property
    def name(self) -> str:
        return "fbuild"

    def install_packages(
        self,
        build_dir: Path,
        environment: str | None,
        timeout: float = 1800,
    ) -> bool:
        from ci.util.pio_package_client import ensure_packages_installed

        return ensure_packages_installed(build_dir, environment, timeout=int(timeout))

    def deploy(
        self,
        build_dir: Path,
        environment: str | None,
        upload_port: str | None,
        verbose: bool = False,
        clean: bool = False,
        quiet: bool = False,
        log_file: IO[str] | None = None,
    ) -> bool:
        from ci.util.fbuild_runner import run_fbuild_deploy

        return run_fbuild_deploy(
            build_dir,
            environment=environment,
            upload_port=upload_port,
            verbose=verbose,
            clean=clean,
            quiet=quiet,
            log_file=log_file,
        )

    def firmware_path(self, build_dir: Path, environment: str) -> Path:
        return build_dir / ".fbuild" / "build" / environment / "firmware.bin"


class PlatformIODriver:
    """Deprecated PlatformIO build driver kept for legacy callers.

    New board-build paths should use FbuildDriver. This compatibility shim is
    retained only for code that still imports PlatformIODriver directly.
    """

    @property
    def name(self) -> str:
        return "platformio"

    def install_packages(
        self,
        build_dir: Path,
        environment: str | None,
        timeout: float = 1800,
    ) -> bool:
        from ci.compiler.build_utils import get_utf8_env

        cmd: list[str] = ["pio", "pkg", "install", "--project-dir", str(build_dir)]
        if environment:
            cmd.extend(["--environment", environment])

        print("=" * 60)
        print("PACKAGE INSTALLATION (PlatformIO)")
        print("=" * 60)
        result = subprocess.run(cmd, env=get_utf8_env())
        if result.returncode != 0:
            print("\n... Package installation failed")
            return False
        print("... Package installation completed\n")
        return True

    def deploy(
        self,
        build_dir: Path,
        environment: str | None,
        upload_port: str | None,
        verbose: bool = False,
        clean: bool = False,
        quiet: bool = False,
        log_file: IO[str] | None = None,
    ) -> bool:
        from ci.debug_attached import run_compile, run_upload
        from ci.util.port_utils import kill_port_users

        if not run_compile(build_dir, environment, verbose, clean=clean):
            return False
        if upload_port:
            kill_port_users(upload_port)
        if not run_upload(build_dir, environment, upload_port, verbose):
            return False
        return True

    def firmware_path(self, build_dir: Path, environment: str) -> Path:
        return build_dir / ".pio" / "build" / environment / "firmware.bin"


def select_build_driver(
    _environment: str | None,
    _use_fbuild_flag: bool,
    _no_fbuild_flag: bool,
) -> BuildDriver:
    """Select the appropriate build driver.

    All board builds use fbuild. The fbuild selection flags are kept only for
    command compatibility and do not change this behavior.

    Args:
        _environment: Deprecated compatibility parameter; ignored.
        _use_fbuild_flag: Deprecated compatibility parameter; ignored.
        _no_fbuild_flag: Deprecated compatibility parameter; ignored.
    """
    return FbuildDriver()
