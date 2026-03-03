#!/usr/bin/env python3
"""Checker for bare heap allocation/deallocation.

Detects usage of new, delete, malloc, calloc, realloc, free and suggests
using fl::unique_ptr, fl::shared_ptr, or fl::shared_array instead.

Suppression: add '// ok bare allocation' or '// okay bare allocation' to the line.
"""

import re

from ci.util.check_files import EXCLUDED_FILES, FileContent, FileContentChecker


# Files that legitimately need bare allocation (memory infrastructure)
_WHITELISTED_SUFFIXES: tuple[str, ...] = (
    # STL allocator infrastructure
    "fl/stl/allocator.h",
    "fl/stl/allocator.cpp.hpp",
    "fl/stl/malloc.h",
    "fl/stl/malloc.cpp.hpp",
    "fl/stl/new.h",
    # Smart pointer implementations
    "fl/stl/unique_ptr.h",
    "fl/stl/shared_ptr.h",
    "fl/stl/shared_ptr.cpp.hpp",
    "fl/stl/scoped_ptr.h",
    "fl/stl/weak_ptr.h",
    # Legacy array smart pointer
    "fl/scoped_array.h",
    # String holder (uses malloc/realloc/free internally)
    "fl/stl/detail/string_holder.cpp.hpp",
    # Memory introspection
    "fl/memory.h",
    "fl/memory.cpp.hpp",
)

_SUPPRESSION_OK = "// ok bare allocation"
_SUPPRESSION_OKAY = "// okay bare allocation"

# Regex to strip string literals (both "" and '') to avoid false positives
_STRING_LITERAL_RE = re.compile(r'"(?:[^"\\]|\\.)*"|\'(?:[^\'\\]|\\.)*\'')

# Patterns for bare allocation
# new <Type> — matches allocation but NOT placement new (new (ptr) Type)
# and NOT operator new
_NEW_ALLOC_RE = re.compile(r"\bnew\s+[A-Za-z_\:]")

# delete as standalone keyword (not part of delete_request, deleteFixup, etc.)
_DELETE_RE = re.compile(r"\bdelete\b")

# malloc(, calloc(, realloc( — but NOT fl::malloc( or obj.malloc(
_MALLOC_FAMILY_RE = re.compile(r"(?<!::)(?<!\.)\b(malloc|calloc|realloc)\s*\(")

# free( — but NOT fl::free(, obj.free(, or xxx_free(
_FREE_RE = re.compile(r"(?<!::)(?<!\.)\bfree\s*\(")

# Pre-compiled patterns for deleted functions (= delete;)
_DELETED_FUNC_RE = re.compile(r"=\s*delete\s*[;{]")
_DELETED_FUNC_EOL_RE = re.compile(r"=\s*delete\s*$")

_SUGGESTION_NEW = (
    "Use fl::make_unique<T>() or fl::make_shared<T>() instead of bare 'new', "
    "or add '// ok bare allocation' to suppress"
)

_SUGGESTION_DELETE = (
    "Use fl::unique_ptr or fl::shared_ptr (automatic cleanup) instead of bare 'delete', "
    "or add '// ok bare allocation' to suppress"
)

_SUGGESTION_MALLOC = (
    "Use fl::make_unique<T>() or fl::make_shared<T>() instead, "
    "or fl::malloc() if raw memory is required, "
    "or add '// ok bare allocation' to suppress"
)

_SUGGESTION_FREE = (
    "Use fl::unique_ptr or fl::shared_ptr (automatic cleanup) instead, "
    "or fl::free() if raw memory is required, "
    "or add '// ok bare allocation' to suppress"
)


class BareAllocationChecker(FileContentChecker):
    """Checker for bare new/delete/malloc/calloc/realloc/free usage."""

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino", ".cpp.hpp")):
            return False

        if any(file_path.endswith(excluded) for excluded in EXCLUDED_FILES):
            return False

        if "third_party" in file_path or "thirdparty" in file_path:
            return False

        normalized = file_path.replace("\\", "/")
        if any(normalized.endswith(suffix) for suffix in _WHITELISTED_SUFFIXES):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        for line_number, line in enumerate(file_content.lines, 1):
            stripped = line.strip()

            # Track multi-line comment state
            if "/*" in line:
                in_multiline_comment = True
            if "*/" in line:
                in_multiline_comment = False
                continue

            if in_multiline_comment:
                continue

            # Skip single-line comment lines
            if stripped.startswith("//"):
                continue

            # Check for suppression comment on this line
            if _SUPPRESSION_OK in line or _SUPPRESSION_OKAY in line:
                continue

            # Fast first pass: skip expensive regex/string-strip if line contains
            # none of the allocation keywords (allows false positives, no false negatives)
            if not (
                "new" in line
                or "delete" in line
                or "malloc" in line
                or "calloc" in line
                or "realloc" in line
                or "free" in line
            ):
                continue

            # Remove inline comment portion before checking
            code_part = line.split("//")[0]

            # Strip string literals to avoid false positives
            # (e.g., "test for new features" should not trigger)
            code_part = _STRING_LITERAL_RE.sub('""', code_part)

            # Skip operator new/delete overloads and declarations
            if "operator new" in code_part or "operator delete" in code_part:
                continue

            # Skip deleted functions: = delete
            if _DELETED_FUNC_RE.search(code_part):
                continue
            # Also handle = delete at end of line (before comment was stripped)
            if _DELETED_FUNC_EOL_RE.search(code_part.rstrip()):
                continue

            # Check for bare new
            if _NEW_ALLOC_RE.search(code_part):
                violations.append(
                    (line_number, f"bare 'new': {stripped} — {_SUGGESTION_NEW}")
                )
                continue

            # Check for bare delete (keyword only, not part of identifiers)
            if _DELETE_RE.search(code_part):
                violations.append(
                    (line_number, f"bare 'delete': {stripped} — {_SUGGESTION_DELETE}")
                )
                continue

            # Check for malloc/calloc/realloc (not fl:: prefixed)
            match = _MALLOC_FAMILY_RE.search(code_part)
            if match:
                func = match.group(1)
                violations.append(
                    (line_number, f"bare '{func}': {stripped} — {_SUGGESTION_MALLOC}")
                )
                continue

            # Check for free (not fl:: prefixed)
            if _FREE_RE.search(code_part):
                violations.append(
                    (line_number, f"bare 'free': {stripped} — {_SUGGESTION_FREE}")
                )
                continue

        if violations:
            self.violations[file_content.path] = violations

        return []  # MUST return empty list


def main() -> None:
    """Run bare allocation checker standalone."""
    from ci.util.check_files import run_checker_standalone
    from ci.util.paths import PROJECT_ROOT

    checker = BareAllocationChecker()
    run_checker_standalone(
        checker,
        [
            str(PROJECT_ROOT / "src"),
            str(PROJECT_ROOT / "examples"),
            str(PROJECT_ROOT / "tests"),
        ],
        (
            "Found bare heap allocation — use fl::unique_ptr, fl::shared_ptr, "
            "or fl::shared_array, or add '// ok bare allocation' comment"
        ),
    )


if __name__ == "__main__":
    main()
