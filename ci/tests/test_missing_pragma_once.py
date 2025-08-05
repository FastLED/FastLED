# pyright: reportUnknownMemberType=false
import os
import unittest
from concurrent.futures import ThreadPoolExecutor
from typing import List

from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"

NUM_WORKERS = 1 if os.environ.get("NO_PARALLEL") else (os.cpu_count() or 1) * 4

# Files that are allowed to not have #pragma once
EXCLUDED_FILES: List[str] = [
    # Add any exceptions here
]

EXCLUDED_DIRS = [
    "third_party",
    "platforms",
]


class TestMissingPragmaOnce(unittest.TestCase):
    def check_file(self, file_path: str) -> list[str]:
        """Check if a header file has #pragma once directive or if a cpp file incorrectly has it."""
        failings: list[str] = []

        with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
            content = f.read()

            if file_path.endswith(".h"):
                # For header files, check if #pragma once is missing
                if "#pragma once" not in content:
                    failings.append(f"Missing #pragma once in {file_path}")
            elif file_path.endswith(".cpp"):
                # For cpp files, check if #pragma once is incorrectly present
                if "#pragma once" in content:
                    failings.append(f"Incorrect #pragma once in cpp file: {file_path}")

        return failings

    def test_pragma_once_usage(self) -> None:
        """
        Searches through files to:
        1. Check for missing #pragma once in header files
        2. Check for incorrect #pragma once in cpp files
        """
        files_to_check: List[str] = []
        current_dir = None

        # Collect files to check
        for root, dirs, files in os.walk(SRC_ROOT):
            # Log when we enter a new directory
            rel_path = os.path.relpath(root, SRC_ROOT)
            if current_dir != rel_path:
                current_dir = rel_path
                if rel_path in EXCLUDED_DIRS:
                    dirs[:] = []  # Skip this directory and its subdirectories
                    continue

            # Check if this directory should be excluded
            # if any(os.path.normpath(root).startswith(os.path.normpath(excluded_dir))
            #        for excluded_dir in EXCLUDED_DIRS):
            #     print(f"  Skipping excluded directory: {rel_path}")
            #     continue
            for excluded_dir in EXCLUDED_DIRS:
                npath = os.path.normpath(root)
                npath_excluded = os.path.normpath(excluded_dir)
                if npath.startswith(npath_excluded):
                    break

            for file in files:
                if file.endswith((".h", ".cpp")):  # Check both header and cpp files
                    file_path = os.path.join(root, file)

                    # Check if file is excluded
                    # if any(file_path.endswith(excluded) for excluded in EXCLUDED_FILES):
                    #     print(f"  Skipping excluded file: {file}")
                    #     continue
                    for excluded in EXCLUDED_FILES:
                        # print(f"Checking {file_path} against excluded {excluded}")
                        if file_path.endswith(excluded):
                            print(f"  Skipping excluded file: {file}")
                            break

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
            msg = f"Found {len(all_failings)} pragma once issues: \n" + "\n".join(
                all_failings
            )
            for failing in all_failings:
                print(failing)
            self.fail(msg)
        else:
            print("All files have proper pragma once usage.")

        print(f"Pragma once check completed. Processed {len(files_to_check)} files.")


if __name__ == "__main__":
    unittest.main()
