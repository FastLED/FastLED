"""Compile-command adaptation must preserve the real ESP preprocessing inputs."""

import hashlib
import json
import os
from pathlib import Path

import pytest

from ci.tools.ast_profiles import (
    ESP_PROFILES,
    ProfileError,
    adapt_arguments,
    load_host_profile,
    load_profile,
    load_target_query,
    parse_implicit_includes,
    project_ini,
    read_compile_command,
    target_type_definitions,
)


def test_required_profiles_are_pinned_and_use_both_architectures() -> None:
    assert {profile.idf_major for profile in ESP_PROFILES} == {4, 5}
    assert {profile.target for profile in ESP_PROFILES} == {
        "xtensa-esp32-elf",
        "riscv32-esp-elf",
    }
    for profile in ESP_PROFILES:
        ini = project_ini(profile)
        assert "latest" not in ini
        assert "greiman/SdFat@2.3.1" in ini
        # Core libraries must be selected from SDK includes, not resolved as
        # registry dependencies (FS is not a registry package).
        assert "    SD\n" not in ini and "    FS\n" not in ini


def test_adapter_preserves_macros_and_abi_and_removes_build_outputs(
    tmp_path: Path,
) -> None:
    command = [
        "riscv32-esp-elf-g++",
        "-march=rv32imac_zicsr_zifencei",
        "-mabi=ilp32",
        "-std=gnu++17",
        '-DARDUINO_BOARD=\\"ESP32C6\\"',
        "-I",
        "sdk headers",
        "-include",
        "sdkconfig.h",
        "-fno-exceptions",
        "-fstrict-volatile-bitfields",
        "-MMD",
        "-MF",
        "output.d",
        "-c",
        "input.cpp",
        "-o",
        "output.o",
    ]
    adapted = adapt_arguments(command, tmp_path, tmp_path / "input.cpp")
    assert "-march=rv32imac_zicsr_zifencei" in adapted.arguments
    assert "-mabi=ilp32" in adapted.arguments
    assert '-DARDUINO_BOARD="ESP32C6"' in adapted.arguments
    assert str(tmp_path / "sdk headers") in adapted.arguments
    assert str(tmp_path / "sdkconfig.h") in adapted.arguments
    assert "-fno-exceptions" in adapted.arguments
    assert "-fstrict-volatile-bitfields" in adapted.removed
    assert "output.o" not in adapted.arguments
    assert "input.cpp" not in adapted.arguments


def test_adapter_does_not_silently_discard_unknown_target_flags(tmp_path: Path) -> None:
    adapted = adapt_arguments(
        ["g++", "-munknown-flag", "unit.cpp"], tmp_path, tmp_path / "unit.cpp"
    )
    assert "-munknown-flag" in adapted.arguments


def test_response_file_arguments_are_included(tmp_path: Path) -> None:
    (tmp_path / "flags.rsp").write_text(
        '-I"sdk headers" -DSELECTED=1', encoding="utf-8"
    )
    adapted = adapt_arguments(
        ["g++", "@flags.rsp", "unit.cpp"], tmp_path, tmp_path / "unit.cpp"
    )
    assert "-DSELECTED=1" in adapted.arguments
    assert "-I" + str(tmp_path / "sdk headers") in adapted.arguments
    assert tmp_path / "flags.rsp" in adapted.response_files


def test_response_file_cycles_fail(tmp_path: Path) -> None:
    (tmp_path / "flags.rsp").write_text("@flags.rsp", encoding="utf-8")
    with pytest.raises(ProfileError, match="response"):
        adapt_arguments(["g++", "@flags.rsp"], tmp_path, tmp_path / "unit.cpp")


def test_portable_implicit_include_output(tmp_path: Path) -> None:
    sdk = tmp_path / "sdk headers"
    sdk.mkdir()
    output = (
        'ignoring nonexistent directory "/missing"\n'
        '#include "..." search starts here:\n'
        "#include <...> search starts here:\n"
        f" {sdk}\nEnd of search list.\n"
    )
    assert parse_implicit_includes(output) == (sdk,)


def test_incomplete_compiler_include_discovery_fails() -> None:
    with pytest.raises(ProfileError, match="include"):
        parse_implicit_includes("compiler could not start")


def test_missing_database_is_actionable(tmp_path: Path) -> None:
    with pytest.raises(ProfileError, match="provision"):
        read_compile_command(tmp_path)


def test_missing_target_query_is_actionable(tmp_path: Path) -> None:
    with pytest.raises(ProfileError, match="provision"):
        load_target_query(tmp_path)


def test_target_integer_aliases_come_from_real_compiler_macro_spellings() -> None:
    definitions = target_type_definitions(
        "#define __INT32_TYPE__ long int\n"
        "#define __UINT32_TYPE__ long unsigned int\n"
        "#define __INT_LEAST32_TYPE__ long int\n"
        "#define __UINT_FAST32_TYPE__ long unsigned int\n"
        "#define __INTPTR_TYPE__ int\n"
        "#define __SIZE_TYPE__ unsigned int\n"
        "#define __GNUC__ 14\n"
        "#define __riscv 1\n"
    )
    assert "-D__INT32_TYPE__=long int" in definitions
    assert "-U__INT32_TYPE__" in definitions
    assert "-D__UINT32_TYPE__=long unsigned int" in definitions
    assert "-D__INT_LEAST32_TYPE__=long int" in definitions
    assert "-D__UINT_FAST32_TYPE__=long unsigned int" in definitions
    assert not any("GNUC" in arg or "__riscv" in arg for arg in definitions)


