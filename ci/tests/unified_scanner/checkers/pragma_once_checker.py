# pyright: reportUnknownMemberType=false
from pathlib import Path

from ..base_checker import BaseChecker
from ..result import CheckResult


class PragmaOnceChecker(BaseChecker):
    """Checks that header files use #pragma once and cpp files don't."""

    def name(self) -> str:
        return "pragma-once"

    def should_check(self, file_path: Path) -> bool:
        if file_path.suffix not in {".h", ".hpp", ".cpp"}:
            return False

        path_str = str(file_path).replace("\\", "/")
        path_parts = path_str.split("/")

        # Skip examples (example headers don't need #pragma once)
        if "examples" in path_parts:
            return False

        # Skip tests (test headers don't need #pragma once)
        if "tests" in path_parts:
            return False

        return True

    def check_file(self, file_path: Path, content: str) -> list[CheckResult]:
        results: list[CheckResult] = []

        # Check for third-party or excluded directories
        excluded_dirs = ["third_party", "platforms"]
        for excluded in excluded_dirs:
            if excluded in str(file_path):
                return results

        has_pragma_once = "#pragma once" in content

        if file_path.suffix in {".h", ".hpp"}:
            # Header files should have #pragma once
            if not has_pragma_once:
                results.append(
                    CheckResult(
                        checker_name=self.name(),
                        file_path=str(file_path),
                        line_number=1,
                        severity="ERROR",
                        message="Missing #pragma once directive",
                        suggestion="Add '#pragma once' at the top of the header file",
                    )
                )
        elif file_path.suffix == ".cpp":
            # CPP files should NOT have #pragma once
            if has_pragma_once:
                results.append(
                    CheckResult(
                        checker_name=self.name(),
                        file_path=str(file_path),
                        line_number=1,
                        severity="ERROR",
                        message="Incorrect #pragma once in cpp file",
                        suggestion="Remove '#pragma once' from cpp files",
                    )
                )

        return results
