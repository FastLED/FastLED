#!/usr/bin/env python3
"""Checker for plain enum usage - prefer enum class for type safety.

Plain enums (enum Foo { ... }) at namespace/global scope leak their enumerators
into the enclosing scope, causing name collisions and implicit integer conversions.
Prefer enum class (enum class Foo { ... }) which scopes enumerators and prevents
implicit conversions.

Enums inside class/struct bodies are NOT flagged because the enclosing type
already provides scoping.

Scope: Only checks src/fl/** and src/platforms/** files.
Root src/*.h files are treated as legacy and excluded.
Examples are excluded (legacy code with many plain enums).

Suppression: Add '// ok plain enum' comment on the line to suppress.
"""

import re

from ci.util.check_files import FileContent, FileContentChecker, is_excluded_file


# Match named plain enum declarations only (not enum class/struct).
# Captures: enum FooName {, enum FooName :
# Does NOT match:
#   - enum class, enum struct (scoped enums — already correct)
#   - enum { (anonymous enum — compile-time constant pattern)
#   - enum : type { (anonymous typed enum — metaprogramming constant pattern)
#   - typedef enum { (C-style anonymous enum)
#   - enum FooName in function params like "enum MicType type" (not a declaration)
_NAMED_ENUM_RE = re.compile(
    r"\benum\s+"
    r"(?!class\b)"  # not enum class
    r"(?!struct\b)"  # not enum struct
    r"([A-Za-z_]\w*)"  # capture the enum name (any identifier)
    r"\s*[{:;]"  # followed by {, :, or ; (declaration context)
)

# Match class/struct declarations that open a body (with '{').
# Handles: class Foo {, struct Bar : Base {, class FASTLED_API Baz {, template<> struct X {
# Does NOT match forward declarations (class Foo;) or function params.
_CLASS_STRUCT_RE = re.compile(
    r"\b(?:class|struct)\s+"
    r"(?:[A-Za-z_]\w*\s+)*"  # optional attributes/macros before name
    r"[A-Za-z_]\w*"  # the class/struct name
)


class EnumClassChecker(FileContentChecker):
    """Checker for plain enum usage - prefer enum class for type safety.

    Only runs on src/fl/** and src/platforms/** files.
    """

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed.

        Only processes files under src/fl/ and src/platforms/.
        """
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino", ".cpp.hpp")):
            return False

        if is_excluded_file(file_path):
            return False

        if "third_party" in file_path or "thirdparty" in file_path:
            return False

        # Skip examples/ — legacy code with many plain enums
        if "/examples/" in file_path or "\\examples\\" in file_path:
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for plain enum usage.

        Only flags plain enums at namespace/global scope.
        Enums inside class/struct bodies are skipped because the enclosing
        type already provides scoping.
        """
        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        # Track class/struct nesting via brace depth.
        brace_depth = 0
        # Stack: brace depth at which each class/struct body was entered.
        class_struct_enter_depths: list[int] = []
        # Set when we see a class/struct declaration without '{' on the same line.
        pending_class_struct = False

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

            if stripped.startswith("//"):
                continue

            # Remove single-line comment portion
            code_part = line.split("//")[0]

            # Detect class/struct declarations (not forward declarations).
            # A forward declaration like "class Foo;" has ';' before any '{'.
            if _CLASS_STRUCT_RE.search(code_part):
                before_brace = (
                    code_part.split("{")[0] if "{" in code_part else code_part
                )
                if ";" not in before_brace:
                    # Mark pending — the brace loop below will consume it on
                    # the first '{', whether on this line or a subsequent one.
                    pending_class_struct = True

            # Track braces and class/struct boundaries
            for ch in code_part:
                if ch == "{":
                    if pending_class_struct:
                        class_struct_enter_depths.append(brace_depth)
                        pending_class_struct = False
                    brace_depth += 1
                elif ch == "}":
                    brace_depth -= 1
                    if (
                        class_struct_enter_depths
                        and brace_depth == class_struct_enter_depths[-1]
                    ):
                        class_struct_enter_depths.pop()

            # Skip lines with suppression comment
            if "// ok plain enum" in line or "// okay plain enum" in line:
                continue

            # Fast first pass: skip regex if "enum" not in line
            if "enum" not in code_part:
                continue

            # Skip enums inside class/struct bodies — they are already scoped
            if class_struct_enter_depths:
                continue

            # Check for named plain enum declarations
            if _NAMED_ENUM_RE.search(code_part):
                violations.append(
                    (
                        line_number,
                        f"Plain enum detected - use 'enum class' for type safety. "
                        f"Plain enums leak names into enclosing scope and allow "
                        f"implicit integer conversions. "
                        f"Suppress with '// ok plain enum' for legacy enums: "
                        f"{stripped}",
                    )
                )

        if violations:
            self.violations[file_content.path] = violations

        return []  # MUST return empty list


def main() -> None:
    """Run enum class checker standalone."""
    from ci.util.check_files import run_checker_standalone
    from ci.util.paths import PROJECT_ROOT

    checker = EnumClassChecker()
    run_checker_standalone(
        checker,
        [
            str(PROJECT_ROOT / "src" / "fl"),
            str(PROJECT_ROOT / "src" / "platforms"),
        ],
        "Found plain enum usage - use 'enum class' for type safety!",
    )


if __name__ == "__main__":
    main()
