"""LPC845 --ws2812-loopback: reply parsing and per-case verdicts.

The CSV comes from the `ws2812SctTest` handler in
examples/AutoResearch/AutoResearchLowMemory.h.
"""

from ci.autoresearch.test_lpc_ws2812_loopback import (
    CASES,
    checkable_bytes,
    evaluate,
    parse_csv_result,
)


def _verdict(case_index: int, csv: str) -> bool:
    return evaluate(CASES[case_index], parse_csv_result(csv))[0]


def test_parse_reads_the_capture_capacity() -> None:
    parsed = parse_csv_result("1,0,1,3,3,3,0,47,72")
    assert parsed["cap"] == 72
    assert parsed["edges"] == 47


def test_parse_rejects_the_old_eight_field_reply() -> None:
    assert parse_csv_result("0,0,1,3,0,0,3,0") == {}


def test_full_frame_passes_only_when_every_byte_matches() -> None:
    assert _verdict(0, "1,0,1,3,3,3,0,48,72")
    assert not _verdict(0, "0,0,1,3,2,2,0,47,72")  # last byte not decoded
    assert not _verdict(0, "0,0,1,3,3,2,1,48,72")  # one wrong byte


def test_hundred_led_case_checks_the_prefix_the_capture_holds() -> None:
    # 192 edges hold 12 of the 300 bytes.
    assert checkable_bytes(300, 192) == 12
    assert _verdict(4, "1,4,100,300,12,12,0,192,192")
    assert not _verdict(4, "0,4,100,300,11,11,0,192,192")
    assert not _verdict(4, "0,4,100,300,12,11,1,192,192")


def test_rejected_arguments_are_reported() -> None:
    ok, msg = evaluate(CASES[0], parse_csv_result("0,0,0,0,0,0,0,0,0"))
    assert not ok
    assert "open-drain" in msg


def test_firmware_and_host_verdicts_must_agree() -> None:
    assert not _verdict(0, "0,0,1,3,3,3,0,48,72")


def test_setup_failure_keeps_the_case_fields() -> None:
    ok, msg = evaluate(CASES[1], parse_csv_result("0,1,3,9,0,0,0,0,0"))
    assert not ok
    assert "rejected" not in msg


def test_reply_for_another_case_fails() -> None:
    # Cases 0 and 2 are both one LED; only test_case tells them apart.
    assert not _verdict(0, "1,2,1,3,3,3,0,48,72")


def test_no_capture_fails() -> None:
    assert not _verdict(0, "0,0,1,3,0,0,0,0,72")
