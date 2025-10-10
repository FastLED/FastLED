"""Shared PCH (Precompiled Header) builder for both unit tests and examples.

This module provides unified PCH generation that serves both unit tests and examples,
eliminating duplicate PCH compilation and reducing build times.

Key features:
- Single shared PCH at .build/shared/fastled_shared_pch.hpp.pch
- Includes all headers needed by both unit tests and examples
- Test-specific headers (test.h) are included but won't affect examples
- Namespace directives included for test compatibility
"""

from pathlib import Path
from typing import Optional

from ci.util.paths import PROJECT_ROOT


def get_shared_pch_content() -> str:
    """Get the shared PCH header content for both unit tests and examples.

    This PCH includes:
    - Core test framework headers (test.h) - unused by examples but harmless
    - Core FastLED headers used by nearly all compilations
    - Common C++ standard library headers
    - Platform headers for stub environment
    - Commonly used FastLED components
    - Namespace directive for test compatibility

    Returns:
        String containing the PCH header content
    """
    return """// FastLED Shared PCH - Common headers for faster compilation
#pragma once

// Core test framework (only available in unit tests)
#if defined(FASTLED_UNIT_TEST) || defined(FASTLED_TESTING)
    #include "test.h"
#endif

// Core FastLED headers used in nearly all unit tests and examples
#include "FastLED.h"

// Common C++ standard library headers
#include <string>
#include <vector>
#include <stdio.h>
#include <cstdint>
#include <cmath>
#include <cassert>
#include <iostream>
#include <memory>

// Platform headers for stub environment
#include "platforms/stub/fastled_stub.h"

// Commonly used FastLED components
#include "lib8tion.h"
#include "colorutils.h"
#include "hsv2rgb.h"
#include "fl/math.h"
#include "fl/vector.h"

// Using namespace to match test files (examples won't be affected)
using namespace fl;
"""


def get_shared_pch_path() -> Path:
    """Get the path to the shared PCH file.

    Returns:
        Path to the shared PCH file at .build/shared/fastled_shared_pch.hpp.pch
    """
    shared_pch_dir = PROJECT_ROOT / ".build" / "shared"
    shared_pch_dir.mkdir(parents=True, exist_ok=True)
    return shared_pch_dir / "fastled_shared_pch.hpp.pch"


def should_use_shared_pch() -> bool:
    """Determine if shared PCH should be used.

    This can be extended later to support configuration-based decisions.
    For now, always returns True to enable shared PCH.

    Returns:
        True if shared PCH should be used, False otherwise
    """
    return True


def get_pch_config_for_unit_tests(
    use_shared: bool = True,
) -> tuple[Optional[str], Optional[str]]:
    """Get PCH configuration for unit tests.

    Args:
        use_shared: If True, use shared PCH. If False, use unit test specific PCH.

    Returns:
        Tuple of (pch_output_path, pch_header_content)
    """
    if use_shared and should_use_shared_pch():
        return (str(get_shared_pch_path()), get_shared_pch_content())

    # Fallback to unit test specific PCH (legacy)
    cache_dir = PROJECT_ROOT / ".build" / "cache"
    cache_dir.mkdir(parents=True, exist_ok=True)
    pch_path = str(cache_dir / "fastled_unit_test_pch.hpp.pch")

    # Use same content as shared (they're identical now)
    return (pch_path, get_shared_pch_content())


def get_pch_config_for_examples(
    use_shared: bool = True,
) -> tuple[Optional[str], Optional[str]]:
    """Get PCH configuration for examples.

    Args:
        use_shared: If True, use shared PCH. If False, use example specific PCH.

    Returns:
        Tuple of (pch_output_path, pch_header_content)
    """
    if use_shared and should_use_shared_pch():
        return (str(get_shared_pch_path()), get_shared_pch_content())

    # Fallback to example specific PCH (legacy - minimal headers)
    cache_dir = PROJECT_ROOT / ".build" / "cache"
    cache_dir.mkdir(parents=True, exist_ok=True)
    pch_path = str(cache_dir / "fastled_pch.hpp.pch")

    # Minimal PCH for examples (legacy behavior)
    minimal_content = """// FastLED Example PCH - Minimal headers for examples
#pragma once

#include "FastLED.h"
"""
    return (pch_path, minimal_content)
