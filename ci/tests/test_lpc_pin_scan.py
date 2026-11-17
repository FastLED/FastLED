"""LPC jumper scan: reply parsing and loopback-pair choice.

The reply format comes from `lowMemFindConnectedPins` in
examples/AutoResearch/AutoResearchLowMemory.h. The first reply below is the
LPC845-BRK bench as scanned on silicon: one net joins P0_10, P0_11 (true
open-drain pads) and P0_13.
"""

from ci.autoresearch.lpc_pin_scan import parse_scan, pick_pair


BENCH_REPLY: str = (
    "pairs=10>13od 11>13od ;stuck=4H 7H 23H 30H ;"
    "skip=2:debug-swd 3:debug-swd 5:reset 12:isp-entry 24:console-link "
    "25:console-link ;od=10 11 "
)


def test_parse_bench_reply() -> None:
    scan = parse_scan(BENCH_REPLY)
    assert scan.pairs == [(10, 13, True), (11, 13, True)]
    assert scan.stuck == ["4H", "7H", "23H", "30H"]
    assert scan.skipped[0] == "2:debug-swd"
    assert scan.open_drain == [10, 11]


def test_open_drain_detection_is_driven_from_the_push_pull_end() -> None:
    # An open-drain pad only sinks; its rising edge would be the slow pull-up.
    assert pick_pair(parse_scan(BENCH_REPLY)) == (13, 10)


def test_prefers_bidirectional_push_pull_pair() -> None:
    reply = "pairs=10>13od 4>9 7>8 8>7 ;stuck=;skip=;od=10 11"
    assert pick_pair(parse_scan(reply)) == (7, 8)


def test_falls_back_to_one_way_push_pull_pair() -> None:
    assert pick_pair(parse_scan("pairs=10>13od 4>9 ;stuck=;skip=;od=")) == (4, 9)


def test_no_pairs() -> None:
    scan = parse_scan("pairs=;stuck=;skip=;od=")
    assert scan.pairs == []
    assert pick_pair(scan) is None


class _Bench:
    def __init__(self, reply: object) -> None:
        self.reply = reply

    def call_flat(self, *_a: object, **_k: object) -> object:
        return self.reply


def test_one_explicit_pin_is_rejected_not_dropped() -> None:
    from ci.autoresearch.lpc_pin_scan import resolve_pins

    assert resolve_pins(_Bench(BENCH_REPLY), 13, None, "t") is None


def test_timeout_and_unbound_are_reported_differently() -> None:
    from ci.autoresearch.lpc_pin_scan import run_scan
    from ci.autoresearch.rpc_bench import METHOD_NOT_FOUND

    assert "not bound" in str(run_scan(_Bench(METHOD_NOT_FOUND)))
    assert "did not answer" in str(run_scan(_Bench(None)))
