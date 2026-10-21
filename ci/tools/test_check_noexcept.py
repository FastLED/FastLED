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
    assert not check_noexcept._signature_is_exempt("void foo() FL_NO_EXCEPT;")
    assert not check_noexcept._signature_is_exempt("void foo() noexcept;")
    assert check_noexcept._signature_is_exempt("void foo(); // ok no noexcept")
    assert not check_noexcept._signature_is_exempt("auto fn = [x]() { return x; };")
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
