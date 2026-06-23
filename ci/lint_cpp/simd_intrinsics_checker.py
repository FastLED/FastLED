#!/usr/bin/env python3
"""Checker for direct platform SIMD intrinsic usage outside src/platforms/.

Direct SIMD intrinsics (__m128i, _mm_*, arm_neon.h, ee.vadds, etc.) should
only appear inside platform-specific SIMD implementations in src/platforms/.
All other code should use the fl::simd abstraction layer (fl/math/simd.h).
"""

from ci.util.check_files import EXCLUDED_FILES, FileContent, FileContentChecker


# Patterns that indicate direct platform SIMD intrinsic usage.
# Each tuple: (pattern_substring, description)
_SIMD_PATTERNS: list[tuple[str, str]] = [
    # x86 SSE/AVX types and intrinsics
    ("__m128i", "x86 SSE type — use fl::simd::simd_u8x16 / simd_u16x8 / simd_u32x4"),
    ("__m128", "x86 SSE type — use fl::simd types"),
    ("__m256i", "x86 AVX type — use fl::simd types"),
    ("__m256", "x86 AVX type — use fl::simd types"),
    ("_mm_", "x86 SSE intrinsic — use fl::simd operations"),
    ("_mm256_", "x86 AVX2 intrinsic — use fl::simd operations"),
    ("_mm512_", "x86 AVX-512 intrinsic — use fl::simd operations"),
    ("_MM_HINT", "x86 prefetch hint — use __builtin_prefetch"),
    # x86 SIMD headers
    ("<emmintrin.h>", "SSE2 header — use fl/math/simd.h"),
    ("<immintrin.h>", "AVX header — use fl/math/simd.h"),
    ("<smmintrin.h>", "SSE4.1 header — use fl/math/simd.h"),
    ("<xmmintrin.h>", "SSE header — use fl/math/simd.h"),
    ("<tmmintrin.h>", "SSSE3 header — use fl/math/simd.h"),
    # ARM NEON
    ("<arm_neon.h>", "ARM NEON header — use fl/math/simd.h"),
    ("vld1q_", "ARM NEON intrinsic — use fl::simd operations"),
    ("vst1q_", "ARM NEON intrinsic — use fl::simd operations"),
    ("vaddq_", "ARM NEON intrinsic — use fl::simd operations"),
    ("vmulq_", "ARM NEON intrinsic — use fl::simd operations"),
    ("vmovl_", "ARM NEON intrinsic — use fl::simd operations"),
    ("vqmovn_", "ARM NEON intrinsic — use fl::simd operations"),
    ("vcombine_", "ARM NEON intrinsic — use fl::simd operations"),
    ("vdupq_", "ARM NEON intrinsic — use fl::simd operations"),
    # ESP32 Xtensa PIE
    ("ee.vadds", "Xtensa PIE intrinsic — use fl::simd operations"),
    ("ee.vld", "Xtensa PIE intrinsic — use fl::simd operations"),
    ("ee.vst", "Xtensa PIE intrinsic — use fl::simd operations"),
]

# Suppression comment
_SUPPRESSION = "// ok platform simd"


class SimdIntrinsicsChecker(FileContentChecker):
    """Flags direct platform SIMD intrinsic usage outside src/platforms/."""

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        if not file_path.endswith((".cpp", ".h", ".hpp", ".cpp.hpp")):
            return False
        if any(file_path.endswith(excluded) for excluded in EXCLUDED_FILES):
            return False
        if "third_party" in file_path or "thirdparty" in file_path:
            return False
        # Allow direct intrinsics inside platform implementations
        normalized = file_path.replace("\\", "/")
        if "/platforms/" in normalized:
            return False
        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        for line_number, line in enumerate(file_content.lines, 1):
            stripped = line.strip()

            if "/*" in line:
                in_multiline_comment = True
            if "*/" in line:
                in_multiline_comment = False
                continue
            if in_multiline_comment:
                continue
            if stripped.startswith("//"):
                continue

            # Check for suppression comment
            if _SUPPRESSION in line:
                continue

            code_part = line.split("//")[0]

            for pattern, description in _SIMD_PATTERNS:
                if pattern in code_part:
                    violations.append((line_number, f"{stripped}  [{description}]"))
                    break  # One violation per line is enough

        if violations:
            self.violations[file_content.path] = violations

        return []  # MUST return empty list


def main() -> None:
    """Run SIMD intrinsics checker standalone."""
    from ci.util.check_files import run_checker_standalone
    from ci.util.paths import PROJECT_ROOT

    checker = SimdIntrinsicsChecker()
    run_checker_standalone(
        checker,
        [str(PROJECT_ROOT / "src")],
        f"Found direct SIMD intrinsic usage — use fl::simd abstraction, or add '{_SUPPRESSION}' comment",
    )


if __name__ == "__main__":
    main()
