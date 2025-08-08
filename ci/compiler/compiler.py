#!/usr/bin/env python3
"""
Abstract Base Class for FastLED Compilers

Defines the interface that all FastLED compiler implementations must follow.
"""

from abc import ABC, abstractmethod
from concurrent.futures import Future
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Any


class CacheType(Enum):
    """Compiler cache type options."""

    NO_CACHE = "no_cache"
    SCCACHE = "sccache"


@dataclass
class CompilerResult:
    """Base result class for compiler operations."""

    success: bool
    output: str
    build_dir: Path


@dataclass
class InitResult(CompilerResult):
    """Result from compiler initialization."""

    @property
    def platformio_ini(self) -> Path:
        return self.build_dir / "platformio.ini"


@dataclass
class SketchResult(CompilerResult):
    """Result from sketch compilation."""

    example: str


class Compiler(ABC):
    """Abstract base class defining the interface for FastLED compilers."""

    def __init__(self) -> None:
        """Initialize the compiler."""
        pass

    @abstractmethod
    def build(self, examples: list[str]) -> list[Future[SketchResult]]:
        """Build a list of examples with proper resource management.

        Args:
            examples: List of example names or paths to compile

        Returns:
            List of Future objects containing SketchResult for each example
        """
        pass

    @abstractmethod
    def clean(self) -> None:
        """Clean build artifacts for this platform."""
        pass

    @abstractmethod
    def clean_all(self) -> None:
        """Clean all build artifacts (local and global) for this platform."""
        pass

    @abstractmethod
    def deploy(
        self, example: str, upload_port: str | None = None, monitor: bool = False
    ) -> SketchResult:
        """Deploy (upload) a specific example to the target device.

        Args:
            example: Name of the example to deploy
            upload_port: Optional specific port for upload
            monitor: If True, attach to device monitor after successful upload

        Returns:
            SketchResult indicating success/failure of deployment
        """
        pass

    @abstractmethod
    def cancel_all(self) -> None:
        """Cancel all currently running builds."""
        pass

    @abstractmethod
    def check_usb_permissions(self) -> tuple[bool, str]:
        """Check if USB device access is properly configured.

        Returns:
            Tuple of (has_access, status_message)
        """
        pass

    @abstractmethod
    def install_usb_permissions(self) -> bool:
        """Install platform-specific USB permissions or equivalent.

        Returns:
            True if installation succeeded, False otherwise
        """
        pass

    @abstractmethod
    def get_cache_stats(self) -> str:
        """Get compiler statistics as a formatted string.

        This can include cache statistics, build metrics, performance data,
        or any other relevant compiler statistics.

        Returns:
            Formatted string containing compiler statistics, or empty string if none available
        """
        pass
