#!/usr/bin/env python3
"""Checker to require Emscripten JS glue macros live only in *.js.cpp.hpp files."""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


_ASM_JS_MACRO = re.compile(r"\b(?:EM_JS|EM_ASYNC_JS|EM_ASM)\s*\(")


class AsmJsLocationChecker(FileContentChecker):
    """Reject EM_JS/EM_ASYNC_JS/EM_ASM outside *.js.cpp.hpp files."""

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        normalized = file_path.replace("\\", "/")
        if "/src/" not in normalized and not normalized.startswith("src/"):
            return False
        if "/third_party/" in normalized:
            return False
        return normalized.endswith((".h", ".hpp", ".cpp", ".cpp.hpp"))

    def check_file_content(self, file_content: FileContent) -> list[str]:
        normalized = file_content.path.replace("\\", "/")
        if normalized.endswith(".js.cpp.hpp"):
            return []
        if not any(
            token in file_content.content
            for token in ("EM_JS(", "EM_ASYNC_JS(", "EM_ASM(")
        ):
            return []

        violations: list[tuple[int, str]] = []
        in_multiline_comment = False
        for line_number, line in enumerate(file_content.lines, 1):
            stripped = line.strip()

            if "/*" in line:
                in_multiline_comment = True
            if in_multiline_comment and "*/" in line:
                in_multiline_comment = False
                continue
            if in_multiline_comment or stripped.startswith("//"):
                continue

            code = stripped.split("//", 1)[0].strip()
            if not code:
                continue
            if _ASM_JS_MACRO.search(code):
                violations.append(
                    (
                        line_number,
                        "Emscripten JS glue macros must live in a *.js.cpp.hpp file: "
                        f"{stripped}",
                    )
                )

        if violations:
            self.violations[file_content.path] = violations

        return []


def main() -> None:
    from ci.util.check_files import run_checker_standalone

    checker = AsmJsLocationChecker()
    run_checker_standalone(
        checker,
        [str(PROJECT_ROOT / "src")],
        "Found Emscripten JS glue macros outside *.js.cpp.hpp files",
        extensions=[".h", ".hpp", ".cpp", ".cpp.hpp"],
    )


if __name__ == "__main__":
    main()
