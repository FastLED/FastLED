from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


"""Toolchain abstraction for multi-platform builds.

This module provides abstract interfaces and concrete implementations for
different compilation toolchains (Emscripten, native clang, etc.).

The Toolchain interface abstracts away toolchain-specific details like:
- Finding compiler/archiver executables
- Loading build flags from configuration
- Getting compiler versions
- Platform-specific include paths

This enables reusing build orchestration logic (PCH, incremental compilation,
parallel compilation) across different toolchains.
"""

import subprocess
import tomllib
from abc import ABC, abstractmethod
from pathlib import Path


class Toolchain(ABC):
    """Abstract toolchain interface for multi-platform builds.

    Subclasses must implement methods to locate tools, load build flags,
    and provide platform-specific configuration.
    """

    @abstractmethod
    def find_compiler(self) -> Path:
        """Find C++ compiler executable.

        Returns:
            Path to compiler executable (e.g., em++, clang++, g++)

        Raises:
            FileNotFoundError: If compiler cannot be found
        """
        pass

    @abstractmethod
    def find_archiver(self) -> Path:
        """Find static library archiver (ar/llvm-ar/emar).

        Returns:
            Path to archiver executable

        Raises:
            FileNotFoundError: If archiver cannot be found
        """
        pass

    @abstractmethod
    def get_compiler_version(self) -> str:
        """Get compiler version string for invalidation.

        Returns:
            Version string (e.g., "emcc 3.1.50")
        """
        pass

    @abstractmethod
    def load_build_flags(self, mode: str, target: str) -> dict[str, list[str]]:
        """Load build flags for given mode and target.

        Args:
            mode: Build mode (debug, fast_debug, quick, release)
            target: Target type (library, sketch, link)

        Returns:
            Dictionary with keys:
            - 'defines': List of -D defines
            - 'compiler_flags': List of compiler flags
            - 'link_flags': List of linker flags (may be empty for non-link targets)
        """
        pass

    @abstractmethod
    def get_include_paths(self) -> list[str]:
        """Get platform-specific include paths.

        Returns:
            List of include paths with -I prefix (e.g., ["-Isrc/platforms/wasm"])
        """
        pass


