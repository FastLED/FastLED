"""The macro-prefix ratchet has to catch the case that motivated it.

`FASTLED_SAMD51_HW_SPI` (FastLED#4021) was a user-supplied opt-in: it appeared
only as `#if defined(...)` and was never `#define`d in this repo. A checker
that scanned definitions alone would have passed it, which is the failure
these tests exist to prevent.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from ci.tools.check_macro_prefix import (
    is_excluded,
    load_baseline,
    names_in,
    render_baseline,
    scan,
)


def test_a_reference_only_macro_is_found() -> None:
    """The historical defect's exact shape: tested, never defined."""
    source = "#if defined(FL_IS_SAMD51) && defined(FASTLED_SAMD51_HW_SPI)\n#endif\n"
    assert names_in(source) == {"FASTLED_SAMD51_HW_SPI"}


def test_a_definition_is_found() -> None:
    assert names_in("#define FASTLED_SOMETHING 1\n") == {"FASTLED_SOMETHING"}


def test_every_conditional_form_is_covered() -> None:
    # `#ifdef`, `#ifndef` and `#elif` all name a macro just as `#if` does;
    # missing one leaves a hole the next opt-in flag walks through.
    for line in (
        "#ifdef FASTLED_KNOB",
        "#ifndef FASTLED_KNOB",
        "#elif defined(FASTLED_KNOB)",
        "#if FASTLED_KNOB > 0",
        "#undef FASTLED_KNOB",
        "  #  define FASTLED_KNOB 1",
    ):
        assert names_in(line + "\n") == {"FASTLED_KNOB"}, line


def test_a_conforming_name_is_not_flagged() -> None:
    assert names_in("#if defined(FL_SAMD51_HW_SPI)\n") == set()
    assert names_in("#define FL_WATCHDOG_HAS_WINDOW_MODE 1\n") == set()


def test_a_mention_outside_a_preprocessor_line_is_ignored() -> None:
    # Prose and code that merely name a macro are not declarations of one;
    # flagging them would make the checker unusable in commentary.
    assert names_in("// FASTLED_SAMD51_HW_SPI is documented here\n") == set()
    assert names_in('const char* s = "FASTLED_SAMD51_HW_SPI";\n') == set()


def test_a_suppression_needs_a_reason() -> None:
    with_reason = (
        "// fl-lint: macro-prefix-ok(matches the Arduino core's own name)\n"
        "#define FASTLED_EXTERNALLY_NAMED 1\n"
    )
    assert names_in(with_reason) == set()

    # A bare marker is indistinguishable from someone silencing the check.
    bare = "// fl-lint: macro-prefix-ok\n#define FASTLED_EXTERNALLY_NAMED 1\n"
    assert names_in(bare) == {"FASTLED_EXTERNALLY_NAMED"}


def test_a_same_line_suppression_works() -> None:
    line = "#define FASTLED_X 1  // fl-lint: macro-prefix-ok(vendor header)\n"
    assert names_in(line) == set()


def test_vendored_code_is_out_of_scope() -> None:
    assert is_excluded("third_party/minimp3/minimp3.h")
    assert not is_excluded("platforms/arm/d51/spi_hw_2_samd51.cpp.hpp")


def test_the_tree_matches_its_baseline() -> None:
    """The ratchet is only meaningful if it is currently satisfied."""
    from ci.tools.check_macro_prefix import BASELINE_PATH, SOURCE_ROOT

    found = set(scan(SOURCE_ROOT))
    baseline = load_baseline(BASELINE_PATH)
    assert found - baseline == set(), "regenerate the baseline"
    # Not vacuous: there really are names being tracked.
    assert len(baseline) > 100


def test_the_baseline_round_trips(tmp_path: Path) -> None:
    names = ["FASTLED_A", "FASTLED_B"]
    path = tmp_path / "baseline.txt"
    path.write_text(render_baseline(names), encoding="utf-8")
    assert load_baseline(path) == set(names)


def test_a_name_inside_a_string_is_not_a_use() -> None:
    # `#define LABEL "FASTLED_NEW"` defines LABEL. Flagging the string would
    # make the ratchet fire on data.
    assert names_in('#define LABEL "FASTLED_NEW"\n') == set()