def test_missing_target_integer_aliases_fail() -> None:
    with pytest.raises(ProfileError, match="type"):
        target_type_definitions("#define __GNUC__ 14\n")


def test_missing_host_profile_is_actionable(tmp_path: Path) -> None:
    with pytest.raises(ProfileError, match="provision"):
        load_host_profile(tmp_path)


def test_host_sdk_metadata_is_snapshotted_outside_include_paths(tmp_path: Path) -> None:
    from ci.lint_cpp.ast_cache import AstInputSnapshot
    from ci.tools import ast_profiles

    # Filesystem dependency fixture; no SDK declarations or compiler stand-ins.
    # Real macOS CI Clang -H reports SDKSettings.json outside usr/include.
    sdk = tmp_path / "Developer Tools" / "MacOSX15.5.sdk"
    headers = sdk / "usr" / "include"
    frameworks = sdk / "System" / "Library" / "Frameworks"
    headers.mkdir(parents=True)
    frameworks.mkdir(parents=True)
    metadata = sdk / "SDKSettings.json"
    metadata.write_text('{"Version":"15.5"}', encoding="utf-8")
    other_headers = tmp_path / "ordinary-headers"
    other_headers.mkdir()
    roots = ast_profiles.host_dependency_roots((headers, frameworks, other_headers))
    assert roots == (sdk.resolve(),)
    snapshot = AstInputSnapshot()
    snapshot.prepare_parse(list(roots), [])
    snapshot.verify_parse(list(roots), [metadata])

    stat = metadata.stat()
    metadata.write_text('{"Version":"15.6"}', encoding="utf-8")
    os.utime(metadata, ns=(stat.st_atime_ns, stat.st_mtime_ns))
    with pytest.raises(ValueError, match="changed during analysis"):
        snapshot.verify_parse(list(roots), [metadata])


