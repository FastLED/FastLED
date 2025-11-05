"""Path management for FastLED PlatformIO builds."""

from pathlib import Path
from typing import Optional


def resolve_project_root() -> Path:
    """Resolve the FastLED project root directory."""
    current = Path(
        __file__
    ).parent.parent.parent.resolve()  # Go up from ci/compiler/path_manager.py
    while current != current.parent:
        if (current / "src" / "FastLED.h").exists():
            return current
        current = current.parent
    raise RuntimeError("Could not find FastLED project root")


class FastLEDPaths:
    """Centralized path management for FastLED board-specific directories and files."""

    def __init__(self, board_name: str, project_root: Optional[Path] = None) -> None:
        self.board_name = board_name
        self.project_root = project_root or resolve_project_root()
        self.home_dir = Path.home()

        # Base FastLED directory
        self.fastled_root = self.home_dir / ".fastled"
        # Initialize the optional cache directory override
        self._global_platformio_cache_dir: Optional[Path] = None

    @property
    def build_dir(self) -> Path:
        """Project-local build directory for this board."""
        return self.project_root / ".build" / "pio" / self.board_name

    @property
    def build_cache_dir(self) -> Path:
        """Project-local build cache directory for this board."""
        return self.build_dir / "build_cache"

    @property
    def platform_lock_file(self) -> Path:
        """Platform-specific build lock file."""
        return self.build_dir.parent / f"{self.board_name}.lock"

    @property
    def global_package_lock_file(self) -> Path:
        """Global package installation lock file."""
        packages_lock_root = self.fastled_root / "pio" / "packages"
        return packages_lock_root / f"{self.board_name}_global.lock"

    @property
    def core_dir(self) -> Path:
        """PlatformIO core directory (build cache, platforms)."""
        return self.fastled_root / "compile" / "pio" / self.board_name

    @property
    def packages_dir(self) -> Path:
        """PlatformIO packages directory (toolchains, frameworks)."""
        return self.home_dir / ".platformio" / "packages"

    @property
    def global_platformio_cache_dir(self) -> Path:
        """Global PlatformIO package cache directory (shared across all boards)."""
        if self._global_platformio_cache_dir is not None:
            return self._global_platformio_cache_dir
        return self.fastled_root / "platformio_cache"

    def ensure_directories_exist(self) -> None:
        """Create all necessary directories."""
        self.build_dir.mkdir(parents=True, exist_ok=True)
        self.global_package_lock_file.parent.mkdir(parents=True, exist_ok=True)
        self.core_dir.mkdir(parents=True, exist_ok=True)
        self.packages_dir.mkdir(parents=True, exist_ok=True)
        self.global_platformio_cache_dir.mkdir(parents=True, exist_ok=True)