def test_a_name_inside_a_comment_is_not_a_use() -> None:
    assert names_in("#if 1 // FASTLED_NEW\n") == set()
    assert names_in("#if 1 /* FASTLED_NEW */\n") == set()


def test_a_directive_quoted_in_a_block_comment_is_not_a_directive() -> None:
    assert names_in("/*\n#define FASTLED_DOCUMENTED 1\n*/\n") == set()


def test_a_reference_split_across_a_line_continuation_is_found() -> None:
    """The false *negative* — the direction that matters for a ratchet.

    A physical-line scan misses this, because the continued line does not
    start with `#`.
    """
    source = "#if defined( \\\n    FASTLED_CONTINUED)\n#endif\n"
    assert names_in(source) == {"FASTLED_CONTINUED"}


def test_stripping_comments_does_not_shift_the_suppression_lookup() -> None:
    # Block-comment removal has to preserve line count, or the "suppression
    # on the line above" rule starts pointing at the wrong line.
    source = (
        "/* a\n   multi-line\n   comment */\n"
        "// fl-lint: macro-prefix-ok(external name)\n"
        "#define FASTLED_AFTER_COMMENT 1\n"
    )
    assert names_in(source) == set()


def test_a_missing_baseline_fails_loudly(tmp_path: Path) -> None:
    # An empty fallback would report all ~490 historical names as new, which
    # reads as a catastrophic regression and trains the reader to ignore it.
    with pytest.raises(FileNotFoundError) as caught:
        load_baseline(tmp_path / "nope.txt")
    assert "--update-baseline" in str(caught.value)


def test_a_suppression_inside_a_string_does_not_silence_a_macro() -> None:
    """The contract is `// fl-lint: ...`, and only that.

    A marker in a string literal or a block comment reads as a suppression to
    a raw-text search, which is a way past the check rather than a use of it.
    """
    in_string = (
        'const char* s = "fl-lint: macro-prefix-ok(fake)";\n#define FASTLED_SNEAKY 1\n'
    )
    assert names_in(in_string) == {"FASTLED_SNEAKY"}

    in_block = "/* fl-lint: macro-prefix-ok(fake) */\n#define FASTLED_SNEAKY 1\n"
    assert names_in(in_block) == {"FASTLED_SNEAKY"}


def test_a_genuine_line_comment_still_suppresses() -> None:
    # Guards the fix above from becoming "nothing suppresses".
    above = "// fl-lint: macro-prefix-ok(vendor name)\n#define FASTLED_OK 1\n"
    assert names_in(above) == set()
    same_line = "#define FASTLED_OK 1  // fl-lint: macro-prefix-ok(vendor name)\n"
    assert names_in(same_line) == set()


def test_a_literal_spanning_a_continuation_does_not_break_the_splice() -> None:
    """Splice before masking, or the backslash the splice needs is gone.

    A string literal spanning a backslash-newline inside a continued
    directive is legal C. Masking it first blanked the backslash, the
    continuation broke, and the name on the next physical line was never seen
    as part of a directive -- a false negative.
    """
    source = '#define BANNER "abc\\\ndef" \\\n    FASTLED_HIDDEN\n'
    assert names_in(source) == {"FASTLED_HIDDEN"}


def test_a_directive_inside_a_raw_string_is_not_a_directive() -> None:
    source = (
        'const char* k = R"json(\n'
        "#define FASTLED_IN_RAW 1\n"
        ')json";\n'
        "#define FASTLED_REAL 1\n"
    )
    # The real macro after the literal must still be found.
    assert names_in(source) == {"FASTLED_REAL"}


def test_a_raw_string_body_may_contain_a_close_paren() -> None:
    # The delimiter is back-matched, which is what makes a bare `)` in the
    # body harmless; a non-delimiter-aware pattern would end the literal
    # early and expose the rest.
    source = (
        'const char* k = R"j(a) not the end\n'
        "#define FASTLED_HIDDEN 1\n"
        ')j";\n'
        "#define FASTLED_REAL 1\n"
    )
    assert names_in(source) == {"FASTLED_REAL"}


def test_a_prefixed_raw_string_is_recognised() -> None:
    source = 'const char* k = u8R"x(\n#define FASTLED_IN_RAW 1\n)x";\n'
    assert names_in(source) == set()
