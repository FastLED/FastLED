# pyright: reportUnknownMemberType=false
from pathlib import Path

from ..base_checker import BaseChecker
from ..result import CheckResult


class BannedDefineChecker(BaseChecker):
    """Checks for banned #define patterns."""

    WRONG_DEFINES: dict[str, str] = {
        "#if ESP32": "Use #ifdef ESP32 instead of #if ESP32",
        "#if defined(FASTLED_RMT5)": "Use #ifdef FASTLED_RMT5 instead of #if defined(FASTLED_RMT5)",
        "#if defined(FASTLED_ESP_HAS_CLOCKLESS_SPI)": "Use #ifdef FASTLED_ESP_HAS_CLOCKLESS_SPI instead of #if defined(FASTLED_ESP_HAS_CLOCKLESS_SPI)",
    }

    def name(self) -> str:
        return "banned-defines"

    def should_check(self, file_path: Path) -> bool:
        return file_path.suffix in {".cpp", ".h", ".hpp", ".cc"}

    def check_file(self, file_path: Path, content: str) -> list[CheckResult]:
        results: list[CheckResult] = []
        lines = content.split("\n")

        for line_num, line in enumerate(lines, 1):
            stripped = line.strip()

            # Skip comments
            if stripped.startswith("//"):
                continue

            # Check for wrong defines
            for needle, message in self.WRONG_DEFINES.items():
                if needle in line:
                    results.append(
                        CheckResult(
                            checker_name=self.name(),
                            file_path=str(file_path),
                            line_number=line_num,
                            severity="ERROR",
                            message=message,
                            suggestion=None,
                        )
                    )

        return results
