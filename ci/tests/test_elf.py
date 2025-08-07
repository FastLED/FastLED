import subprocess
import unittest
import warnings
from pathlib import Path

from ci.util.elf import dump_symbol_sizes
from ci.util.paths import PROJECT_ROOT
from ci.util.tools import Tools, load_tools


HERE = Path(__file__).resolve().parent.absolute()
UNO = HERE / "uno"
OUTPUT = HERE / "output"
ELF_FILE = UNO / "firmware.elf"
BUILD_INFO_PATH = PROJECT_ROOT / ".build" / "uno" / "build_info.json"
BUILD_INFO_PATH2 = (
    PROJECT_ROOT / ".build" / "fled" / "examples" / "uno" / "build_info.json"
)


PLATFORMIO_PATH = Path.home() / ".platformio"
PLATFORMIO_PACKAGES_PATH = PLATFORMIO_PATH / "packages"
TOOLCHAIN_AVR = PLATFORMIO_PACKAGES_PATH / "toolchain-atmelavr"


def init() -> None:
    uno_build = PROJECT_ROOT / ".build" / "uno"
    print(f"Checking for Uno build in: {uno_build}")
    if not BUILD_INFO_PATH.exists() or not TOOLCHAIN_AVR.exists():
        print("Uno build not found. Running compilation...")
        try:
            subprocess.run(
                "uv run python -m ci.ci-compile uno --examples Blink",
                shell=True,
                check=True,
                cwd=str(PROJECT_ROOT),
            )
            print("Compilation completed successfully.")
        except subprocess.CalledProcessError as e:
            print(f"Error during compilation: {e}")
            raise


class TestBinToElf(unittest.TestCase):
    def test_bin_to_elf_conversion(self) -> None:
        # Skip test if required UNO build directory is missing
        uno_build_dir = PROJECT_ROOT / ".build" / "uno"
        if not uno_build_dir.exists():
            warnings.warn(
                "Skipping TestBinToElf::test_bin_to_elf_conversion because .build/uno does not exist. "
                "Run 'uv run ci/ci-compile.py uno --examples Blink' to generate it."
            )
            self.skipTest(".build/uno missing; skipping ELF conversion test")
        tools: Tools
        try:
            tools = load_tools(BUILD_INFO_PATH)
        except Exception as e:
            warnings.warn(f"Error while loading tools: {e}")
            tools = load_tools(BUILD_INFO_PATH2)
        msg = dump_symbol_sizes(tools.nm_path, tools.cpp_filt_path, ELF_FILE)
        print(msg)


if __name__ == "__main__":
    unittest.main()
