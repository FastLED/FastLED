"""Platform-specific utilities for compilation and linking."""

import platform
from typing import List


def get_platform_linker_args() -> List[str]:
    """Get platform-specific linker arguments for FastLED executables."""
    system = platform.system()
    if system == "Windows":
        # In this project, we use Zig toolchain which has GNU target
        # The compiler is hardcoded to "python -m ziglang c++" in create_fastled_compiler()
        # Zig toolchain uses GNU-style linking even on Windows
        return [
            "-static-libgcc",  # Static link GCC runtime
            "-static-libstdc++",  # Static link C++ runtime
            "-lpthread",  # Threading support
            "-lm",  # Math library
            # Windows system libraries linked automatically
        ]
    elif system == "Linux":
        return [
            "-pthread",  # Threading support
            "-lm",  # Math library
            "-ldl",  # Dynamic loading
            "-lrt",  # Real-time extensions
        ]
    elif system == "Darwin":  # macOS
        return [
            "-pthread",  # Threading support
            "-lm",  # Math library
            "-framework",
            "CoreFoundation",
            "-framework",
            "IOKit",
        ]
    else:
        return [
            "-pthread",  # Threading support
            "-lm",  # Math library
        ]


def get_executable_name(example_name: str) -> str:
    """Get platform-appropriate executable name."""
    if platform.system() == "Windows":
        return f"{example_name}.exe"
    else:
        return example_name