def test_host_sdk_dependency_roots_reach_matrix_without_search_flags(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from ci.tools import ast_matrix, ast_profiles

    sdk = tmp_path / "MacOSX.sdk"
    sdk.mkdir()
    headers = sdk / "usr" / "include"
    headers.mkdir(parents=True)
    host = ast_profiles.HostEnvironment(
        query=("clang-query",),
        version="test host description",
        arguments=("-std=c++17",),
        include_paths=(headers,),
        identity_files=(),
        dependency_roots=(sdk,),
    )
    monkeypatch.setattr(ast_matrix, "load_host_profile", lambda: host)
    monkeypatch.setattr(
        ast_matrix, "_scope_tus", lambda _: [("src/fl/build/src.cpp", "src/.*")]
    )
    jobs = ast_matrix.build_jobs(profile_names=())
    assert len(jobs) == 1
    assert jobs[0].dependency_roots == (sdk,)
    assert jobs[0].arguments == host.arguments
    assert sdk not in jobs[0].include_roots


@pytest.mark.parametrize(
    "changed", ["database", "response", "compiler", "predefines", "adapter"]
)
def test_changed_provisioning_derivation_requires_reprovision(
    tmp_path: Path, changed: str
) -> None:
    from ci.tools import ast_profiles

    # Reuse actual SDK metadata; only provenance inputs get disposable copies.
    # No fabricated vendor headers and no edits to the installed toolchain.
    original = ast_profiles.PROFILE_ROOT / ESP_PROFILES[0].name
    payload = json.loads((original / ast_profiles.MANIFEST_NAME).read_text())
    payload["schema"] = ast_profiles.SCHEMA_VERSION
    payload["environment"] = ast_profiles._include_environment(target=True)
    payload.setdefault("dependency_roots", payload["include_paths"])
    (tmp_path / "platformio.ini").write_text(
        project_ini(ESP_PROFILES[0]), encoding="utf-8"
    )
    sources = {
        "database": original / ".fbuild/build/release/compile_commands.raw.json",
        "response": original / "platformio.ini",
        "compiler": ast_profiles.PROJECT_ROOT / "lint",
        "predefines": original / "compiler-predefines.txt",
        "adapter": Path(ast_profiles.__file__),
    }
    inputs = {}
    for name, source in sources.items():
        copy = tmp_path / name
        copy.write_bytes(source.read_bytes())
        inputs[str(copy)] = hashlib.sha256(copy.read_bytes()).hexdigest()
    payload["derivation_inputs"] = inputs
    payload["compiler"] = str(tmp_path / "compiler")
    (tmp_path / ast_profiles.MANIFEST_NAME).write_text(
        json.dumps(payload), encoding="utf-8"
    )
    load_profile(ESP_PROFILES[0], tmp_path)
    changed_file = tmp_path / changed
    stat = changed_file.stat()
    contents = changed_file.read_bytes()
    changed_file.write_bytes(bytes([contents[0] ^ 1]) + contents[1:])
    os.utime(changed_file, ns=(stat.st_atime_ns, stat.st_mtime_ns))
    with pytest.raises(ProfileError, match="provision"):
        load_profile(ESP_PROFILES[0], tmp_path)


def test_changed_host_query_binary_requires_reprovision(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from ci.tools import ast_profiles, check_noexcept

    payload = json.loads(
        (ast_profiles.PROFILE_ROOT / ast_profiles.HOST_MANIFEST_NAME).read_text()
    )
    payload["schema"] = ast_profiles.SCHEMA_VERSION
    payload["environment"] = ast_profiles._include_environment()
    binary = tmp_path / "query"
    binary.write_bytes((ast_profiles.PROJECT_ROOT / "lint").read_bytes())
    payload["query"] = [str(binary)]
    payload["executable"] = str(binary)
    payload["query_files"] = [str(binary)]
    payload["derivation_inputs"] = {
        str(binary): hashlib.sha256(binary.read_bytes()).hexdigest()
    }
    monkeypatch.setattr(check_noexcept, "_find_clang_query", lambda: [str(binary)])
    (tmp_path / ast_profiles.HOST_MANIFEST_NAME).write_text(
        json.dumps(payload), encoding="utf-8"
    )
    load_host_profile(tmp_path)
    binary.write_bytes(binary.read_bytes() + b"\n")
    with pytest.raises(ProfileError, match="provision"):
        load_host_profile(tmp_path)


def test_changed_target_query_binary_requires_reprovision(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    import clang_tool_chain.path_utils
    from clang_tool_chain.platform.detection import get_platform_info

    from ci.tools import ast_profiles

    payload = json.loads(
        (ast_profiles.PROFILE_ROOT / ast_profiles.QUERY_MANIFEST_NAME).read_text()
    )
    payload["schema"] = ast_profiles.SCHEMA_VERSION
    platform, _ = get_platform_info()
    binary = (
        tmp_path / "bin" / ("clang-query.exe" if platform == "win" else "clang-query")
    )
    binary.parent.mkdir()
    binary.write_bytes((ast_profiles.PROJECT_ROOT / "lint").read_bytes())
    payload["executable"] = str(binary)
    payload["derivation_inputs"] = {
        str(binary): hashlib.sha256(binary.read_bytes()).hexdigest()
    }
    monkeypatch.setattr(
        clang_tool_chain.path_utils, "get_clang_extra_install_dir", lambda *_: tmp_path
    )
    (tmp_path / ast_profiles.QUERY_MANIFEST_NAME).write_text(
        json.dumps(payload), encoding="utf-8"
    )
    load_target_query(tmp_path)
    binary.write_bytes(binary.read_bytes() + b"\n")
    with pytest.raises(ProfileError, match="provision"):
        load_target_query(tmp_path)


def test_target_discovery_excludes_nonstandard_host_include_environment(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from ci.tools import ast_profiles

    environment = load_profile(ESP_PROFILES[0])
    host_headers = tmp_path / "nonstandard-host-sdk"
    host_headers.mkdir()
    monkeypatch.setenv("CPATH", str(host_headers))
    monkeypatch.setenv("CPLUS_INCLUDE_PATH", str(host_headers))
    monkeypatch.setenv("SDKROOT", str(host_headers))
    process_env = ast_profiles.target_process_environment()
    assert "CPATH" not in process_env
    assert "CPLUS_INCLUDE_PATH" not in process_env
    assert "SDKROOT" not in process_env
    assert process_env.get("PATH") == os.environ.get("PATH")
    output = ast_profiles._run(
        [str(environment.compiler), "-E", "-x", "c++", "-v", "-"],
        tmp_path,
        process_env=process_env,
    )
    assert host_headers.resolve() not in parse_implicit_includes(output)
    # Host commands keep their real user-selected include environment.
    host_output = ast_profiles._run(
        [str(environment.compiler), "-E", "-x", "c++", "-v", "-"], tmp_path
    )
    assert host_headers.resolve() in parse_implicit_includes(host_output)


def test_dependency_roots_cover_sibling_sdk_headers_without_new_search_paths(
    tmp_path: Path,
) -> None:
    from ci.tools.ast_profiles import dependency_roots

    installed = tmp_path / "installed-framework"
    sdk_headers = installed / "tools" / "sdk" / "esp32" / "include"
    search_paths = (sdk_headers / "bt" / "include", sdk_headers / "driver" / "include")
    receipt = json.dumps(
        {
            "environments": [
                {
                    "env": "esp",
                    "packages": [{"kind": "framework", "install_path": str(installed)}],
                }
            ]
        }
    )
    roots = dependency_roots(receipt, "esp", search_paths)
    assert roots == (sdk_headers,)
    relative_header = sdk_headers / "bt" / "controller" / "chip" / "esp_bt_cfg.h"
    assert any(relative_header.is_relative_to(root) for root in roots)
    assert not any(relative_header.is_relative_to(root) for root in search_paths)
    assert installed not in roots  # Excludes package binaries and static archives.
