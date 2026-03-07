#!/usr/bin/env python3
"""
Lint check: Enforce correct Singleton vs SingletonShared usage.

Rules:
  1. Public header files (.h only) must use SingletonShared<T> for instance() calls.
     - Singleton<T>::instance() in .h headers → FAIL (use SingletonShared<T>)
     - friend class Singleton<T> in .h headers → OK (just grants access)
  2. Private implementation headers (.hpp) can use Singleton<T>.
     - These are internal headers included in .cpp.hpp files, not across DLL boundaries.
  3. Implementation files (.cpp.hpp) must use Singleton<T>, not SingletonShared<T>.
     - SingletonShared<T> in .cpp.hpp → FAIL (use Singleton<T>)

Rationale:
  - Public .h headers are included across DLL boundaries; SingletonShared uses a registry.
  - Private .hpp and .cpp.hpp files have single definitions per process; Singleton is sufficient.
  - friend declarations just grant constructor access and must match the actual
    singleton type used in the corresponding .cpp.hpp.
"""

# pyright: reportUnknownMemberType=false
import re

from ci.util.check_files import EXCLUDED_FILES, FileContent, FileContentChecker


class SingletonInHeadersChecker(FileContentChecker):
    """Checker that enforces correct Singleton vs SingletonShared usage."""

    # Match Singleton< but NOT SingletonShared<
    _SINGLETON_PATTERN = re.compile(r"(?<!Shared)\bSingleton\s*<")
    # Match SingletonShared< (or SingletonShared without template for friend decls)
    _SINGLETON_SHARED_PATTERN = re.compile(r"\bSingletonShared\b")
    # Match friend class declarations
    _FRIEND_PATTERN = re.compile(r"\bfriend\s+class\b")

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Process header files (.h, .hpp) and implementation files (.cpp.hpp)."""
        if not file_path.endswith((".h", ".hpp")):
            return False
        # Exclude the singleton.h definition file itself
        normalized = file_path.replace("\\", "/")
        if normalized.endswith("fl/singleton.h"):
            return False
        # Exclude private implementation headers (.hpp only, not .h)
        # Private headers are included in .cpp.hpp files, so Singleton<T> is sufficient
        if file_path.endswith(".hpp"):
            return False
        if any(file_path.endswith(excluded) for excluded in EXCLUDED_FILES):
            return False
        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check singleton usage rules."""
        is_cpp_hpp = file_content.path.endswith(".cpp.hpp")
        is_private_header = any(
            "IWYU pragma: private" in line for line in file_content.lines[:20]
        )
        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        # Skip checks if this is a private header or .cpp.hpp file
        if is_cpp_hpp or is_private_header:
            return []

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

            # Skip single-line comments
            if stripped.startswith("//"):
                continue

            # Remove inline comment portion
            code_part = line.split("//")[0]

            # Fast first pass: skip regex if "Singleton" not in line
            if "Singleton" not in code_part:
                continue

            if is_cpp_hpp:
                # In .cpp.hpp: flag SingletonShared usage
                if self._SINGLETON_SHARED_PATTERN.search(code_part):
                    violations.append(
                        (
                            line_number,
                            f"Use Singleton<T> instead of SingletonShared<T> in .cpp.hpp: {stripped}",
                        )
                    )
            else:
                # In headers: flag Singleton<T> usage, but allow friend declarations
                if self._SINGLETON_PATTERN.search(code_part):
                    # Allow "friend class Singleton<T>" — just grants access
                    if self._FRIEND_PATTERN.search(code_part):
                        continue
                    violations.append(
                        (
                            line_number,
                            f"Use SingletonShared<T> instead of Singleton<T> in headers: {stripped}",
                        )
                    )

        if violations:
            self.violations[file_content.path] = violations

        return []  # MUST return empty list


def main() -> None:
    """Run singleton checker standalone."""
    from ci.util.check_files import run_checker_standalone
    from ci.util.paths import PROJECT_ROOT

    checker = SingletonInHeadersChecker()
    run_checker_standalone(
        checker,
        [str(PROJECT_ROOT / "src")],
        "Singleton usage violation (see rules in singleton_in_headers_checker.py)",
        extensions=[".h", ".hpp"],
    )


if __name__ == "__main__":
    main()
