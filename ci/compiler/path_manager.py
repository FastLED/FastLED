"""Path management for FastLED fbuild board builds."""

from pathlib import Path
from typing import Optional


# Directory under the project root that holds every board's fbuild project
# (``.build/fbuild/<board>``). This is the single definition; tooling that
# needs to find a board's build directory derives it from here.
BOARD_BUILD_ROOT = Path(".build") / "fbuild"


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


def board_build_dir(board_name: str, project_root: Optional[Path] = None) -> Path:
    """Project-local build directory for ``board_name``."""
    root = project_root or resolve_project_root()
    return root / BOARD_BUILD_ROOT / board_name


class FastLEDPaths:
    """Centralized path management for FastLED board-specific directories and files."""

    def __init__(self, board_name: str, project_root: Optional[Path] = None) -> None:
        self.board_name = board_name
        self.project_root = project_root or resolve_project_root()
        self.home_dir = Path.home()

        # Base FastLED directory
        self.fastled_root = self.home_dir / ".fastled"

    @property
    def build_dir(self) -> Path:
        """Project-local build directory for this board."""
        return board_build_dir(self.board_name, self.project_root)

    @property
    def platform_lock_file(self) -> Path:
        """Platform-specific build lock file."""
        return self.build_dir.parent / f"{self.board_name}.lock"

    def ensure_directories_exist(self) -> None:
        """Create all necessary directories."""
        self.build_dir.mkdir(parents=True, exist_ok=True)
