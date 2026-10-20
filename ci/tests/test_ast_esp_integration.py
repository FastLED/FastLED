"""Real SDK regressions for branches that disappear from the host AST."""

import re
from pathlib import Path

import pytest

from ci.tools import check_array_params, check_ast_combined, check_noexcept
from ci.tools.ast_profiles import ESP_PROFILES, EspProfile, load_profile
from ci.util.paths import PROJECT_ROOT


def test_riscv_interrupt_definition_requires_matching_noexcept(tmp_path: Path) -> None:
    """Reproduce #4869 with the real C6 SDK and the shipped implementation."""
    from ci.tools.ast_matrix import target_clang_query

    profile = next(p for p in ESP_PROFILES if p.name == "esp32c6-idf5")
    environment = load_profile(profile)
    source = PROJECT_ROOT / "src/platforms/esp/32/interrupts/riscv.cpp.hpp"
    original = source.read_text(encoding="utf-8")
    assert original.count(") FL_NO_EXCEPT {") >= 1
    overlay = tmp_path / "riscv.cpp.hpp"
    probe = tmp_path / "probe.cpp"
    probe.write_text(f'#include "{overlay.as_posix()}"\n', encoding="utf-8")
    expected = ("fastled_riscv_install_interrupt()",)
    overlay.write_text(original.replace(") FL_NO_EXCEPT {", ") {", 1), encoding="utf-8")
    with pytest.raises(
        check_noexcept.NoexceptCheckError, match="exception specification"
    ):
        check_ast_combined.analyze_translation_unit(
            target_clang_query(),
            str(probe),
            ".*",
            list(environment.arguments),
            expected_records=expected,
        )
    overlay.write_text(original, encoding="utf-8")
    result = check_ast_combined.analyze_translation_unit(
        target_clang_query(),
        str(probe),
        ".*",
        list(environment.arguments),
        expected_records=expected,
    )
    assert result.present_records == list(expected)


def test_absent_real_spi_dependency_cannot_pass_as_sd_coverage() -> None:
    from ci.tools.ast_matrix import target_clang_query

    environment = load_profile(ESP_PROFILES[0])
    spi_paths = {
        path for path in environment.include_paths if (path / "SPI.h").is_file()
    }
    assert spi_paths, "provisioned profile must first contain real SPI headers"
    removed = {f"-I{path}" for path in spi_paths}
    arguments = [
        argument for argument in environment.arguments if argument not in removed
    ]
    assert len(arguments) < len(environment.arguments)
    with pytest.raises(
        check_noexcept.NoexceptCheckError, match="expected backend definitions missing"
    ):
        check_ast_combined.analyze_translation_unit(
            target_clang_query(),
            str(PROJECT_ROOT / "src/fl/build/fl.fs.sd+.cpp"),
            ".*src.*",
            arguments,
            expected_records=("fl::SDFileHandle",),
        )


@pytest.mark.parametrize("profile", ESP_PROFILES, ids=lambda profile: profile.name)
@pytest.mark.parametrize("backend", ["sd", "sdfat"])
def test_original_sd_override_regression_with_real_sdk(
    tmp_path: Path, profile: EspProfile, backend: str
) -> None:
    from ci.tools.ast_matrix import target_clang_query

    environment = load_profile(profile)
    relative = Path("fl/fs/sd/fs_sdcard_arduino.cpp.hpp")
    original = (PROJECT_ROOT / "src" / relative).read_text(encoding="utf-8")
    broken, replacements = re.subn(
        r"((?:has_error|clear_error|error_code|error_message)\(\)(?: const)?) FL_NO_EXCEPT override",
        r"\1 override",
        original,
    )
    assert replacements == 8, "the motivating eight declarations must all be tested"
    overlay = tmp_path / relative
    overlay.parent.mkdir(parents=True)
    probe = tmp_path / "probe.cpp"
    record = "fl::SDFileHandle" if backend == "sd" else "fl::SdFatFileHandle"
    probe.write_text(
        f'#include "{relative.as_posix()}"\n'
        f'static_assert(sizeof({record}) > 0, "required backend");\n',
        encoding="utf-8",
    )
    arguments = [f"-I{tmp_path}", *environment.backend_arguments(backend)]
    query = target_clang_query()
    overlay.write_text(broken, encoding="utf-8")
    with pytest.raises(
        check_noexcept.NoexceptCheckError, match="exception specification"
    ):
        check_ast_combined.analyze_translation_unit(
            query, str(probe), ".*", arguments, expected_records=(record,)
        )
    overlay.write_text(original, encoding="utf-8")
    result = check_ast_combined.analyze_translation_unit(
        query, str(probe), ".*", arguments, expected_records=(record,)
    )
    assert result.present_records == [record]
    assert overlay.resolve() in result.dependencies


@pytest.mark.parametrize("profile", ESP_PROFILES, ids=lambda profile: profile.name)
def test_esp_only_function_is_checked_where_stub_excludes_it(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, profile: EspProfile
) -> None:
    from ci.tools.ast_matrix import target_clang_query

    environment = load_profile(profile)
    relative = Path("src/platforms/esp/32/drivers/spi_hw_manager_esp32.cpp.hpp")
    original = (PROJECT_ROOT / relative).read_text(encoding="utf-8")
    signature = "static void addSpiHw1IfPossible() FL_NO_EXCEPT"
    assert original.count(signature) == 1
    overlay = tmp_path / relative
    overlay.parent.mkdir(parents=True)
    overlay.write_text(
        original.replace(signature, signature.replace(" FL_NO_EXCEPT", "")),
        encoding="utf-8",
    )
    probe = tmp_path / "probe.cpp"
    probe.write_text(f'#include "{overlay.as_posix()}"\n', encoding="utf-8")
    # Read diagnostics from the copied real implementation while keeping all
    # dependencies in the actual checkout/SDK. No vendor declarations are mocked.
    for module in (check_ast_combined, check_noexcept, check_array_params):
        monkeypatch.setattr(module, "PROJECT_ROOT", tmp_path)
    query = check_noexcept._find_clang_query()
    assert query
    stub_args = [
        arg for arg in check_noexcept._compiler_args() if not arg.startswith("-I")
    ] + [f"-I{PROJECT_ROOT / 'src'}", f"-I{PROJECT_ROOT / 'src/platforms/stub'}"]
    stub = check_ast_combined.analyze_translation_unit(
        query, str(probe), ".*", stub_args
    )
    assert not stub.coverage
    target_query = target_clang_query()
    broken = check_ast_combined.analyze_translation_unit(
        target_query, str(probe), ".*", list(environment.arguments)
    )
    assert any("addSpiHw1IfPossible" in hit.signature for hit in broken.noexcept_hits)
    overlay.write_text(original, encoding="utf-8")
    fixed = check_ast_combined.analyze_translation_unit(
        target_query, str(probe), ".*", list(environment.arguments)
    )
    assert not fixed.noexcept_hits
    assert any(location.path == relative.as_posix() for location in fixed.coverage)
