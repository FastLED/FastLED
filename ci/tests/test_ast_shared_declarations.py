from collections import Counter

import pytest

from ci.tools import check_ast_combined, check_noexcept


@pytest.mark.parametrize("combined", [False, True])
def test_shared_header_declarations_count_once_across_units(
    monkeypatch: pytest.MonkeyPatch, combined: bool
) -> None:
    units = [
        ("ci/tools/_noexcept_check_fl_tu.cpp", ".*src.fl.*"),
        ("ci/tools/_noexcept_check_platforms_tu.cpp", ".*src.fl.*"),
    ]
    first = check_noexcept.NoexceptHit("src/fl/example.h", 10, "void f();", "void f();")
    second = check_noexcept.NoexceptHit(
        "src/fl/example.h", 11, "void f();", "void f();"
    )
    module = check_ast_combined if combined else check_noexcept
    monkeypatch.setattr(module, "_find_clang_query", lambda: ["clang-query"])
    resolver = "_noexcept_scope_tus" if combined else "_scope_tus"
    monkeypatch.setattr(module, resolver, lambda _scope: units)
    if combined:
        monkeypatch.setattr(module, "_array_scope_tus", lambda _scope: units)
        monkeypatch.setattr(
            module, "_run_combined_clang_query", lambda *_args: ([first, second], [])
        )
        hits, _ = check_ast_combined.find_combined_hits()
    else:
        monkeypatch.setattr(module, "_run_clang_query", lambda *_args: [first, second])
        hits = check_noexcept.find_missing_noexcept()
    assert hits == [first, second]
    new_hits, _ = check_noexcept.diff_against_baseline(
        hits, Counter({first.baseline_key: 1})
    )
    assert new_hits == [second]
