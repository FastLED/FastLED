"""Compile contracts for the ``NEED_CXX_BITS`` ABI fallback."""

from __future__ import annotations

import os
import shutil
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from running_process import PIPE, RunningProcess


ROOT = Path(__file__).resolve().parents[2]
ERROR_TEXT = "FastLED #4994: NEED_CXX_BITS on ESP32/ESP8266 breaks thread-safe local statics with LTO"


def _find_cxx() -> str | None:
    """Locate the C++ driver used by the native build."""
    suffix = ".exe" if sys.platform == "win32" else ""
    launcher = ROOT / ".cached" / "clang-native" / f"ctc-clang++{suffix}"
    if launcher.is_file():
        return str(launcher)
    bindir = Path(sys.prefix) / ("Scripts" if sys.platform == "win32" else "bin")
    wrapper = bindir / f"clang-tool-chain-cpp{suffix}"
    if wrapper.is_file():
        return str(wrapper)
    return shutil.which("clang-tool-chain-cpp")


CXX = _find_cxx()


@unittest.skipIf(
    CXX is None and not os.environ.get("CI"),
    "FastLED clang toolchain not installed (run ./install)",
)
class TestNeedCxxBitsPlatformGuard(unittest.TestCase):
    def setUp(self) -> None:
        if CXX is None:
            self.fail("CI must have the FastLED clang toolchain; refusing to skip")
        self._tmp = TemporaryDirectory()
        self.dir = Path(self._tmp.name)
        self.tu = self.dir / "need_cxx_bits.cpp"
        self.tu.write_text('#include "FastLED.cpp.hpp"\n', encoding="utf-8")

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def _compile(
        self, *defines: str, include_fastled: bool = False, skip_fastled: bool = False
    ) -> object:
        assert CXX is not None
        if include_fastled:
            self.tu.write_text(
                '#include "FastLED.h"\n#include "FastLED.cpp.hpp"\n', encoding="utf-8"
            )
        return RunningProcess.run(
            [
                CXX,
                "-std=gnu++17",
                "-I",
                str(ROOT / "src"),
                *(["-D__INC_FASTSPI_LED2_H"] if skip_fastled else []),
                *(f"-D{define}" for define in defines),
                "-c",
                str(self.tu),
                "-o",
                str(self.dir / "need_cxx_bits.o"),
            ],
            stdout=PIPE,
            stderr=PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )

    def test_esp32_rejects_need_cxx_bits(self) -> None:
        result = self._compile("NEED_CXX_BITS", "FL_IS_ESP32", skip_fastled=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(ERROR_TEXT, result.stderr)

    def test_esp8266_rejects_need_cxx_bits(self) -> None:
        result = self._compile("NEED_CXX_BITS", "FL_IS_ESP8266", skip_fastled=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(ERROR_TEXT, result.stderr)

    def test_non_esp_platforms_keep_the_fallback(self) -> None:
        result = self._compile("NEED_CXX_BITS", include_fastled=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_platforms_without_need_cxx_bits_compile(self) -> None:
        result = self._compile(include_fastled=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
