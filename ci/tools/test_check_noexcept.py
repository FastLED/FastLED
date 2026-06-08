from collections import Counter

from ci.tools import check_noexcept


def test_query_excludes_non_actionable_ast_nodes() -> None:
    query = check_noexcept.build_query(".*src.fl.*")

    assert "unless(isDeleted())" in query
    assert "unless(isDefaulted())" in query
    assert "unless(isImplicit())" in query
    assert "unless(cxxDestructorDecl())" in query
    assert "isLambda()" in query
    assert "linkageSpecDecl()" in query
    assert 'isExpansionInFileMatching(".*src.fl.*")' in query


def test_signature_exemptions() -> None:
    assert check_noexcept._signature_is_exempt("void foo() FL_NOEXCEPT;")
    assert check_noexcept._signature_is_exempt("void foo() noexcept;")
    assert check_noexcept._signature_is_exempt("void foo(); // ok no noexcept")
    assert check_noexcept._signature_is_exempt("auto fn = [x]() { return x; };")
    assert check_noexcept._signature_is_exempt("~Foo();")
    assert check_noexcept._signature_is_exempt("FASTLED_UI_DEFINE_OPERATORS(UIButton)")
    assert check_noexcept._signature_is_exempt('extern "C" void app_main();')


def test_signature_without_annotation_is_not_exempt() -> None:
    assert not check_noexcept._signature_is_exempt("void foo();")


def test_normalize_signature_removes_comments_and_collapses_whitespace() -> None:
    signature = """
    static
    int foo(
        int value
    ); // trailing note
    """

    assert (
        check_noexcept.normalize_signature(signature) == "static int foo( int value );"
    )


def test_baseline_diff_counts_duplicate_signatures() -> None:
    hits = [
        check_noexcept.NoexceptHit(
            path="src/fl/example.h",
            line=10,
            line_text="void foo();",
            signature="void foo();",
        ),
        check_noexcept.NoexceptHit(
            path="src/fl/example.h",
            line=11,
            line_text="void foo();",
            signature="void foo();",
        ),
        check_noexcept.NoexceptHit(
            path="src/fl/example.h",
            line=12,
            line_text="void bar();",
            signature="void bar();",
        ),
    ]
    baseline = Counter({hits[0].baseline_key: 1, "src/fl/example.h|void old();": 1})

    new_hits, stale = check_noexcept.diff_against_baseline(hits, baseline)

    assert [hit.line for hit in new_hits] == [11, 12]
    assert stale == ["src/fl/example.h|void old();"]
