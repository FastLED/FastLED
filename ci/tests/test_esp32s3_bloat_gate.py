"""The baseline is a ratchet, and a ratchet with slack is not one.

`tests/test_esp32s3_bloat_regression.py` used to say two different things
about what to do with an intentional regression: its docstring said the
baseline only ratchets down and that any regression fails, while its own
failure message said to update the baseline in the same PR. #4200 stalled on
that contradiction, and the same comment measured its cost: 232 B of unclaimed
headroom had accumulated, so a 429 B regression printed as 197 B.

These pin the settled rule -- the file ends every PR equal to what that PR
builds -- and the parser that now lets an upward move carry its reason.
"""

from __future__ import annotations

from typeguard import typechecked

from tests.test_esp32s3_bloat_regression import (
    comment_lines,
    kHeadroomTolerance,
    parse_baseline,
    raise_lacks_reason,
)


@typechecked
def _verdict(total_flash: int, baseline: int) -> str:
    """The three outcomes, expressed the way `main()` decides them."""

    delta = total_flash - baseline
    if delta > 0:
        return "over"
    if -delta > kHeadroomTolerance:
        return "under"
    return "pass"


def test_a_bare_integer_still_parses() -> None:
    assert parse_baseline("342177") == 342177
    assert parse_baseline("  342177  \n") == 342177


def test_a_comment_can_record_why_the_baseline_moved_up() -> None:
    # The point of allowing comments: an upward move is a decision, and the
    # file that records it is where the reason belongs.
    text = (
        "# Raised by #4200: running the colour pipeline in show() costs 429 B\n"
        "# of dispatch that cannot be elided, because it is the code that\n"
        "# decides whether to elide.\n"
        "342606\n"
    )
    assert parse_baseline(text) == 342606


def test_blank_lines_are_skipped() -> None:
    assert parse_baseline("\n\n# note\n\n342177\n") == 342177


def test_a_file_with_no_number_is_reported_rather_than_raising() -> None:
    # A malformed baseline is an infrastructure failure, not a regression, and
    # the caller exits differently for the two. Raising here would collapse
    # them into one.
    assert parse_baseline("") is None
    assert parse_baseline("# only a comment\n") is None
    assert parse_baseline("not a number\n") is None


def test_a_build_over_the_baseline_fails() -> None:
    assert _verdict(342_606, 342_177) == "over"
    assert _verdict(342_178, 342_177) == "over"


def test_a_build_at_the_baseline_passes() -> None:
    assert _verdict(342_177, 342_177) == "pass"


def test_small_headroom_is_noise_and_passes() -> None:
    assert _verdict(342_177 - kHeadroomTolerance, 342_177) == "pass"


def test_unclaimed_headroom_fails() -> None:
    # The case that had no verdict before: a saving nobody pinned. 232 B is
    # the drift #4200 measured, and it used to pass silently.
    assert _verdict(342_177 - 232, 342_177) == "under"
    assert _verdict(342_177 - (kHeadroomTolerance + 1), 342_177) == "under"


def test_a_raise_without_a_reason_is_caught() -> None:
    # Documenting the requirement is not the same as having it: without this
    # a PR could raise the number, match it, and pass -- reproducing in a new
    # place the exact gap the policy rewrite was meant to close.
    assert raise_lacks_reason("342177\n", "342606\n")


def test_a_raise_with_a_reason_passes() -> None:
    current = (
        "# Raised by #4200: the dispatch that decides whether to elide the\n"
        "# colour pipeline cannot itself be elided.\n"
        "342606\n"
    )
    assert not raise_lacks_reason("342177\n", current)


def test_a_reason_left_over_from_an_earlier_raise_does_not_count() -> None:
    # "Has a comment" would accept this. The comparison is against the
    # previous comments, so each raise has to say its own piece.
    previous = "# Raised by #3944: TM1812 encoder\n342177\n"
    current = "# Raised by #3944: TM1812 encoder\n342606\n"
    assert raise_lacks_reason(previous, current)


def test_editing_the_reason_in_place_counts_as_giving_one() -> None:
    # Full-line comparison rather than counting, so replacing the old reason
    # with a new one is a reason rather than a repeat.
    previous = "# Raised by #3944: TM1812 encoder\n342177\n"
    current = "# Raised by #4200: colour pipeline dispatch\n342606\n"
    assert not raise_lacks_reason(previous, current)


def test_lowering_the_baseline_needs_no_reason() -> None:
    # A saving explains itself, and demanding prose for one would make the
    # ratchet's easy direction the annoying one.
    assert not raise_lacks_reason("342606\n", "342177\n")
    assert not raise_lacks_reason("342177\n", "342177\n")


def test_a_malformed_file_is_not_reported_as_a_missing_reason() -> None:
    # It is an infrastructure failure and exits differently. Naming it a
    # missing reason would point the reader at the wrong fault.
    assert not raise_lacks_reason("junk\n", "342606\n")
    assert not raise_lacks_reason("342177\n", "junk\n")


def test_comment_lines_are_returned_in_order() -> None:
    assert comment_lines("# a\n342177\n# b\n") == ("# a", "# b")
    assert comment_lines("342177\n") == ()
