from collections import Counter

from ci.tools import check_array_params


def test_query_uses_ast_to_find_owned_pointer_parameter_functions() -> None:
    query = check_array_params.build_query(".*src.fl.*")

    assert "functionDecl(" in query
    assert "hasAnyParameter(parmVarDecl(hasType(pointerType())))" in query
    assert "unless(isImplicit())" in query
    assert "isLambda()" in query
    assert "linkageSpecDecl()" in query
    assert 'isExpansionInFileMatching(".*src.fl.*")' in query


def test_signature_exemptions() -> None:
    assert check_array_params._signature_is_exempt(
        "void c_api(float out[2]); // ok array parameter"
    )
    assert check_array_params._signature_is_exempt(
        "void c_api(float out[2]); // NOLINT"
    )
    assert check_array_params._signature_is_exempt(
        "FASTLED_INTERNAL_CALLBACK(float out[2])"
    )
    assert check_array_params._signature_is_exempt(
        'extern "C" void c_api(float out[2]);'
    )


def test_rejects_fixed_and_unsized_array_parameters() -> None:
    findings = check_array_params._find_array_params_in_signature(
        "void f(float out[2], const unsigned char bytes[]);"
    )

    assert [finding.spelling for finding in findings] == ["out[2]", "bytes[]"]


def test_rejects_multidimensional_array_parameter() -> None:
    findings = check_array_params._find_array_params_in_signature(
        "void f(float matrix[2][3]);"
    )

    assert [finding.spelling for finding in findings] == ["matrix[2][3]"]


def test_allows_array_reference_parameter() -> None:
    findings = check_array_params._find_array_params_in_signature(
        "void f(float (&out)[2], const float (&input)[3]);"
    )

    assert findings == ()


def test_allows_span_parameter() -> None:
    findings = check_array_params._find_array_params_in_signature(
        "void f(fl::span<float, 2> out, fl::span<const fl::u8> bytes);"
    )

    assert findings == ()


def test_ignores_local_array_and_member_array_in_function_body() -> None:
    signature = """
    struct Holder { float member[2]; };
    void f(float* out) {
        float tmp[2] = {};
        out[0] = tmp[0];
    }
    """

    assert check_array_params._find_array_params_in_signature(signature) == ()


def test_handles_multiline_signature_and_top_level_commas() -> None:
    signature = """
    void f(
        fl::span<const fl::pair<int, int>> pairs,
        float out[2],
        void (*callback)(int, int)
    ) FL_NO_EXCEPT;
    """

    findings = check_array_params._find_array_params_in_signature(signature)

    assert [finding.spelling for finding in findings] == ["out[2]"]


def test_marks_iram_signatures_for_array_reference_recommendation() -> None:
    findings = check_array_params._find_array_params_in_signature(
        "FL_IRAM void f(fl::u8 out[4]) FL_NO_EXCEPT;"
    )
    hit = check_array_params.ArrayParamHit(
        path="src/platforms/example.h",
        line=10,
        line_text="FL_IRAM void f(fl::u8 out[4]) FL_NO_EXCEPT;",
        signature="FL_IRAM void f(fl::u8 out[4]) FL_NO_EXCEPT;",
        findings=findings,
    )

    assert findings[0].is_iram
    assert "Use 'T (&value)[N]' for ISR-safe" in check_array_params._diagnostic_for_hit(
        hit
    )


def test_non_iram_diagnostic_allows_span_or_array_reference() -> None:
    findings = check_array_params._find_array_params_in_signature(
        "void f(fl::u8 out[4]) FL_NO_EXCEPT;"
    )
    hit = check_array_params.ArrayParamHit(
        path="src/fl/example.h",
        line=10,
        line_text="void f(fl::u8 out[4]) FL_NO_EXCEPT;",
        signature="void f(fl::u8 out[4]) FL_NO_EXCEPT;",
        findings=findings,
    )

    diagnostic = check_array_params._diagnostic_for_hit(hit)

    assert "fl::span<T, N>" in diagnostic
    assert "T (&value)[N]" in diagnostic


def test_baseline_diff_counts_duplicate_signatures() -> None:
    hits = [
        check_array_params.ArrayParamHit(
            path="src/fl/example.h",
            line=10,
            line_text="void foo(float out[2]);",
            signature="void foo(float out[2]);",
            findings=(
                check_array_params.ArrayParamFinding(
                    name="out", spelling="out[2]", is_iram=False
                ),
            ),
        ),
        check_array_params.ArrayParamHit(
            path="src/fl/example.h",
            line=11,
            line_text="void foo(float out[2]);",
            signature="void foo(float out[2]);",
            findings=(
                check_array_params.ArrayParamFinding(
                    name="out", spelling="out[2]", is_iram=False
                ),
            ),
        ),
        check_array_params.ArrayParamHit(
            path="src/fl/example.h",
            line=12,
            line_text="void bar(float out[]);",
            signature="void bar(float out[]);",
            findings=(
                check_array_params.ArrayParamFinding(
                    name="out", spelling="out[]", is_iram=False
                ),
            ),
        ),
    ]
    baseline = Counter({hits[0].baseline_key: 1, "src/fl/example.h|void old();": 1})

    new_hits, stale = check_array_params.diff_against_baseline(hits, baseline)

    assert [hit.line for hit in new_hits] == [11, 12]
    assert stale == ["src/fl/example.h|void old();"]
