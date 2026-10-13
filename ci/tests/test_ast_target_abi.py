"""The AST target must preserve the real SDK compiler's C++ type ABI."""

import re
from pathlib import Path

import pytest

from ci.tools import check_ast_combined
from ci.tools.ast_matrix import target_clang_query
from ci.tools.ast_profiles import (
    ESP_PROFILES,
    EspProfile,
    ProfileError,
    _run,
    load_profile,
    target_abi_arguments,
    target_process_environment,
    validate_target_abi,
)


@pytest.mark.parametrize("profile", ESP_PROFILES, ids=lambda profile: profile.name)
def test_ast_builtin_types_match_real_sdk_compiler(
    tmp_path: Path, profile: EspProfile
) -> None:
    environment = load_profile(profile)
    macros = dict(
        re.findall(
            r"^#define (\w+) (.+)$",
            (environment.project_dir / "compiler-predefines.txt").read_text(
                encoding="utf-8"
            ),
            re.MULTILINE,
        )
    )
    contracts = [
        "#include <stdint.h>",
        "#include <stddef.h>",
        "template<class A, class B> struct same { static constexpr bool value = false; };",
        "template<class A> struct same<A, A> { static constexpr bool value = true; };",
    ]
    for spelling, macro in (
        ("int", "__SIZEOF_INT__"),
        ("long", "__SIZEOF_LONG__"),
        ("void*", "__SIZEOF_POINTER__"),
        ("size_t", "__SIZEOF_SIZE_T__"),
        ("intptr_t", "__SIZEOF_POINTER__"),
        ("int32_t", "__SIZEOF_INT__"),
        ("wchar_t", "__SIZEOF_WCHAR_T__"),
        ("long double", "__SIZEOF_LONG_DOUBLE__"),
    ):
        contracts.append(
            f'static_assert(sizeof({spelling}) == {macros[macro]}, "{spelling} size");'
        )
    for spelling, unsigned in (
        ("int", False),
        ("long", False),
        ("size_t", True),
        ("intptr_t", False),
        ("int32_t", False),
        ("wchar_t", "__WCHAR_UNSIGNED__" in macros),
        ("char", "__CHAR_UNSIGNED__" in macros),
    ):
        contracts.append(
            f"static_assert((static_cast<{spelling}>(-1) > 0) == "
            f'{str(unsigned).lower()}, "{spelling} signedness");'
        )
    for spelling, macro in (
        ("size_t", "__SIZE_TYPE__"),
        ("intptr_t", "__INTPTR_TYPE__"),
        ("int32_t", "__INT32_TYPE__"),
    ):
        contracts.append(
            f'static_assert(same<{spelling}, {macros[macro]}>::value, "{spelling} alias");'
        )
    contracts.append(
        'static_assert(same<size_t, decltype(sizeof(0))>::value, "builtin size_t");'
    )
    probe = tmp_path / "target_abi.cpp"
    probe.write_text("\n".join(contracts) + "\n", encoding="utf-8")
    abi_arguments = [
        argument
        for argument in environment.arguments
        if argument.startswith(("-march=", "-mabi=", "-mcpu="))
    ]
    # First establish that the assertions describe the actual GCC types,
    # without any of the Clang adapter's preprocessor alias overrides.
    _run(
        [
            str(environment.compiler),
            *abi_arguments,
            "-std=c++17",
            "-fsyntax-only",
            str(probe),
        ],
        tmp_path,
        process_env=target_process_environment(),
    )
    # sizeof and comparisons exercise builtin C++ semantics; redefining
    # __WCHAR_TYPE__ or __SIZEOF_WCHAR_T__ cannot make this probe pass.
    check_ast_combined.analyze_translation_unit(
        target_clang_query(),
        str(probe),
        ".*",
        list(environment.arguments),
        process_env=target_process_environment(),
    )


def test_provisioning_rejects_actual_builtin_abi_mismatch(tmp_path: Path) -> None:
    environment = load_profile(ESP_PROFILES[0])
    macros = (environment.project_dir / "compiler-predefines.txt").read_text(
        encoding="utf-8"
    )
    # A real Clang flag deliberately restores the original broken semantics;
    # all real SDK headers and alias definitions remain present.
    with pytest.raises(ProfileError, match=r"Target C\+\+ ABI validation failed"):
        validate_target_abi(
            tmp_path, macros, (*environment.arguments, "-fno-short-wchar")
        )


def test_unsupported_wchar_abi_is_not_guessed() -> None:
    with pytest.raises(ProfileError, match="Unsupported target wchar_t ABI"):
        target_abi_arguments(
            "#define __SIZEOF_WCHAR_T__ 4\n#define __WCHAR_UNSIGNED__ 1\n"
        )