class EmscriptenToolchain(Toolchain):
    """Emscripten (WASM) toolchain implementation.

    This toolchain compiles FastLED to WebAssembly using emscripten's em++
    compiler and emar archiver.

    Configuration is loaded from: src/platforms/wasm/compiler/build_flags.toml
    """

    def __init__(self, project_root: Path | None = None):
        """Initialize Emscripten toolchain.

        Args:
            project_root: FastLED project root (defaults to auto-detect)
        """
        if project_root is None:
            # Auto-detect from this file's location (ci/compiler/toolchain.py)
            project_root = Path(__file__).parent.parent.parent
        self.project_root = project_root
        self.build_flags_toml = (
            project_root
            / "src"
            / "platforms"
            / "wasm"
            / "compiler"
            / "build_flags.toml"
        )

    def find_compiler(self) -> Path:
        """Find emscripten em++ compiler in clang-tool-chain."""
        home = Path.home()

        # Try Windows path
        emcc_path = (
            home
            / ".clang-tool-chain"
            / "emscripten"
            / "win"
            / "x86_64"
            / "emscripten"
            / "em++.bat"
        )
        if emcc_path.exists():
            return emcc_path

        # Try Linux path
        emcc_path = (
            home
            / ".clang-tool-chain"
            / "emscripten"
            / "linux"
            / "x86_64"
            / "emscripten"
            / "em++"
        )
        if emcc_path.exists():
            return emcc_path

        # Try macOS path
        emcc_path = (
            home
            / ".clang-tool-chain"
            / "emscripten"
            / "darwin"
            / "x86_64"
            / "emscripten"
            / "em++"
        )
        if emcc_path.exists():
            return emcc_path

        raise FileNotFoundError(
            "Could not find emscripten compiler in clang-tool-chain installation. "
            "Make sure clang-tool-chain is installed with emscripten support."
        )

    def find_archiver(self) -> Path:
        """Find emscripten emar archiver in clang-tool-chain."""
        home = Path.home()

        # Try Windows path
        emar_path = (
            home
            / ".clang-tool-chain"
            / "emscripten"
            / "win"
            / "x86_64"
            / "emscripten"
            / "emar.bat"
        )
        if emar_path.exists():
            return emar_path

        # Try Linux path
        emar_path = (
            home
            / ".clang-tool-chain"
            / "emscripten"
            / "linux"
            / "x86_64"
            / "emscripten"
            / "emar"
        )
        if emar_path.exists():
            return emar_path

        # Try macOS path
        emar_path = (
            home
            / ".clang-tool-chain"
            / "emscripten"
            / "darwin"
            / "x86_64"
            / "emscripten"
            / "emar"
        )
        if emar_path.exists():
            return emar_path

        raise FileNotFoundError(
            "Could not find emscripten archiver (emar) in clang-tool-chain installation. "
            "Make sure clang-tool-chain is installed with emscripten support."
        )

    def get_compiler_version(self) -> str:
        """Get emscripten compiler version string."""
        try:
            emcc = self.find_compiler()
            result = subprocess.run(
                [str(emcc), "--version"],
                capture_output=True,
                text=True,
                check=True,
                timeout=10,
            )
            # Return first line which contains version info
            return result.stdout.split("\n")[0].strip()
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"Warning: Could not get compiler version: {e}")
            return "unknown"

    def load_build_flags(self, mode: str, target: str) -> dict[str, list[str]]:
        """Load build flags from build_flags.toml.

        Args:
            mode: Build mode (debug, fast_debug, quick, release)
            target: Target type (library, sketch, link)

        Returns:
            Dictionary with 'defines', 'compiler_flags', 'link_flags' lists

        Raises:
            FileNotFoundError: If build_flags.toml doesn't exist
        """
        if not self.build_flags_toml.exists():
            raise FileNotFoundError(
                f"Build flags TOML not found: {self.build_flags_toml}"
            )

        with open(self.build_flags_toml, "rb") as f:
            config = tomllib.load(f)

        # Collect flags from [all] section (used by everything)
        defines = config.get("all", {}).get("defines", []).copy()
        compiler_flags = config.get("all", {}).get("compiler_flags", []).copy()

        # Add target-specific flags
        if target == "library":
            defines.extend(config.get("library", {}).get("defines", []))
            compiler_flags.extend(config.get("library", {}).get("compiler_flags", []))
        elif target == "sketch":
            defines.extend(config.get("sketch", {}).get("defines", []))
            compiler_flags.extend(config.get("sketch", {}).get("compiler_flags", []))

        # Add build mode-specific flags
        build_mode_config = config.get("build_modes", {}).get(mode, {})
        compiler_flags.extend(build_mode_config.get("flags", []))

        # Get linking flags (used only during linking phase)
        link_flags: list[str] = []
        if target == "link":
            link_flags = (
                config.get("linking", {}).get("base", {}).get("flags", []).copy()
            )
            link_flags.extend(
                config.get("linking", {}).get("sketch", {}).get("flags", [])
            )
            link_flags.extend(build_mode_config.get("link_flags", []))

        return {
            "defines": defines,
            "compiler_flags": compiler_flags,
            "link_flags": link_flags,
        }

    def get_include_paths(self) -> list[str]:
        """Get WASM-specific include paths.

        Returns:
            List of include paths for WASM platform
        """
        return [
            "-Isrc",
            "-Isrc/platforms/wasm",
            "-Isrc/platforms/wasm/compiler",
        ]


class NativeToolchain(Toolchain):
    """Native clang-tool-chain implementation (for future use).

    This toolchain would compile FastLED natively using clang++ from
    the clang-tool-chain, potentially replacing or complementing Meson
    for native builds.

    Note: Not yet implemented - placeholder for future extension.
    """

    def find_compiler(self) -> Path:
        """Find clang++ compiler in clang-tool-chain."""
        raise NotImplementedError("NativeToolchain not yet implemented")

    def find_archiver(self) -> Path:
        """Find llvm-ar archiver in clang-tool-chain."""
        raise NotImplementedError("NativeToolchain not yet implemented")

    def get_compiler_version(self) -> str:
        """Get clang compiler version string."""
        raise NotImplementedError("NativeToolchain not yet implemented")

    def load_build_flags(self, mode: str, target: str) -> dict[str, list[str]]:
        """Load build flags for native compilation."""
        raise NotImplementedError("NativeToolchain not yet implemented")

    def get_include_paths(self) -> list[str]:
        """Get native platform include paths."""
        raise NotImplementedError("NativeToolchain not yet implemented")
