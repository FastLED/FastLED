"""Exercise real feature selection against legacy and modern SDK capabilities."""

import shutil
from pathlib import Path

import pytest
from running_process import RunningProcess


ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "src/platforms/esp/32/feature_flags/enabled.h"


@pytest.mark.parametrize(
    ("caps", "chip", "idf_major", "expected_rmt", "expected_rmt5"),
    [
        ("#define SOC_RMT_CHANNELS_PER_GROUP 8", "FL_IS_ESP_32DEV", 4, 1, 0),
        ("#define SOC_RMT_CHANNELS_PER_GROUP 4", "FL_IS_ESP_32S2", 4, 1, 0),
        ("#define SOC_RMT_CHANNELS_PER_GROUP 0", "FL_IS_ESP_32DEV", 4, 0, 0),
        ("", "FL_IS_ESP_32DEV", 4, 0, 0),
        (
            "#define SOC_RMT_SUPPORTED 0\n#define SOC_RMT_CHANNELS_PER_GROUP 8",
            "FL_IS_ESP_32DEV",
            4,
            0,
            0,
        ),
        ("#define SOC_RMT_SUPPORTED 1", "FL_IS_ESP_32DEV", 5, 1, 1),
        ("#define SOC_RMT_SUPPORTED 1", "FL_IS_ESP_32C6", 5, 1, 1),
        ("#define SOC_RMT_SUPPORTED 1", "FL_IS_ESP_32C2", 5, 0, 0),
    ],
)
def test_sdk_capabilities_select_correct_rmt_backend(
    tmp_path: Path,
    caps: str,
    chip: str,
    idf_major: int,
    expected_rmt: int,
    expected_rmt5: int,
) -> None:
    compiler = shutil.which("clang++") or shutil.which("g++")
    if compiler is None:
        pytest.skip("C++ preprocessor unavailable")

    # Only replace SDK/platform dependencies. The policy under test is the
    # production enabled.h, so missing legacy capability handling fails here.
    headers: dict[str, str] = {
        "platforms/is_platform.h": f"#define FL_IS_ESP32 1\n#define {chip} 1\n",
        "fl/stl/compiler_control.h": "#define FL_EXTERN_C_BEGIN\n#define FL_EXTERN_C_END\n",
        "fl/stl/has_include.h": "#define FL_HAS_INCLUDE(x) 0\n",
        "sdkconfig.h": "",
        "platforms/esp/esp_version.h": (
            "#define ESP_IDF_VERSION_VAL(a,b,c) ((a)*10000+(b)*100+(c))\n"
            f"#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL({idf_major},4,7)\n"
        ),
        "soc/soc_caps.h": caps + "\n",
    }
    for name, contents in headers.items():
        path = tmp_path / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents, encoding="utf-8")

    source = (
        f'#include "{HEADER}"\n'
        f"static_assert(FASTLED_ESP32_HAS_RMT == {expected_rmt});\n"
        f"static_assert(FASTLED_RMT5 == {expected_rmt5});\n"
    )
    result = RunningProcess.run(
        [
            compiler,
            "-std=c++17",
            "-fsyntax-only",
            "-x",
            "c++",
            "-I",
            str(tmp_path),
            "-",
        ],
        input=source,
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
        check=False,
        timeout=30,
    )
    assert result.returncode == 0, result.stdout
