#!/usr/bin/env python3
"""Checker to ensure stdint types (uint8_t, uint16_t, uint32_t, uint64_t, etc.) are not used.

Use FastLED's own integer types (u8, u16, u32, u64, i8, i16, i32, i64) from fl/int.h
instead of stdint types to maintain consistency across the codebase.
"""

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


# Directories excluded from checking (vendor/third-party code)
_EXCLUDED_DIRS = [
    "/third_party/",
]

# Files excluded from checking (type definition files that intentionally use stdint types)
_EXCLUDED_FILENAMES = {
    "stdint.h",  # fl/stl/stdint.h - defines uint8_t as typedef of fl::u8
    "cstdint.h",  # fl/stl/cstdint.h - C language type definitions
    "run_unit_test.hpp",  # Platform runners use std:: types to avoid FastLED dependencies
}

# Mapping of stdint types to FastLED equivalents
STDINT_TO_FL: dict[str, str] = {
    "uint8_t": "u8",
    "uint16_t": "u16",
    "uint32_t": "u32",
    "uint64_t": "u64",
    "int8_t": "i8",
    "int16_t": "i16",
    "int32_t": "i32",
    "int64_t": "i64",
}

# Word boundary characters for manual boundary checking
_WORD_CHARS = frozenset(
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"
)


def _normalize_path(path: str) -> str:
    """Normalize path separators to forward slashes."""
    return path.replace("\\", "/")


def _find_stdint_matches(code_part: str) -> list[str]:
    """Find all stdint type matches in a code string using tree-match.

    Uses a single str.find("int") scan and branches on the suffix character
    to identify which of the 8 types matched (u8/u16/u32/u64/i8/i16/i32/i64).
    ~10x faster than the original regex approach, ~13% faster than a combined
    regex alternation.

    Phase 1 (fast, false positives OK): str.find("int") locates candidates.
    Phase 2 (exact, no false positives): suffix check + word boundary verification.
    """
    word_chars = _WORD_CHARS
    cp_len = len(code_part)
    matches: list[str] = []
    idx = 0

    while True:
        idx = code_part.find("int", idx)
        if idx == -1:
            break

        # Phase 1 found "int" at idx. Now check suffix for [u]int{8,16,32,64}_t
        after = idx + 3  # position right after "int"
        matched_type = ""
        start = idx
        end = idx

        if after < cp_len:
            c = code_part[after]
            if (
                c == "8"
                and after + 3 <= cp_len
                and code_part[after : after + 3] == "8_t"
            ):
                # int8_t (6 chars) or uint8_t (7 chars)
                if idx > 0 and code_part[idx - 1] == "u":
                    start = idx - 1
                    end = start + 7
                    matched_type = "uint8_t"
                else:
                    end = idx + 6
                    matched_type = "int8_t"
            elif (
                c == "1"
                and after + 4 <= cp_len
                and code_part[after : after + 4] == "16_t"
            ):
                # int16_t (7 chars) or uint16_t (8 chars)
                if idx > 0 and code_part[idx - 1] == "u":
                    start = idx - 1
                    end = start + 8
                    matched_type = "uint16_t"
                else:
                    end = idx + 7
                    matched_type = "int16_t"
            elif (
                c == "3"
                and after + 4 <= cp_len
                and code_part[after : after + 4] == "32_t"
            ):
                # int32_t (7 chars) or uint32_t (8 chars)
                if idx > 0 and code_part[idx - 1] == "u":
                    start = idx - 1
                    end = start + 8
                    matched_type = "uint32_t"
                else:
                    end = idx + 7
                    matched_type = "int32_t"
            elif (
                c == "6"
                and after + 4 <= cp_len
                and code_part[after : after + 4] == "64_t"
            ):
                # int64_t (7 chars) or uint64_t (8 chars)
                if idx > 0 and code_part[idx - 1] == "u":
                    start = idx - 1
                    end = start + 8
                    matched_type = "uint64_t"
                else:
                    end = idx + 7
                    matched_type = "int64_t"

        if matched_type:
            # Phase 2: exact word boundary verification
            if (start == 0 or code_part[start - 1] not in word_chars) and (
                end >= cp_len or code_part[end] not in word_chars
            ):
                matches.append(matched_type)
            idx = end
        else:
            idx += 3  # skip past "int", no match

    return matches


class StdintTypeChecker(FileContentChecker):
    """Checker that flags stdint types (uint32_t, etc.) in platform files."""

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Process C++ files under checked platform and src directories."""
        normalized = _normalize_path(file_path)

        # Only C++ source/header files
        if not normalized.endswith((".cpp", ".h", ".hpp", ".cpp.hpp")):
            return False

        # Check excluded filenames (type definition files)
        filename = normalized.rsplit("/", 1)[-1] if "/" in normalized else normalized
        if filename in _EXCLUDED_FILENAMES:
            return False

        # Must be under src/ directory
        if "/src/" not in normalized:
            return False

        # Exclude third-party/vendor code
        if any(d in normalized for d in _EXCLUDED_DIRS):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for stdint type usage."""
        # Fast pre-scan: all stdint types contain "int", bail if absent
        if "int" not in file_content.content:
            return []

        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        for line_number, line in enumerate(file_content.lines, 1):
            # Fast bail: all stdint types contain "int"
            if (
                not in_multiline_comment
                and "int" not in line
                and "/*" not in line
                and "*/" not in line
            ):
                continue

            # Skip full-line comments (before multiline comment tracking
            # to avoid false multiline triggers from /* inside // comments)
            i = 0
            n = len(line)
            while i < n and (line[i] == " " or line[i] == "\t"):
                i += 1
            is_line_comment = i + 1 < n and line[i] == "/" and line[i + 1] == "/"

            if is_line_comment and not in_multiline_comment:
                continue

            # Track multi-line comment state using only the code portion
            # (exclude text after // to avoid false triggers from /* in comments)
            if in_multiline_comment:
                if "*/" in line:
                    in_multiline_comment = False
                continue

            code_for_comment_tracking = line.split("//", 1)[0]
            if "/*" in code_for_comment_tracking:
                # Check if the comment closes on the same line (inline comment)
                open_pos = code_for_comment_tracking.index("/*")
                close_pos = code_for_comment_tracking.find("*/", open_pos + 2)
                if close_pos == -1:
                    # True multiline comment start - skip rest of line
                    in_multiline_comment = True
                    continue

            # Fast bail: all stdint types contain "int"
            if "int" not in line:
                continue

            # Remove trailing comment before checking
            code_part = line.split("//", 1)[0]

            # Two-phase match: fast string find → exact boundary check
            matches = _find_stdint_matches(code_part)
            if matches:
                stripped = line.strip()
                for match in matches:
                    fl_type = STDINT_TO_FL[match]
                    violations.append(
                        (
                            line_number,
                            f"Use '{fl_type}' from fl/int.h instead of '{match}': {stripped}",
                        )
                    )

        if violations:
            self.violations[file_content.path] = violations

        return []


def main() -> None:
    """Run stdint type checker standalone."""
    from ci.util.check_files import run_checker_standalone

    # Check all of src/ (checker's should_process_file excludes third_party/)
    target_dirs = [str(PROJECT_ROOT / "src")]
    checker = StdintTypeChecker()
    run_checker_standalone(
        checker,
        target_dirs,
        "Found stdint types in checked files (use u8/u16/u32/u64/i8/i16/i32/i64 from fl/int.h instead)",
    )


if __name__ == "__main__":
    main()
