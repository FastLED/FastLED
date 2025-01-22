import os
import unittest
from concurrent.futures import ThreadPoolExecutor

from ci.paths import PROJECT_ROOT

SRC_ROOT = PROJECT_ROOT / "src"
# PLATFORMS_DIR = os.path.join(SRC_ROOT, "platforms")

NUM_WORKERS = (os.cpu_count() or 1) * 4

WRONG_DEFINES: dict[str, str] = {
    "#if ESP32": "Use #ifdef ESP32 instead of #if ESP32",
    "#if defined(FASTLED_RMT5)": "Use #ifdef FASTLED_RMT5 instead of #if defined(FASTLED_RMT5)",
    "#if defined(FASTLED_ESP_HAS_CLOCKLESS_SPI)": "Use #ifdef FASTLED_ESP_HAS_CLOCKLESS_SPI instead of #if defined(FASTLED_ESP_HAS_CLOCKLESS_SPI)",
}


class TestWrongDefines(unittest.TestCase):

    def check_file(self, file_path) -> list[str]:
        failings = []
        with open(file_path, "r", encoding="utf-8") as f:
            for line_number, line in enumerate(f, 1):
                line = line.strip()
                if line.startswith("//"):
                    continue
                for needle, message in WRONG_DEFINES.items():
                    if needle in line:
                        failings.append(f"{file_path}:{line_number}: {message}")
        return failings

    def test_no_bad_defines(self) -> None:
        """Searches through the program files to check for banned headers, excluding src/platforms."""
        files_to_check = []
        for root, _, files in os.walk(SRC_ROOT):
            for file in files:
                if file.endswith(
                    (".cpp", ".h", ".hpp")
                ):  # Add or remove file extensions as needed
                    file_path = os.path.join(root, file)
                    files_to_check.append(file_path)

        all_failings = []
        with ThreadPoolExecutor(max_workers=NUM_WORKERS) as executor:
            futures = [
                executor.submit(self.check_file, file_path)
                for file_path in files_to_check
            ]
            for future in futures:
                all_failings.extend(future.result())

        if all_failings:
            msg = f"Found {len(all_failings)} bad defines: \n" + "\n".join(all_failings)
            for failing in all_failings:
                print(failing)
            self.fail("Please fix the defines: \n" + msg + "\n")
        else:
            print("No bad defines found.")


if __name__ == "__main__":
    unittest.main()
