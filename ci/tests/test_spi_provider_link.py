"""Link actual SPI selection code with absent, archive and direct providers."""

import shutil
import sys
from pathlib import Path

import pytest
from running_process import RunningProcess


SRC = Path(__file__).resolve().parents[2] / "src"
MARKER = b"FASTLED_NATIVE_SPI_FALLBACK_4890"


@pytest.mark.parametrize("mode", ["absent", "archive", "direct"])
@pytest.mark.skipif(sys.platform != "linux", reason="ELF linker GC probe")
def test_spi_provider_linkage(tmp_path: Path, mode: str) -> None:
    compiler = shutil.which("c++") or shutil.which("clang++")
    archive = shutil.which("ar")
    if compiler is None or archive is None:
        pytest.skip("C++ compiler and ELF archiver required")

    def run(arguments: list[str]) -> None:
        result = RunningProcess.run(
            arguments,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=60,
        )
        assert result.returncode == 0, result.stdout

    entry = tmp_path / "entry.cpp"
    entry.write_text(
        '#include "platforms/esp/32/drivers/spi/spi_provider.hpp"\n'
        "extern int nativeCalls;\nextern const char* volatile nativeMarker;\n"
        "namespace fl { namespace platforms { namespace detail {\n"
        "fl::shared_ptr<SpiHw1> makeNativeSpiHw1(u8 bus) FL_NO_EXCEPT {\n"
        " if (bus != 2 && bus != 3) __builtin_trap();\n"
        ' ++nativeCalls; nativeMarker="FASTLED_NATIVE_SPI_FALLBACK_4890"; return {};\n}}}}\n'
    )
    provider = tmp_path / "provider.cpp"
    provider.write_text(
        '#include "platforms/esp/32/drivers/spi/spi_provider.h"\n'
        "extern int providerCalls;\n"
        "namespace fl { namespace platforms {\n"
        "fl::shared_ptr<SpiHw1> fastled_spi_provider(u8 bus) FL_NO_EXCEPT {\n"
        " if (bus != 2 && bus != 3) __builtin_trap();\n"
        " ++providerCalls; return {};\n}}}\n"
    )
    main = tmp_path / "main.cpp"
    expected_native = 0 if mode == "direct" else 2
    expected_provider = 2 if mode == "direct" else 0
    main.write_text(
        '#include "fl/stl/shared_ptr.cpp.hpp"\n'
        '#include "platforms/shared/spi_hw_1.h"\n'
        "namespace fl { shared_ptr<SpiHw1>& getController2() FL_NO_EXCEPT;\n"
        "shared_ptr<SpiHw1>& getController3() FL_NO_EXCEPT; }\n"
        "int nativeCalls=0; int providerCalls=0;\n"
        "const char* volatile nativeMarker=nullptr;\n"
        "int main() {\n"
        " if (fl::getController2() || fl::getController3()) return 1;\n"
        " if (&fl::getController2() != &fl::getController2()) return 2;\n"
        " if (&fl::getController3() != &fl::getController3()) return 3;\n"
        f" return nativeCalls == {expected_native} && providerCalls == {expected_provider} ? 0 : 4;\n"
        "}\n"
    )
    flags = [
        compiler,
        "-std=c++11",
        "-Os",
        "-ffunction-sections",
        "-fdata-sections",
        "-DFASTLED_ESP32_HAS_SPI3=1",
        f"-I{SRC}",
    ]
    entry_obj = tmp_path / "entry.o"
    provider_obj = tmp_path / "provider.o"
    run([*flags, "-c", str(entry), "-o", str(entry_obj)])
    run([*flags, "-c", str(provider), "-o", str(provider_obj)])
    optional = tmp_path / "liboptional.a"
    run([archive, "rcs", str(optional), str(provider_obj)])
    dependencies = (
        [str(provider_obj)]
        if mode == "direct"
        else [str(optional)]
        if mode == "archive"
        else []
    )
    executable = tmp_path / "probe"
    run(
        [
            *flags,
            str(main),
            str(entry_obj),
            *dependencies,
            "-Wl,--gc-sections",
            "-o",
            str(executable),
        ]
    )
    run([str(executable)])
    assert (MARKER in executable.read_bytes()) == (mode != "direct")
