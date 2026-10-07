"""Compile the real RMT feature gate with legacy and modern SDK capabilities."""

import shutil
from pathlib import Path

import pytest
from running_process import RunningProcess


SRC = Path(__file__).resolve().parents[2] / "src"


@pytest.mark.parametrize("sdk_major", [4, 5])
@pytest.mark.parametrize(
    ("variant", "tx_channels", "support_flag", "override", "expected"),
    [
        ("32DEV", 8, None, None, 1),
        ("32S2", 4, None, None, 1),
        ("32S3", 4, None, None, 1),
        ("32C3", 2, None, None, 1),
        ("32C2", 0, None, None, 0),
        ("32DEV", 0, None, None, 0),
        ("32DEV", None, None, None, 0),
        ("32DEV", 8, 0, None, 0),
        ("32DEV", 8, 1, None, 1),
        ("32DEV", 8, None, 0, 0),
    ],
)
def test_rmt_capability_uses_vendor_tx_count_without_modern_flag(
    tmp_path: Path,
    sdk_major: int,
    variant: str,
    tx_channels: int | None,
    support_flag: int | None,
    override: int | None,
    expected: int,
) -> None:
    compiler = shutil.which("clang++") or shutil.which("g++")
    if compiler is None:
        pytest.skip("host C++ preprocessor unavailable")
    # Shadow only SDK/platform identification, leaving the actual feature gate
    # and portable support headers unchanged. Counts match IDF 4.4 SoC headers.
    headers: dict[str, str] = {
        "platforms/is_platform.h": (
            f"#define FL_IS_ESP32 1\n#define FL_IS_ESP_{variant} 1\n"
            "#if defined(__clang__)\n#define FL_IS_CLANG 1\n"
            "#elif defined(__GNUC__)\n#define FL_IS_GCC 1\n#endif\n"
        ),
        "sdkconfig.h": "",
        "platforms/esp/esp_version.h": (
            "#define ESP_IDF_VERSION_VAL(a,b,c) (((a)<<16)|((b)<<8)|(c))\n"
            f"#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL({sdk_major},4,1)\n"
        ),
        "soc/soc_caps.h": (
            ""
            if tx_channels is None
            else f"#define SOC_RMT_TX_CANDIDATES_PER_GROUP {tx_channels}\n"
        ),
    }
    if support_flag is not None:
        headers["soc/soc_caps.h"] += f"#define SOC_RMT_SUPPORTED {support_flag}\n"
    for name, contents in headers.items():
        header = tmp_path / name
        header.parent.mkdir(parents=True, exist_ok=True)
        header.write_text(contents, encoding="utf-8")
    flags: list[str] = (
        [] if override is None else [f"-DFASTLED_ESP32_HAS_RMT={override}"]
    )
    result = RunningProcess.run(
        [compiler, "-E", "-x", "c++", f"-I{tmp_path}", f"-I{SRC}", *flags, "-"],
        input=(
            '#include "platforms/esp/32/feature_flags/enabled.h"\n'
            f"#if FASTLED_ESP32_HAS_RMT != {expected}\n"
            '#error "incorrect RMT capability"\n'
            "#endif\n"
            f"#if FASTLED_RMT5 != {int(sdk_major >= 5 and expected == 1)}\n"
            '#error "incorrect RMT SDK backend selection"\n'
            "#endif\n"
        ),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    assert result.returncode == 0, result.stderr or result.stdout
