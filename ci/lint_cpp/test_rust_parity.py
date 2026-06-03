#!/usr/bin/env python3
"""Parity tests for the Rust C++ linter checker subset."""

from __future__ import annotations

from pathlib import Path
from typing import Any

import pytest
from running_process import RunningProcess

from ci.lint_cpp import (
    cpp_hpp_includes_checker,
    fastled_header_usage_checker,
    include_paths_checker,
    relative_include_checker,
    serial_printf_checker,
    using_namespace_fl_in_examples_checker,
)
from ci.lint_cpp.asm_js_location_checker import AsmJsLocationChecker
from ci.lint_cpp.banned_define_checker import BannedDefineChecker
from ci.lint_cpp.banned_macros_checker import BannedMacrosChecker
from ci.lint_cpp.banned_namespace_checker import BannedNamespaceChecker
from ci.lint_cpp.bare_allocation_checker import BareAllocationChecker
from ci.lint_cpp.builtin_memcpy_checker import BuiltinMemcpyChecker
from ci.lint_cpp.cpp_hpp_includes_checker import CppHppIncludesChecker
from ci.lint_cpp.cpp_include_checker import CppIncludeChecker
from ci.lint_cpp.esp_rom_printf_checker import EspRomPrintfChecker
from ci.lint_cpp.fastled_header_usage_checker import FastLEDHeaderUsageChecker
from ci.lint_cpp.impl_hpp_includes_checker import ImplHppIncludesChecker
from ci.lint_cpp.include_paths_checker import IncludePathsChecker
from ci.lint_cpp.pragma_once_checker import PragmaOnceChecker
from ci.lint_cpp.reinterpret_cast_checker import ReinterpretCastChecker
from ci.lint_cpp.relative_include_checker import RelativeIncludeChecker
from ci.lint_cpp.rust_bridge import parse_rust_json_output
from ci.lint_cpp.serial_printf_checker import SerialPrintfChecker
from ci.lint_cpp.sleep_for_checker import SleepForChecker
from ci.lint_cpp.span_from_pointer_checker import SpanFromPointerChecker
from ci.lint_cpp.static_in_headers_checker import StaticInHeaderChecker
from ci.lint_cpp.thread_local_keyword_checker import ThreadLocalKeywordChecker
from ci.lint_cpp.using_namespace_fl_in_examples_checker import (
    UsingNamespaceFlInExamplesChecker,
)
from ci.lint_cpp.weak_attribute_checker import WeakAttributeChecker
from ci.util.check_files import FileContent, FileContentChecker
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
    violations = violations_by_path.get(
        file_path, violations_by_path.get(_normalize_path(file_path), [])
    )
    return [
        {
            "checker": checker.__class__.__name__,
            "path": _normalize_path(file_path),
            "line": line,
            "message": message,
        }
        for line, message in violations
    ]


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
            "asm_js_location",
            AsmJsLocationChecker(),
            Path("src/fl/example.h"),
            "EM_JS(void, demo, (), {});\n",
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
            "cpp_include",
            CppIncludeChecker(),
            Path("src/fl/example.h"),
            '#include "foo.cpp"\n',
        ),
        (
            "esp_rom_printf",
            EspRomPrintfChecker(),
            Path("src/fl/example.h"),
            'esp_rom_printf("x");\n',
        ),
        (
            "fastled_header_usage",
            FastLEDHeaderUsageChecker(),
            Path("src/fl/example.h"),
            '#include "FastLED.h"\n',
        ),
        (
            "include_paths",
            IncludePathsChecker(),
            Path("src/fl/example.h"),
            '#include "../foo.h"\n',
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
            "bare_allocation",
            BareAllocationChecker(),
            Path("src/fl/example.h"),
            "void* p = malloc(4);\nvoid* q = fl::malloc(4);\n",
        ),
        (
            "pragma_once",
            PragmaOnceChecker(),
            Path("src/fl/example.h"),
            "int value;\n",
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
            "span_from_pointer",
            SpanFromPointerChecker(),
            Path("src/fl/example.h"),
            "auto s = span<int>(values.data(), values.size());\n",
        ),
        (
            "thread_local_keyword",
            ThreadLocalKeywordChecker(),
            Path("src/fl/example.h"),
            "thread_local int value;\n",
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
            "weak_attribute",
            WeakAttributeChecker(),
            Path("src/fl/example.h"),
            "__attribute__((weak)) void foo();\n",
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
    monkeypatch.setattr(cpp_hpp_includes_checker, "SRC_ROOT", src_root)
    monkeypatch.setattr(cpp_hpp_includes_checker, "TESTS_ROOT", tests_root)
    monkeypatch.setattr(
        fastled_header_usage_checker, "SRC_DIR", _normalize_path(src_root)
    )
    monkeypatch.setattr(include_paths_checker, "PROJECT_ROOT", tmp_path)
    monkeypatch.setattr(relative_include_checker, "SRC_DIR", _normalize_path(src_root))
    monkeypatch.setattr(serial_printf_checker, "EXAMPLES_ROOT", examples_root)
    monkeypatch.setattr(
        using_namespace_fl_in_examples_checker, "EXAMPLES_ROOT", examples_root
    )

    path = tmp_path / relative_path
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(code, encoding="utf-8")

    assert _rust_records(checker_key, tmp_path, path) == _python_records(
        checker, path, code
    )
