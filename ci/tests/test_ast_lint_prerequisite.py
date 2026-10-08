"""Required clang-query AST lint must fail closed, including with warm caches."""

import sys
from pathlib import Path
from types import SimpleNamespace

import pytest

from ci.lint_cpp import ast_cache, run_all_checkers
from ci.tools import check_array_params, check_ast_combined, check_noexcept


def test_subscript_operator_is_not_exempted_as_a_lambda() -> None:
    assert not check_noexcept._signature_is_exempt(
        "const int& Sample::operator[](size_t index) const {"
    )
    assert not check_noexcept._signature_is_exempt("auto fn = []() { return 1; };")


def test_missing_ast_binary_is_fatal(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(check_noexcept, "_find_clang_query", lambda: [])
    with pytest.raises(check_noexcept.NoexceptCheckError, match="not found"):
        run_all_checkers._require_ast_tool()


def test_unspawnable_uv_ast_wrapper_is_fatal(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(
        check_noexcept,
        "_find_clang_query",
        lambda: ["uv", "run", "clang-tool-chain-clang-query"],
    )
    monkeypatch.setattr(
        run_all_checkers.RunningProcess,
        "run",
        lambda *args, **kwargs: SimpleNamespace(
            returncode=1,
            stdout="",
            stderr="Failed to spawn: clang-tool-chain-clang-query",
        ),
    )
    with pytest.raises(check_noexcept.NoexceptCheckError, match="Failed to spawn"):
        run_all_checkers._require_ast_tool()


def test_combined_ast_checks_tool_before_warm_cache(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    calls: list[str] = []
    monkeypatch.setattr(ast_cache, "_CACHE_DIR", tmp_path)
    monkeypatch.setattr(ast_cache, "_compute_fingerprint", lambda *args: "fixed")

    def probe() -> None:
        calls.append("probe")
        if len(calls) == 2:
            raise check_noexcept.NoexceptCheckError("clang-query missing")

    monkeypatch.setattr(run_all_checkers, "_require_ast_tool", probe)
    monkeypatch.setattr(
        check_ast_combined, "find_combined_hits", lambda scope: ([], [])
    )

    first = run_all_checkers.run_combined_ast_check()
    assert not first[0].has_violations()
    assert not first[1].has_violations()
    assert (tmp_path / "noexcept_ast.fingerprint").exists()
    assert (tmp_path / "array_param_ast.fingerprint").exists()

    with pytest.raises(check_noexcept.NoexceptCheckError, match="missing"):
        run_all_checkers.run_combined_ast_check()
    assert calls == ["probe", "probe"]


def test_combined_ast_error_is_not_cached(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    monkeypatch.setattr(ast_cache, "_CACHE_DIR", tmp_path)
    monkeypatch.setattr(ast_cache, "_compute_fingerprint", lambda *args: "fixed")
    monkeypatch.setattr(run_all_checkers, "_require_ast_tool", lambda: None)

    def fail(scope: str) -> tuple[list[object], list[object]]:
        raise check_noexcept.NoexceptCheckError("clang-query failed")

    monkeypatch.setattr(check_ast_combined, "find_combined_hits", fail)
    with pytest.raises(check_noexcept.NoexceptCheckError, match="failed"):
        run_all_checkers.run_combined_ast_check()
    assert not list(tmp_path.iterdir())


@pytest.mark.parametrize(
    "check_name",
    ["run_noexcept_ast_check", "run_array_param_ast_check"],
)
def test_single_file_ast_error_is_fatal(
    monkeypatch: pytest.MonkeyPatch, check_name: str
) -> None:
    monkeypatch.setattr(run_all_checkers, "_require_ast_tool", lambda: None)

    def fail(scope: str) -> list[object]:
        raise check_noexcept.NoexceptCheckError("clang-query failed")

    if check_name == "run_noexcept_ast_check":
        monkeypatch.setattr(check_noexcept, "find_missing_noexcept", fail)
    else:

        def fail_array(scope: str) -> list[object]:
            raise check_array_params.ArrayParamCheckError("clang-query failed")

        monkeypatch.setattr(check_array_params, "find_decayed_array_params", fail_array)

    check = getattr(run_all_checkers, check_name)
    error_type = (
        check_noexcept.NoexceptCheckError
        if check_name == "run_noexcept_ast_check"
        else check_array_params.ArrayParamCheckError
    )
    with pytest.raises(error_type, match="failed"):
        check(
            str(run_all_checkers.PROJECT_ROOT / "src" / "fl" / "chipsets" / "hd108.h")
        )


def _missing_hit() -> check_noexcept.NoexceptHit:
    return check_noexcept.NoexceptHit(
        path="src/fl/example.h",
        line=10,
        line_text="void foo();",
        signature="void foo();",
    )


@pytest.mark.parametrize("extra_args", [[], ["--no-baseline"]])
def test_historical_baseline_cannot_hide_missing_signature(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
    extra_args: list[str],
) -> None:
    hit = _missing_hit()
    historical_baseline = tmp_path / "ci" / "tools" / "noexcept_baseline.txt"
    historical_baseline.parent.mkdir(parents=True)
    historical_baseline.write_text(hit.baseline_key + "\n")
    monkeypatch.setattr(check_noexcept, "PROJECT_ROOT", tmp_path)
    monkeypatch.setattr(check_noexcept, "find_missing_noexcept", lambda scope: [hit])
    monkeypatch.setattr(sys, "argv", ["check_noexcept.py", *extra_args])

    assert check_noexcept.main() == 1
    assert "void foo();" in capsys.readouterr().out


@pytest.mark.parametrize("option", ["--baseline", "--update-baseline"])
def test_baseline_grandfathering_options_are_rejected(
    monkeypatch: pytest.MonkeyPatch,
    option: str,
) -> None:
    monkeypatch.setattr(sys, "argv", ["check_noexcept.py", option])
    with pytest.raises(SystemExit) as failure:
        check_noexcept.main()
    assert failure.value.code == 2


def test_single_file_lint_reports_historical_missing_signature(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    hit = _missing_hit()
    monkeypatch.setattr(check_noexcept, "find_missing_noexcept", lambda scope: [hit])
    monkeypatch.setattr(run_all_checkers, "_require_ast_tool", lambda: None)
    results = run_all_checkers.run_noexcept_ast_check(
        str(run_all_checkers.PROJECT_ROOT / hit.path)
    )
    assert results.has_violations()


def test_all_scope_includes_existing_root_source_router() -> None:
    assert ("src/fl/build/src.cpp", ".*src.*") in check_noexcept._scope_tus("all")


def test_single_file_lint_checks_root_source_scope(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    scopes: list[str] = []
    hit = check_noexcept.NoexceptHit(
        path="src/FastLED.h",
        line=10,
        line_text="void show();",
        signature="void show();",
    )

    def find(scope: str) -> list[check_noexcept.NoexceptHit]:
        scopes.append(scope)
        return [hit]

    monkeypatch.setattr(check_noexcept, "find_missing_noexcept", find)
    monkeypatch.setattr(run_all_checkers, "_require_ast_tool", lambda: None)
    results = run_all_checkers.run_noexcept_ast_check(
        str(run_all_checkers.PROJECT_ROOT / hit.path)
    )
    assert scopes == ["root"]
    assert results.has_violations()


def test_combined_lint_reports_every_noexcept_hit(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    hit = _missing_hit()
    monkeypatch.setattr(run_all_checkers, "_require_ast_tool", lambda: None)
    monkeypatch.setattr(
        check_ast_combined, "find_combined_hits", lambda scope: ([hit], [])
    )
    monkeypatch.setattr(
        ast_cache, "cached_ast_check", lambda **options: options["runner"]()
    )

    noexcept_results, array_results = run_all_checkers.run_combined_ast_check()
    assert noexcept_results.has_violations()
    assert not array_results.has_violations()


@pytest.mark.parametrize("combined", [False, True])
def test_zero_exit_compiler_error_is_fatal(
    monkeypatch: pytest.MonkeyPatch, combined: bool
) -> None:
    monkeypatch.setattr(
        check_noexcept.RunningProcess,
        "run",
        lambda *args, **kwargs: SimpleNamespace(
            returncode=0,
            stdout="0 matches.\n",
            stderr="src/probe.cpp:2:6: error: exception specification does not match\n",
        ),
    )
    run = (
        check_ast_combined._run_combined_clang_query
        if combined
        else check_noexcept._run_clang_query
    )
    with pytest.raises(check_noexcept.NoexceptCheckError, match="does not match"):
        run(["clang-query"], "src/probe.cpp", ".*src.*")


def test_parser_checks_real_contracts_without_changing_production_policy() -> None:
    args = check_noexcept._COMPILER_ARGS
    assert "-DFL_NO_EXCEPT=noexcept" in args
    assert "-DFL_HAS_NOEXCEPT=1" in args
    assert "-fexceptions" in args
    assert "-fno-exceptions" not in args
    assert check_ast_combined._compiler_args is check_noexcept._compiler_args


@pytest.mark.parametrize("combined", [False, True])
def test_missing_definition_specification_is_red_then_matching_is_green(
    tmp_path: Path, combined: bool
) -> None:
    clang_query = check_noexcept._find_clang_query()
    if not clang_query:
        pytest.skip("compiler feature probe requires clang-query")
    run = (
        check_ast_combined._run_combined_clang_query
        if combined
        else check_noexcept._run_clang_query
    )
    fixture = tmp_path / "noexcept_contract.cpp"
    fixture.write_text(
        "void guaranteed() FL_NO_EXCEPT;\nvoid guaranteed() {}\n",
        encoding="utf-8",
    )
    with pytest.raises(check_noexcept.NoexceptCheckError):
        run(clang_query, str(fixture), ".*")
    fixture.write_text(
        "void guaranteed() FL_NO_EXCEPT;\nvoid guaranteed() FL_NO_EXCEPT {}\n",
        encoding="utf-8",
    )
    assert run(clang_query, str(fixture), ".*") == (([], []) if combined else [])


def test_body_annotation_cannot_exempt_enclosing_function(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
) -> None:
    source = tmp_path / "src" / "fl" / "probe.h"
    source.parent.mkdir(parents=True)
    source.write_text("void outer() { auto callback = []() FL_NO_EXCEPT {}; }\n")
    monkeypatch.setattr(check_noexcept, "PROJECT_ROOT", tmp_path)
    _line, signature = check_noexcept._read_source_signature("src/fl/probe.h", 1)
    assert signature == "void outer() {"
    assert not check_noexcept._signature_is_exempt(signature)


def test_multiline_template_definition_contract_is_checked(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
) -> None:
    source = tmp_path / "src" / "fl" / "probe.h"
    source.parent.mkdir(parents=True)
    source.write_text(
        "template<typename T>\n"
        "T transform(const T& input,\n"
        "            int scale) { return input; }\n"
    )
    monkeypatch.setattr(check_noexcept, "PROJECT_ROOT", tmp_path)
    _line, signature = check_noexcept._read_source_signature("src/fl/probe.h", 2)
    assert not check_noexcept._signature_is_exempt(signature)


def test_contract_specific_inline_suppression_remains_in_signature(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
) -> None:
    source = tmp_path / "src" / "fl" / "probe.h"
    source.parent.mkdir(parents=True)
    source.write_text("void vendor_bridge() { // ok no noexcept: vendor ABI\n}\n")
    monkeypatch.setattr(check_noexcept, "PROJECT_ROOT", tmp_path)
    _line, signature = check_noexcept._read_source_signature("src/fl/probe.h", 1)
    assert check_noexcept._signature_is_exempt(signature)


@pytest.mark.parametrize(
    "signature",
    [
        "fl::function<void() FL_NO_EXCEPT> callback_factory();",
        "void invoke(void (*callback)() FL_NO_EXCEPT);",
        "void operation() noexcept(false);",
        "void operation() FL_NO_EXCEPT;",
    ],
)
def test_annotation_tokens_cannot_suppress_semantic_throwing_finding(
    signature: str,
) -> None:
    # Annotated nothrow functions never reach this filter: the AST matcher has
    # excluded them already. Every remaining hit still needs a real contract.
    assert not check_noexcept._signature_is_exempt(signature)


def test_default_argument_lambda_cannot_exempt_enclosing_function() -> None:
    assert "isLambda()" in check_noexcept.build_query(".*src.fl.*")
    assert not check_noexcept._signature_is_exempt(
        "void outer(int value = []() { return 1; }());"
    )


def test_public_fx_headers_have_dedicated_lint_inventory() -> None:
    inventory = ("ci/tools/_noexcept_check_fx_headers_tu.cpp", ".*src.fl.fx.*")
    assert inventory in check_noexcept._scope_tus("fl")
    assert inventory in check_noexcept._scope_tus("all")
    source = (check_noexcept.PROJECT_ROOT / inventory[0]).read_text()
    for directory in ("1d", "2d"):
        for header in (check_noexcept.PROJECT_ROOT / "src/fl/fx" / directory).glob(
            "*.h"
        ):
            if header.name == "animartrix_detail.h":
                continue  # private implementation is reached through public Animartrix
            assert (
                f'#include "{header.relative_to(check_noexcept.PROJECT_ROOT / "src").as_posix()}"'
                in source
            )
    assert '#include "fl/fx/2d/animartrix.hpp"' in source


def test_public_chipsets_reuse_existing_root_lint_inventory() -> None:
    assert ("src/fl/build/src.cpp", ".*src.fl.*") in check_noexcept._scope_tus("fl")
    assert ("src/fl/build/src.cpp", ".*src.*") in check_noexcept._scope_tus("all")
    assert all(
        "chipset_headers_tu" not in path for path, _ in check_noexcept._scope_tus("all")
    )


def test_combined_inventory_deduplicates_physical_hits_across_tus(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    from ci.tools import check_ast_combined
    from ci.tools.check_array_params import ArrayParamHit

    tus = [("first.cpp", ".*"), ("second.cpp", ".*")]
    for filename, _ in tus:
        (tmp_path / filename).write_text("")
    monkeypatch.setattr(check_ast_combined, "PROJECT_ROOT", tmp_path)
    monkeypatch.setattr(
        check_ast_combined, "_find_clang_query", lambda: ["clang-query"]
    )
    monkeypatch.setattr(check_ast_combined, "_noexcept_scope_tus", lambda scope: tus)
    monkeypatch.setattr(check_ast_combined, "_array_scope_tus", lambda scope: tus)
    first = check_noexcept.NoexceptHit("src/shared.h", 10, "void f();", "void f();")
    second = check_noexcept.NoexceptHit("src/shared.h", 20, "void f();", "void f();")
    array_first = ArrayParamHit("src/shared.h", 30, "", "void g(int a[]);", ())
    array_second = ArrayParamHit("src/shared.h", 40, "", "void g(int a[]);", ())
    monkeypatch.setattr(
        check_ast_combined,
        "_run_combined_clang_query",
        lambda *args: ([first, second], [array_first, array_second]),
    )
    noexcept_hits, array_hits = check_ast_combined.find_combined_hits("all")
    assert noexcept_hits == [first, second]
    assert array_hits == [array_first, array_second]


def test_windows_friend_lookup_requires_global_fastled_qualification() -> None:
    import shutil

    from running_process import RunningProcess

    compiler = shutil.which("clang++")
    if compiler is None:
        pytest.skip("Clang required for Windows compiler-feature fixture")
    # Match the public global class and namespace friend introduced by the
    # controller header. MS lookup exposes the incomplete namespace friend.
    source = """
namespace fl {
class Controller { friend class CFastLED; };
int policy();
}
class CFastLED { public: static int channels(); };
int fl::policy() { return LOOKUP::channels(); }
"""
    for lookup, succeeds in (("CFastLED", False), ("::CFastLED", True)):
        result = RunningProcess.run(
            [
                compiler,
                "--target=x86_64-pc-windows-msvc",
                "-std=c++17",
                "-fms-extensions",
                "-fms-compatibility",
                "-fsyntax-only",
                "-x",
                "c++",
                "-",
            ],
            input=source.replace("LOOKUP", lookup),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=120,
        )
        output = (result.stdout or "") + (result.stderr or "")
        assert (result.returncode == 0) == succeeds, output
        if not succeeds:
            assert "incomplete type" in output


def test_ast_compiler_args_preserve_non_windows_profile(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(check_noexcept.sys, "platform", "linux")
    assert check_noexcept._compiler_args() == check_noexcept._COMPILER_ARGS
    assert check_noexcept._compiler_args(["-std=c++11"]) == ["-std=c++11"]


def test_windows_sleep_branch_accepts_real_noexcept() -> None:
    # Validate the real Windows call bodies with host numeric types. Selecting
    # Windows before those types would mix LLP64 declarations with a Linux SDK.
    import shutil

    from running_process import RunningProcess

    compiler = shutil.which("clang++")
    if compiler is None:
        pytest.skip("Clang required for compiler-feature fixture")
    result = RunningProcess.run(
        [
            compiler,
            *check_noexcept._compiler_args(
                ["-std=c++17", "-Isrc", "-DSTUB_PLATFORM", "-DFL_NO_EXCEPT=noexcept"]
            ),
            "-fms-extensions",
            "-fsyntax-only",
            "-x",
            "c++",
            "-",
        ],
        input=(
            '#include "fl/stl/chrono.h"\n'
            "#define FL_IS_WIN\n"
            '#include "platforms/stub/thread_stub_stl.h"\n'
            "void probe() { fl::platforms::detail::native_sleep_1ms(); "
            "fl::platforms::detail::native_sleep(fl::chrono::milliseconds(1)); }\n"
        ),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=120,
    )
    assert result.returncode == 0, (result.stdout or "") + (result.stderr or "")


def test_errno_helpers_match_nonthrowing_socket_contract() -> None:
    import shutil

    from running_process import RunningProcess

    compiler = shutil.which("clang++")
    if compiler is None:
        pytest.skip("Clang required for compiler-feature fixture")
    result = RunningProcess.run(
        [
            compiler,
            "-std=c++17",
            "-Isrc",
            "-DFL_NO_EXCEPT=noexcept",
            "-fsyntax-only",
            "-x",
            "c++",
            "-",
        ],
        input=(
            '#include "fl/stl/cerrno.h"\n'
            "namespace fl { int get_errno() noexcept; }\n"
            'static_assert(noexcept(fl::get_errno()), "get_errno contract");\n'
            'static_assert(noexcept(fl::set_errno(0)), "set_errno contract");\n'
            'static_assert(noexcept(fl::clear_errno()), "clear_errno contract");\n'
        ),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=120,
    )
    assert result.returncode == 0, (result.stdout or "") + (result.stderr or "")


def test_avr_initializer_list_fallback_has_valid_cpp11_contracts() -> None:
    import shutil

    from running_process import RunningProcess

    compiler = shutil.which("clang++")
    if compiler is None:
        pytest.skip("Clang required for AVR compiler-feature fixture")
    result = RunningProcess.run(
        [
            compiler,
            "--target=avr",
            "-mmcu=atmega328p",
            "-std=c++11",
            "-Isrc",
            "-DFL_NO_EXCEPT=noexcept",
            "-fsyntax-only",
            "-x",
            "c++",
            "-",
        ],
        input=(
            '#include "fl/stl/initializer_list.h"\n'
            "fl::initializer_list<int> values = {1, 2};\n"
            'static_assert(noexcept(values.size()), "size contract");\n'
            'static_assert(noexcept(values.empty()), "empty contract");\n'
            'static_assert(noexcept(values.begin()), "begin contract");\n'
            'static_assert(noexcept(values.end()), "end contract");\n'
        ),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=120,
    )
    assert result.returncode == 0, (result.stdout or "") + (result.stderr or "")


def test_public_stl_headers_are_in_strict_lint_inventory() -> None:
    entry = (check_noexcept._TU_STL_HEADERS, ".*src.fl.stl.*")
    assert entry in check_noexcept._scope_tus("all")
    assert entry in check_noexcept._scope_tus("fl")


def test_ast_compiler_args_select_native_windows_gnu_profile(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    from clang_tool_chain.abi import windows_gnu
    from clang_tool_chain.platform import detection

    monkeypatch.setattr(check_noexcept.sys, "platform", "win32")
    monkeypatch.setattr(detection, "get_platform_info", lambda: ("win", "x86_64"))
    calls = []

    def native_profile(platform_name, arch, args):
        calls.append((platform_name, arch, args))
        return ["--target=x86_64-w64-windows-gnu", "--sysroot=native", "-stdlib=libc++"]

    monkeypatch.setattr(windows_gnu, "_get_gnu_target_args", native_profile)
    assert check_noexcept._compiler_args(["-std=c++17"]) == [
        "-std=c++17",
        "--target=x86_64-w64-windows-gnu",
        "--sysroot=native",
        "-stdlib=libc++",
    ]
    assert calls == [("win", "x86_64", ["-std=c++17", "-c"])]


@pytest.mark.parametrize("checker", [check_noexcept, check_array_params])
def test_ast_discovery_uses_installed_query_entrypoint(
    checker, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setattr(Path, "exists", lambda path: False)
    monkeypatch.setattr(
        checker.shutil,
        "which",
        lambda name: (
            "bundled-query" if name == "clang-tool-chain-clang-query" else None
        ),
    )
    assert checker._find_clang_query() == ["bundled-query"]


def test_windows_ast_profile_failure_is_not_silently_ignored(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    from clang_tool_chain.abi import windows_gnu
    from clang_tool_chain.platform import detection

    monkeypatch.setattr(check_noexcept.sys, "platform", "win32")
    monkeypatch.setattr(detection, "get_platform_info", lambda: ("win", "x86_64"))

    def unavailable(*args):
        raise RuntimeError("missing native sysroot")

    monkeypatch.setattr(windows_gnu, "_get_gnu_target_args", unavailable)
    with pytest.raises(
        check_noexcept.NoexceptCheckError, match="missing native sysroot"
    ):
        check_noexcept._compiler_args()
