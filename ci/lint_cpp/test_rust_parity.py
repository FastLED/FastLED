#!/usr/bin/env python3
"""Parity tests for the Rust C++ linter checker subset."""

from __future__ import annotations

from pathlib import Path
from typing import Any

import pytest
from running_process import RunningProcess

from ci.lint_cpp import (
    cpp_hpp_header_pair_checker,
    cpp_hpp_includes_checker,
    fastled_header_usage_checker,
    headers_exist_checker,
    include_paths_checker,
    is_header_include_checker,
    iwyu_pragma_block_checker,
    namespace_platforms_checker,
    native_platform_defines_checker,
    no_namespace_fl_declaration,
    relative_include_checker,
    serial_printf_checker,
    test_aggregation_checker,
    test_include_paths_checker,
    test_path_structure_checker,
    unit_test_checker,
    using_namespace_fl_in_examples_checker,
)
from ci.lint_cpp.arduino_macro_usage_checker import ArduinoMacroUsageChecker
from ci.lint_cpp.asm_js_location_checker import AsmJsLocationChecker
from ci.lint_cpp.attribute_checker import AttributeChecker
from ci.lint_cpp.banned_define_checker import BannedDefineChecker
from ci.lint_cpp.banned_headers_checker import BANNED_HEADERS_CORE, BannedHeadersChecker
from ci.lint_cpp.banned_macros_checker import BannedMacrosChecker
from ci.lint_cpp.banned_namespace_checker import BannedNamespaceChecker
from ci.lint_cpp.bare_allocation_checker import BareAllocationChecker
from ci.lint_cpp.bare_using_checker import BareUsingChecker
from ci.lint_cpp.builtin_memcpy_checker import BuiltinMemcpyChecker
from ci.lint_cpp.check_namespace_includes import NamespaceIncludesChecker
from ci.lint_cpp.check_platform_includes import PlatformTrampolineChecker
from ci.lint_cpp.check_platforms_fl_namespace import PlatformsFlNamespaceChecker
from ci.lint_cpp.check_using_namespace import UsingNamespaceChecker
from ci.lint_cpp.cpp_hpp_header_pair_checker import CppHppHeaderPairChecker
from ci.lint_cpp.cpp_hpp_includes_checker import CppHppIncludesChecker
from ci.lint_cpp.cpp_include_checker import CppIncludeChecker
from ci.lint_cpp.ctype_global_checker import CtypeGlobalChecker
from ci.lint_cpp.enum_class_checker import EnumClassChecker
from ci.lint_cpp.esp_rom_printf_checker import EspRomPrintfChecker
from ci.lint_cpp.example_serial_checker import ExampleSerialChecker
from ci.lint_cpp.fastled_header_usage_checker import FastLEDHeaderUsageChecker
from ci.lint_cpp.fl_is_defined_checker import FlIsDefinedChecker
from ci.lint_cpp.headers_exist_checker import HeadersExistChecker
from ci.lint_cpp.impl_hpp_includes_checker import ImplHppIncludesChecker
from ci.lint_cpp.include_after_namespace_checker import IncludeAfterNamespaceChecker
from ci.lint_cpp.include_paths_checker import IncludePathsChecker
from ci.lint_cpp.is_header_include_checker import IsHeaderIncludeChecker
from ci.lint_cpp.iwyu_pragma_block_checker import IwyuPragmaBlockChecker
from ci.lint_cpp.logging_in_iram_checker import LoggingInIramChecker
from ci.lint_cpp.member_style_checker import MemberStyleChecker
from ci.lint_cpp.namespace_platforms_checker import NamespacePlatformsChecker
from ci.lint_cpp.native_platform_defines_checker import NativePlatformDefinesChecker
from ci.lint_cpp.no_namespace_fl_declaration import NamespaceFlDeclarationChecker
from ci.lint_cpp.no_using_namespace_fl_in_headers import UsingNamespaceFlChecker
from ci.lint_cpp.noexcept_special_members_checker import NoexceptSpecialMembersChecker
from ci.lint_cpp.numeric_limit_macros_checker import NumericLimitMacroChecker
from ci.lint_cpp.platform_includes_checker import PlatformIncludesChecker
from ci.lint_cpp.platform_pragma_checker import PlatformPragmaChecker
from ci.lint_cpp.pragma_once_checker import PragmaOnceChecker
from ci.lint_cpp.raw_noexcept_checker import RawNoexceptChecker
from ci.lint_cpp.raw_pragma_checker import RawPragmaChecker
from ci.lint_cpp.reinterpret_cast_checker import ReinterpretCastChecker
from ci.lint_cpp.relative_include_checker import RelativeIncludeChecker
from ci.lint_cpp.run_all_checkers import _legacy_violation_item_to_line_content
from ci.lint_cpp.rust_bridge import parse_rust_json_output
from ci.lint_cpp.serial_printf_checker import SerialPrintfChecker
from ci.lint_cpp.simd_intrinsics_checker import SimdIntrinsicsChecker
from ci.lint_cpp.singleton_in_headers_checker import SingletonInHeadersChecker
from ci.lint_cpp.sleep_for_checker import SleepForChecker
from ci.lint_cpp.span_from_pointer_checker import SpanFromPointerChecker
from ci.lint_cpp.static_in_headers_checker import StaticInHeaderChecker
from ci.lint_cpp.std_namespace_checker import StdNamespaceChecker
from ci.lint_cpp.stdint_type_checker import StdintTypeChecker
from ci.lint_cpp.subdir_namespace_checker import SubdirNamespaceChecker
from ci.lint_cpp.test_aggregation_checker import (
    TestAggregationChecker as AggregationChecker,
)
from ci.lint_cpp.test_include_paths_checker import (
    TestIncludePathsChecker as IncludePathStyleChecker,
)
from ci.lint_cpp.test_path_structure_checker import (
    TestPathStructureChecker as MirrorPathChecker,
)
from ci.lint_cpp.thread_local_keyword_checker import ThreadLocalKeywordChecker
from ci.lint_cpp.unit_test_checker import UnitTestChecker
from ci.lint_cpp.using_namespace_fl_in_examples_checker import (
    UsingNamespaceFlInExamplesChecker,
)
from ci.lint_cpp.weak_attribute_checker import WeakAttributeChecker
from ci.util.check_files import CheckerResults, FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


