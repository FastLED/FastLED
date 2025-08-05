# pyright: reportUnknownMemberType=false
import os
import unittest
from concurrent.futures import ThreadPoolExecutor
from typing import List

from ci.util.paths import PROJECT_ROOT


NUM_WORKERS = 1 if os.environ.get("NO_PARALLEL") else (os.cpu_count() or 1) * 4


WASM_ROOT = PROJECT_ROOT / "src" / "platforms" / "wasm"


class TestMissingPragmaOnce(unittest.TestCase):
    def check_file(self, file_path: str) -> list[str]:
        """Check if a header file has #pragma once directive or if a cpp file incorrectly has it."""
        failings: list[str] = []

        with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
            if file_path.endswith(".h") or file_path.endswith(".cpp"):
                content = f.read()
                # For header files, check if #pragma once is missing
                if "EM_ASM_" in content and "// clang-format off\n" not in content:
                    if "clang-format off" not in content:
                        failings.append(f"Missing clang-format off in {file_path}")
                    else:
                        failings.append(f"clang-format off is malformed in {file_path}")

        return failings

    def test_esm_asm_and_clang_format(self) -> None:
        files_to_check: List[str] = []
        current_dir = None

        # Collect files to check
        for root, _, files in os.walk(WASM_ROOT):
            # Log when we enter a new directory
            rel_path = os.path.relpath(root, WASM_ROOT)
            if current_dir != rel_path:
                current_dir = rel_path
                print(f"Traversing directory: {rel_path}")

            for file in files:
                if file.endswith((".h", ".cpp")):  # Check both header and cpp files
                    file_path = os.path.join(root, file)
                    files_to_check.append(file_path)

        print(f"Found {len(files_to_check)} files to check")

        # Process files in parallel
        all_failings: List[str] = []
        with ThreadPoolExecutor(max_workers=NUM_WORKERS) as executor:
            futures = [
                executor.submit(self.check_file, file_path)
                for file_path in files_to_check
            ]
            for future in futures:
                all_failings.extend(future.result())

        # Report results
        if all_failings:
            msg = (
                f"Found {len(all_failings)} clang format issues in wasm: \n"
                + "\n".join(all_failings)
            )
            for failing in all_failings:
                print(failing)
            print(
                "Please be aware you need // then one space then clang-format off then a new line exactly"
            )
            self.fail(msg)
        else:
            print("All files passed the check.")

        print(f"Clange format check completed. Processed {len(files_to_check)} files.")


if __name__ == "__main__":
    unittest.main()
