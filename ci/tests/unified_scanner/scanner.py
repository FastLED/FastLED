# pyright: reportUnknownMemberType=false
import threading
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from queue import Queue

from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly

from .base_checker import BaseChecker
from .result import CheckResult


class UnifiedFileScanner:
    """
    Scans filesystem once and dispatches to multiple checkers.
    Thread-safe and efficient.
    """

    def __init__(self, checkers: list[BaseChecker], max_workers: int = 4):
        self.checkers = checkers
        self.max_workers = max_workers
        self.result_queue: Queue[CheckResult] = Queue()
        self._lock = threading.Lock()

    def scan_directories(
        self,
        directories: list[Path],
        extensions: set[str] = {".cpp", ".h", ".hpp", ".cc", ".ino"},
    ) -> list[CheckResult]:
        """
        Scan directories and run all checkers.

        Args:
            directories: List of directories to scan
            extensions: File extensions to check

        Returns:
            List of all CheckResult violations found
        """
        # Collect all files to check (SINGLE filesystem scan)
        files_to_check: list[Path] = []
        for directory in directories:
            if not directory.exists():
                continue
            for file_path in directory.rglob("*"):
                if file_path.is_file() and file_path.suffix in extensions:
                    files_to_check.append(file_path)

        print(f"[UnifiedScanner] Found {len(files_to_check)} files to check")
        print(f"[UnifiedScanner] Running {len(self.checkers)} checkers")

        # Process files in parallel
        with ThreadPoolExecutor(max_workers=self.max_workers) as executor:
            futures = [
                executor.submit(self._check_file, file_path)
                for file_path in files_to_check
            ]

            # Wait for all to complete
            for future in futures:
                future.result()

        # Collect all results from queue
        results: list[CheckResult] = []
        while not self.result_queue.empty():
            results.append(self.result_queue.get())

        return results

    def _check_file(self, file_path: Path) -> None:
        """Check single file with all applicable checkers."""
        try:
            # Read file ONCE
            with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
                content = f.read()

            # Run all applicable checkers
            for checker in self.checkers:
                if checker.should_check(file_path):
                    violations = checker.check_file(file_path, content)
                    for violation in violations:
                        self.result_queue.put(violation)

            handle_keyboard_interrupt_properly()
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            # Report file read errors
            self.result_queue.put(
                CheckResult(
                    checker_name="FileReader",
                    file_path=str(file_path),
                    line_number=None,
                    severity="ERROR",
                    message=f"Failed to read file: {e}",
                    suggestion=None,
                )
            )
