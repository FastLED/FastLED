"""The annotation tool uses lint discovery and preserves function bodies."""

from pathlib import Path
from types import SimpleNamespace

import pytest

from ci.refactor import add_fl_noexcept as tool
from ci.tools.check_noexcept import NoexceptCheckError, build_query


def test_discovery_uses_active_checker_for_every_scope(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    scopes: list[str] = []

    def discover(scope: str) -> list[SimpleNamespace]:
        scopes.append(scope)
        hit = SimpleNamespace(path="src/fl/stl/basic_string.cpp.hpp", line=43)
        return [hit, hit]

    monkeypatch.setattr(tool, "find_missing_noexcept", discover)
    for scope in tool._SCOPES:
        assert tool._discover_hits(scope) == [("src/fl/stl/basic_string.cpp.hpp", 43)]
    assert scopes == list(tool._SCOPES)
    assert {"fl", "platforms", "third_party", "all"}.issubset(scopes)


def test_active_discovery_protects_c_linkage_and_inferred_members() -> None:
    query = build_query(".*src.*")
    assert "unless(hasParent(linkageSpecDecl()))" in query
    assert "unless(isDefaulted())" in query
    assert "unless(isDeleted())" in query


def test_query_failure_never_applies_partial_results(
    monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    def fail(scope: str) -> list[tuple[str, int]]:
        raise NoexceptCheckError("fixture parse failure")

    monkeypatch.setattr(tool, "_discover_hits", fail)
    monkeypatch.setattr("sys.argv", ["add_fl_noexcept.py", "--apply"])
    monkeypatch.setattr(
        tool, "_apply_fixes", lambda *args: pytest.fail("must not apply after failure")
    )
    assert tool.main() == 1
    assert "fixture parse failure" in capsys.readouterr().out


@pytest.mark.parametrize(
    ("source", "expected"),
    [
        (
            "int read(\n    int count) const {\n    return count;\n}",
            "int read(\n    int count) const FL_NO_EXCEPT {\n    return count;\n}",
        ),
        (
            "auto read(int count) const && -> decltype(count) { return count; }",
            "auto read(int count) const && FL_NO_EXCEPT -> decltype(count) { return count; }",
        ),
        (
            "Thing(int count)\n    : value(count) {}",
            "Thing(int count)\n    FL_NO_EXCEPT : value(count) {}",
        ),
    ],
)
def test_multiline_insertion_preserves_body_and_correct_specification_order(
    source: str, expected: str
) -> None:
    lines = source.splitlines()
    replacements = tool._insert_fl_noexcept_multiline(lines, 0)
    assert replacements is not None
    for index, replacement in replacements:
        lines[index] = replacement
    assert "\n".join(lines) == expected


def test_existing_annotation_is_not_duplicated() -> None:
    assert tool._insert_fl_noexcept_multiline(["void run() FL_NO_EXCEPT {}"], 0) is None


@pytest.mark.parametrize(
    "prefix",
    [
        "static int __attribute__((always_inline))",
        "static int __attribute__((always_inline)) __attribute__((unused))",
        "decltype(factory())",
        "Sized<sizeof(int)>",
    ],
)
def test_prefix_expressions_do_not_receive_the_exception_specification(
    prefix: str,
) -> None:
    source = f"{prefix} calculate(int value) {{ return value; }}"
    lines = [source]
    replacements = tool._insert_fl_noexcept_multiline(lines, 0)
    assert replacements is not None
    for index, replacement in replacements:
        lines[index] = replacement
    assert lines == [f"{prefix} calculate(int value) FL_NO_EXCEPT {{ return value; }}"]


def test_multiline_attribute_prefix_preserves_the_function_body() -> None:
    source = [
        "static int __attribute__((",
        "    always_inline)) calculate(",
        "    int value) {",
        "    return value;",
        "}",
    ]
    replacements = tool._insert_fl_noexcept_multiline(source, 0)
    assert replacements == [(2, "    int value) FL_NO_EXCEPT {")]


def test_conditional_constructor_initializer_keeps_directive_on_its_own_line() -> None:
    source = [
        "PerceptualWeighting::PerceptualWeighting()",
        "#if SKETCH_HAS_LARGE_MEMORY",
        "    : mHistoryIndex(0)",
        "#endif",
        "{",
        "    initialize();",
        "}",
    ]
    replacements = tool._insert_fl_noexcept_multiline(source, 0)
    assert replacements == [
        (0, "PerceptualWeighting::PerceptualWeighting() FL_NO_EXCEPT")
    ]


def test_apply_preserves_final_newline_and_body(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
) -> None:
    source = tmp_path / "example.h"
    source.write_text("void run() {}\n")
    monkeypatch.setattr(tool, "PROJECT_ROOT", tmp_path)
    tool._apply_fixes([("example.h", 1)], True)
    assert (
        source.read_text()
        == '#include "fl/stl/noexcept.h"\nvoid run() FL_NO_EXCEPT {}\n'
    )
