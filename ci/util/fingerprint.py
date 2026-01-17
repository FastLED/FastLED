import json
from pathlib import Path
from typing import Callable, Dict, Optional

from ci.util.test_types import (
    FingerprintResult,
    TestArgs,
    calculate_cpp_test_fingerprint,
    calculate_examples_fingerprint,
    calculate_fingerprint,
    calculate_python_test_fingerprint,
)
from ci.util.timestamp_print import ts_print


class FingerprintManager:
    def __init__(self, cache_dir: Path, build_mode: str = "quick"):
        self.cache_dir = cache_dir
        self.build_mode = build_mode
        self.fingerprint_dir = cache_dir / "fingerprint"
        self.cache_dir.mkdir(exist_ok=True)
        self.fingerprint_dir.mkdir(exist_ok=True)
        self._fingerprints: dict[str, FingerprintResult] = {}
        self._prev_fingerprints: dict[str, Optional[FingerprintResult]] = {}

    def _get_fingerprint_file(self, name: str) -> Path:
        # For cpp_test and examples, include build mode in filename to separate caches per build mode
        if name in ("cpp_test", "examples"):
            return self.fingerprint_dir / f"{name}_{self.build_mode}.json"
        return self.fingerprint_dir / f"{name}.json"

    def read(self, name: str) -> Optional[FingerprintResult]:
        fingerprint_file = self._get_fingerprint_file(name)
        if fingerprint_file.exists():
            with open(fingerprint_file, "r") as f:
                try:
                    data = json.load(f)
                    return FingerprintResult(
                        hash=data.get("hash", ""),
                        elapsed_seconds=data.get("elapsed_seconds"),
                        status=data.get("status"),
                    )
                except json.JSONDecodeError:
                    ts_print(f"Invalid {name} fingerprint file. Recalculating...")
        return None

    def write(self, name: str, fingerprint: FingerprintResult) -> None:
        fingerprint_file = self._get_fingerprint_file(name)
        fingerprint_dict = {
            "hash": fingerprint.hash,
            "elapsed_seconds": fingerprint.elapsed_seconds,
            "status": fingerprint.status,
        }
        with open(fingerprint_file, "w") as f:
            json.dump(fingerprint_dict, f, indent=2)

    def check(self, name: str, calculator: Callable[[], FingerprintResult]) -> bool:
        prev_fingerprint = self.read(name)
        self._prev_fingerprints[name] = prev_fingerprint

        fingerprint_data = calculator()
        self._fingerprints[name] = fingerprint_data

        if prev_fingerprint is None:
            return True
        return not prev_fingerprint.should_skip(fingerprint_data)

    def save_all(self, status: str) -> None:
        for name, fingerprint in self._fingerprints.items():
            fingerprint.status = status
            self.write(name, fingerprint)

    def check_cpp(self, args: TestArgs) -> bool:
        return self.check("cpp_test", lambda: calculate_cpp_test_fingerprint(args))

    def check_examples(self, args: TestArgs) -> bool:
        return self.check("examples", lambda: calculate_examples_fingerprint(args))

    def check_python(self) -> bool:
        return self.check("python_test", calculate_python_test_fingerprint)

    def check_all(self) -> bool:
        return self.check("all", calculate_fingerprint)