def _normalize_path(path: Path | str) -> str:
    return str(path).replace("\\", "/")


def _python_records(
    checker: FileContentChecker, path: Path, code: str
) -> list[dict[str, Any]]:
    file_path = str(path)
    if not checker.should_process_file(file_path):
        return []

    checker.check_file_content(
        FileContent(path=file_path, content=code, lines=code.splitlines())
    )
    violations_by_path = getattr(checker, "violations", {})
    if isinstance(violations_by_path, CheckerResults):
        file_violations = violations_by_path.violations.get(
            file_path, violations_by_path.violations.get(_normalize_path(file_path))
        )
        if file_violations is None:
            return []
        records: list[dict[str, Any]] = []
        for violation in file_violations.violations:
            records.append(
                {
                    "checker": checker.__class__.__name__,
                    "path": _normalize_path(file_path),
                    "line": violation.line_number,
                    "message": violation.content,
                }
            )
        return records
    violations = violations_by_path.get(
        file_path, violations_by_path.get(_normalize_path(file_path), [])
    )
    if isinstance(violations, str):
        return [
            {
                "checker": checker.__class__.__name__,
                "path": _normalize_path(file_path),
                "line": 0,
                "message": violations,
            }
        ]
    records = []
    for item in violations:
        converted = _legacy_violation_item_to_line_content(item)
        records.append(
            {
                "checker": checker.__class__.__name__,
                "path": _normalize_path(file_path),
                "line": converted.line_number,
                "message": converted.message,
            }
        )
    return records


