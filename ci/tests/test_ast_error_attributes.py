"""Compiler diagnostic sentinels differ from ordinary macro declarations."""

from pathlib import Path

import pytest

from ci.tools import check_array_params, check_ast_combined, check_noexcept
from ci.tools.ast_matrix import target_clang_query
from ci.tools.ast_profiles import ESP_PROFILES, load_profile, target_process_environment


@pytest.mark.parametrize("combined", [False, True], ids=["standalone", "combined"])
@pytest.mark.parametrize("contract", ["sdk_sentinel", "owned_macro"])
def test_sdk_error_sentinel_is_not_a_runtime_noexcept_contract(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, combined: bool, contract: str
) -> None:
    environment = load_profile(ESP_PROFILES[1])
    probe = tmp_path / "src" / "macro_contracts.cpp"
    probe.parent.mkdir()
    probe.write_text(
        "#include <esp_assert.h>\n"
        "#define OWNED_DECLARE(name) void name()\n"
        "OWNED_DECLARE(owned_macro_function);\n"
        "void check_sdk_assertion() noexcept {\n"
        "    (void)0; TRY_STATIC_ASSERT(true, sdk_assertion);\n"
        "}\n",
        encoding="utf-8",
    )
    for module in (check_noexcept, check_array_params, check_ast_combined):
        monkeypatch.setattr(module, "PROJECT_ROOT", tmp_path)
    query = target_clang_query()
    if combined:
        result = check_ast_combined.analyze_translation_unit(
            query,
            str(probe),
            ".*src.*",
            list(environment.arguments),
            process_env=target_process_environment(),
        )
        hits = result.noexcept_hits
    else:
        # The legacy entry point takes its settings from this accessor; supply
        # the same real target SDK configuration as the combined invocation.
        monkeypatch.setattr(
            check_noexcept, "_compiler_args", lambda: list(environment.arguments)
        )
        hits = check_noexcept._run_clang_query(query, str(probe), ".*src.*")
    if contract == "sdk_sentinel":
        # The declaration's expansion points into line 5. Signature recovery
        # can stop at the preceding semicolon, so check the AST location.
        assert not any(hit.line == 5 for hit in hits), hits
    else:
        assert any("owned_macro_function" in hit.signature for hit in hits), hits