def _rust_records(
    checker_key: str, project_root: Path, path: Path
) -> list[dict[str, Any]]:
    result = RunningProcess.run(
        [
            "soldr",
            "cargo",
            "run",
            "--release",
            "--bin",
            "fastled-lint",
            "--manifest-path",
            "ci/lint_cpp_rs/Cargo.toml",
            "--",
            "--checker",
            checker_key,
            "--format",
            "json",
            "--project-root",
            str(project_root),
            str(path),
        ],
        cwd=str(PROJECT_ROOT),
        capture_output=True,
        check=False,
        timeout=300,
    )
    assert result.returncode in (0, 1), result.stderr
    records = parse_rust_json_output(result.stdout)
    for record in records:
        record["path"] = _normalize_path(record["path"])
    return records


@pytest.mark.parametrize(
    ("checker_key", "checker", "relative_path", "code"),
    [
        (
            "arduino_macro_usage",
            ArduinoMacroUsageChecker(),
            Path("src/fl/example.h"),
            "int mode = INPUT;\n",
        ),
        (
            "asm_js_location",
            AsmJsLocationChecker(),
            Path("src/fl/example.h"),
            "EM_JS(void, demo, (), {});\n",
        ),
        (
            "attribute",
            AttributeChecker(),
            Path("src/fl/example.h"),
            "[[nodiscard]] int value();\n",
        ),
        (
            "banned_headers",
            BannedHeadersChecker(
                banned_headers_list=BANNED_HEADERS_CORE, strict_mode=True
            ),
            Path("src/fl/example.h"),
            "#include <vector>\n",
        ),
        (
            "banned_define",
            BannedDefineChecker(),
            Path("src/fl/example.h"),
            "#if ESP32\n#endif\n",
        ),
        (
            "banned_macros",
            BannedMacrosChecker(),
            Path("src/fl/example.h"),
            'static_assert(sizeof(int) > 0, "ok");\n',
        ),
        (
            "banned_macros",
            BannedMacrosChecker(),
            Path("src/fl/example.h"),
            'FL_WARN("use static_assert in docs only");\n',
        ),
        (
            "banned_namespace",
            BannedNamespaceChecker(),
            Path("src/fl/example.h"),
            "fl::fl::u8 value;\n",
        ),
        (
            "builtin_memcpy",
            BuiltinMemcpyChecker(),
            Path("src/fl/example.h"),
            "__builtin_memcpy(dst, src, n);\n",
        ),
        (
            "cpp_hpp_includes",
            CppHppIncludesChecker(),
            Path("src/fl/example.h"),
            '#include "fl/foo.cpp.hpp"\n',
        ),
        (
            "cpp_hpp_header_pair",
            CppHppHeaderPairChecker(),
            Path("src/fl/example.cpp.hpp"),
            "int value;\n",
        ),
        (
            "cpp_include",
            CppIncludeChecker(),
            Path("src/fl/example.h"),
            '#include "foo.cpp"\n',
        ),
        (
            "ctype_global",
            CtypeGlobalChecker(),
            Path("src/fl/example.h"),
            "strlen(value);\n",
        ),
        (
            "enum_class",
            EnumClassChecker(),
            Path("src/fl/example.h"),
            "enum PlainEnum {\n    Value,\n};\n",
        ),
        (
            "esp_rom_printf",
            EspRomPrintfChecker(),
            Path("src/fl/example.h"),
            'esp_rom_printf("x");\n',
        ),
        (
            "example_serial",
            ExampleSerialChecker(),
            Path("examples/AutoResearch/AutoResearch.ino"),
            "void setup() {\n    Serial.begin(115200);\n}\n",
        ),
        (
            "fastled_header_usage",
            FastLEDHeaderUsageChecker(),
            Path("src/fl/example.h"),
            '#include "FastLED.h"\n',
        ),
        (
            "fastled_header_usage",
            FastLEDHeaderUsageChecker(),
            Path("src/fl/example.h"),
            '/*\n#include "FastLED.h"\n*/\n',
        ),
        (
            "fl_is_defined",
            FlIsDefinedChecker(),
            Path("src/fl/example.h"),
            "#if FL_IS_ESP32\n#endif\n",
        ),
        (
            "headers_exist",
            HeadersExistChecker(),
            Path("tests/fl/missing.cpp"),
            'TEST_CASE("missing") {}\n',
        ),
        (
            "include_paths",
            IncludePathsChecker(),
            Path("src/fl/example.h"),
            '#include "../foo.h"\n',
        ),
        (
            "include_after_namespace",
            IncludeAfterNamespaceChecker(),
            Path("src/fl/example.h"),
            'namespace fl {\n#include "fl/late.h"\n}\n',
        ),
        (
            "include_paths",
            IncludePathsChecker(),
            Path("src/fl/example.h"),
            '#include "fl/missing.h"\n',
        ),
        (
            "impl_hpp_includes",
            ImplHppIncludesChecker(),
            Path("src/fl/example.h"),
            '#include "platforms/foo.impl.hpp"\n',
        ),
        (
            "is_header_include",
            IsHeaderIncludeChecker(),
            Path("src/platforms/example.h"),
            "#if FL_IS_ESP32\n#endif\n",
        ),
        (
            "iwyu_pragma_block",
            IwyuPragmaBlockChecker(),
            Path("src/fl/example.h"),
            "#include <driver/rmt.h>\n",
        ),
        (
            "bare_allocation",
            BareAllocationChecker(),
            Path("src/fl/example.h"),
            "void* p = malloc(4);\nvoid* q = fl::malloc(4);\n",
        ),
        (
            "bare_using",
            BareUsingChecker(),
            Path("src/fl/example.h"),
            "using foo::bar;\n",
        ),
        (
            "logging_in_iram",
            LoggingInIramChecker(),
            Path("src/fl/example.h"),
            'FL_IRAM void isr() {\n    FL_WARN("x");\n}\n',
        ),
        (
            "logging_in_iram",
            LoggingInIramChecker(),
            Path("src/fl/example.h"),
            '/*\ncomment\n*/\nFL_IRAM void isr() {\n    FL_WARN("x");\n}\n',
        ),
        (
            "member_style",
            MemberStyleChecker(),
            Path("src/fl/example.h"),
            "class Foo {\n    int state_;\n};\n",
        ),
        (
            "namespace_fl_declaration",
            NamespaceFlDeclarationChecker(),
            Path("src/example.h"),
            "namespace fl {\n}\n",
        ),
        (
            "namespace_includes",
            NamespaceIncludesChecker(),
            Path("src/fl/example.h"),
            'namespace fl {\n}\n#include "fl/late.h"\n',
        ),
        (
            "namespace_platforms",
            NamespacePlatformsChecker(),
            Path("src/platforms/example.h"),
            "namespace platform {\n}\n",
        ),
        (
            "native_platform_defines",
            NativePlatformDefinesChecker(),
            Path("src/fl/example.h"),
            "#ifdef ESP32\n#endif\n",
        ),
        (
            "noexcept_special_members",
            NoexceptSpecialMembersChecker(),
            Path("src/fl/example.h"),
            "class Foo {\npublic:\n    Foo();\n};\n",
        ),
        (
            "numeric_limit_macros",
            NumericLimitMacroChecker(),
            Path("src/fl/example.h"),
            "auto value = UINT32_MAX;\n",
        ),
        (
            "platform_includes",
            PlatformIncludesChecker(),
            Path("src/fl/example.h"),
            '#include "platforms/shared/int_windows.h"\n',
        ),
        (
            "platforms_fl_namespace",
            PlatformsFlNamespaceChecker(),
            Path("src/platforms/example.h"),
            "void value();\n",
        ),
        (
            "pragma_once",
            PragmaOnceChecker(),
            Path("src/fl/example.h"),
            "int value;\n",
        ),
        (
            "platform_pragma",
            PlatformPragmaChecker(),
            Path("src/fl/example.h"),
            '#pragma GCC diagnostic ignored "-Wfoo"\n',
        ),
        (
            "platform_trampoline",
            PlatformTrampolineChecker(),
            Path("src/fl/example.h"),
            '#include "platforms/shared/int_windows.h"\n',
        ),
        (
            "raw_noexcept",
            RawNoexceptChecker(),
            Path("src/fl/example.h"),
            "void value() noexcept;\n",
        ),
        (
            "raw_pragma",
            RawPragmaChecker(),
            Path("src/fl/example.h"),
            '_Pragma("GCC diagnostic ignored \\"-Wfoo\\"")\n',
        ),
        (
            "reinterpret_cast",
            ReinterpretCastChecker(),
            Path("src/fl/example.h"),
            "auto value = reinterpret_cast<int*>(ptr);\n",
        ),
        (
            "relative_include",
            RelativeIncludeChecker(),
            Path("src/fl/example.h"),
            '#include "../foo.h"\n',
        ),
        (
            "relative_include",
            RelativeIncludeChecker(),
            Path("src/fl/example.h"),
            '/*\n#include "../foo.h"\n*/\n',
        ),
        (
            "static_in_headers",
            StaticInHeaderChecker(),
            Path("src/fl/example.h"),
            "int getAll() {\n    static int instances = 0;\n    return instances;\n}\n",
        ),
        (
            "sleep_for",
            SleepForChecker(),
            Path("src/fl/example.h"),
            "std::this_thread::sleep_for(ms);\n",
        ),
        (
            "singleton_in_headers",
            SingletonInHeadersChecker(),
            Path("src/fl/example.h"),
            "auto& value = Singleton<Foo>::instance();\n",
        ),
        (
            "span_from_pointer",
            SpanFromPointerChecker(),
            Path("src/fl/example.h"),
            "auto s = span<int>(values.data(), values.size());\n",
        ),
        (
            "std_namespace",
            StdNamespaceChecker(),
            Path("src/fl/example.h"),
            "std::vector<int> values;\n",
        ),
        (
            "stdint_type",
            StdintTypeChecker(),
            Path("src/fl/example.h"),
            "uint32_t value;\n",
        ),
        (
            "subdir_namespace",
            SubdirNamespaceChecker("net"),
            Path("src/fl/net/example.h"),
            "namespace fl {\n}\n",
        ),
        (
            "test_aggregation",
            AggregationChecker(),
            Path("tests/fl/codec/orphan.hpp"),
            "int orphan;\n",
        ),
        (
            "test_include_paths",
            IncludePathStyleChecker(),
            Path("tests/fl/example.cpp"),
            '#include "../helper.hpp"\n',
        ),
        (
            "test_path_structure",
            MirrorPathChecker(),
            Path("tests/fl/missing_path.cpp"),
            'TEST_CASE("missing") {}\n',
        ),
        (
            "thread_local_keyword",
            ThreadLocalKeywordChecker(),
            Path("src/fl/example.h"),
            "thread_local int value;\n",
        ),
        (
            "unit_test",
            UnitTestChecker(),
            Path("tests/fl/example.cpp"),
            '#include "doctest.h"\nTEST_CASE("x") {}\n',
        ),
        (
            "using_namespace_fl_in_examples",
            UsingNamespaceFlInExamplesChecker(),
            Path("examples/Demo/Demo.ino"),
            "using   namespace   fl  ;\n",
        ),
        (
            "serial_printf",
            SerialPrintfChecker(),
            Path("examples/Demo/Demo.ino"),
            'void loop() {\n    Serial.printf("x=%d", x);\n}\n',
        ),
        (
            "simd_intrinsics",
            SimdIntrinsicsChecker(),
            Path("src/fl/example.h"),
            "__m128i value;\n",
        ),
        (
            "weak_attribute",
            WeakAttributeChecker(),
            Path("src/fl/example.h"),
            "__attribute__((weak)) void foo();\n",
        ),
        (
            "using_namespace",
            UsingNamespaceChecker(),
            Path("src/fl/example.h"),
            "using namespace std;\n",
        ),
        (
            "using_namespace_fl",
            UsingNamespaceFlChecker(),
            Path("src/fl/example.h"),
            "using namespace fl;\n",
        ),
    ],
)
def test_rust_checker_matches_python_oracle(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
    checker_key: str,
    checker: FileContentChecker,
    relative_path: Path,
    code: str,
) -> None:
    examples_root = tmp_path / "examples"
    src_root = tmp_path / "src"
    tests_root = tmp_path / "tests"
    monkeypatch.setattr(cpp_hpp_header_pair_checker, "PROJECT_ROOT", tmp_path)
    monkeypatch.setattr(cpp_hpp_header_pair_checker, "FL_ROOT", src_root / "fl")
    monkeypatch.setattr(cpp_hpp_includes_checker, "SRC_ROOT", src_root)
    monkeypatch.setattr(cpp_hpp_includes_checker, "TESTS_ROOT", tests_root)
    monkeypatch.setattr(
        fastled_header_usage_checker, "SRC_DIR", _normalize_path(src_root)
    )
    monkeypatch.setattr(headers_exist_checker, "PROJECT_ROOT", tmp_path)
    monkeypatch.setattr(headers_exist_checker, "TESTS_ROOT", tests_root)
    monkeypatch.setattr(headers_exist_checker, "SRC_ROOT", src_root)
    monkeypatch.setattr(include_paths_checker, "PROJECT_ROOT", tmp_path)
    monkeypatch.setattr(
        is_header_include_checker, "PLATFORMS_ROOT", src_root / "platforms"
    )
    monkeypatch.setattr(iwyu_pragma_block_checker, "PROJECT_ROOT", tmp_path)
    monkeypatch.setattr(namespace_platforms_checker, "SRC_ROOT", src_root)
    monkeypatch.setattr(native_platform_defines_checker, "SRC_ROOT", src_root)
    monkeypatch.setattr(
        native_platform_defines_checker, "PLATFORMS_ROOT", src_root / "platforms"
    )
    monkeypatch.setattr(no_namespace_fl_declaration, "SRC_ROOT", src_root)
    monkeypatch.setattr(relative_include_checker, "SRC_DIR", _normalize_path(src_root))
    monkeypatch.setattr(serial_printf_checker, "EXAMPLES_ROOT", examples_root)
    monkeypatch.setattr(test_aggregation_checker, "PROJECT_ROOT", tmp_path)
    monkeypatch.setattr(test_aggregation_checker, "TESTS_DIR", tests_root)
    monkeypatch.setattr(test_aggregation_checker, "_AGGREGATED_TEST_DIRS", None)
    monkeypatch.setattr(test_include_paths_checker, "PROJECT_ROOT", tmp_path)
    monkeypatch.setattr(
        test_include_paths_checker,
        "TESTS_DIR",
        _normalize_path(tests_root),
    )
    monkeypatch.setattr(
        test_include_paths_checker, "_allowed_bare_test_headers_cache", None
    )
    monkeypatch.setattr(
        test_include_paths_checker, "_allowed_bare_src_headers_cache", None
    )
    monkeypatch.setattr(
        test_include_paths_checker, "_allowed_bare_includes_cache", None
    )
    monkeypatch.setattr(test_include_paths_checker, "_all_test_filenames_cache", None)
    monkeypatch.setattr(test_path_structure_checker, "PROJECT_ROOT", tmp_path)
    monkeypatch.setattr(test_path_structure_checker, "TESTS_ROOT", tests_root)
    monkeypatch.setattr(test_path_structure_checker, "SRC_ROOT", src_root)
    monkeypatch.setattr(unit_test_checker, "TESTS_ROOT", tests_root)
    monkeypatch.setattr(
        using_namespace_fl_in_examples_checker, "EXAMPLES_ROOT", examples_root
    )
    if isinstance(checker, SubdirNamespaceChecker):
        checker.subdir_root = _normalize_path(src_root / "fl" / checker.subdir)

    path = tmp_path / relative_path
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(code, encoding="utf-8")
    if isinstance(checker, AggregationChecker):
        aggregation_dir = tests_root / "fl" / "codec"
        aggregation_dir.mkdir(parents=True, exist_ok=True)
        (tests_root / "fl" / "codec.cpp").write_text("", encoding="utf-8")
        monkeypatch.setattr(
            test_aggregation_checker, "_AGGREGATED_TEST_DIRS", {aggregation_dir}
        )

    assert _rust_records(checker_key, tmp_path, path) == _python_records(
        checker, path, code
    )
