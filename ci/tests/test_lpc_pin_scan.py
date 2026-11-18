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


def test_free_pins_excludes_wired_stuck_skipped_and_open_drain() -> None:
    from ci.autoresearch.lpc_pin_scan import free_pins

    # Bench scan: 10/11/13 wired, 4/7/23 stuck, 2/3/5/12/24/25 skipped; the
    # core maps P0_0..P0_29, so 30 and 31 are out of range anyway.
    expected = [0, 1, 6, 8, 9, 14, 15, 16, 17, 18, 19, 20, 21, 22, 26, 27, 28, 29]
    assert free_pins(parse_scan(BENCH_REPLY)) == expected


class _PullBench:
    """Answers the scan, then corePullCheck with a fixed reply."""

    def __init__(self, pull_reply: object) -> None:
        self.pull_reply = pull_reply
        self.pull_args: object = None

    def call_flat(self, method: str, args: object = None, **_k: object) -> object:
        if method == "findConnectedPins":
            return BENCH_REPLY
        self.pull_args = args
        return self.pull_reply


def test_core_pull_check_passes_on_correct_core() -> None:
    from ci.autoresearch.lpc_pin_scan import check_core_pulls

    bench = _PullBench("0:up=1,down=0 1:up=1,down=0 6:up=1,down=0 8:up=1,down=0 ")
    assert check_core_pulls(bench, "t") is True
    assert bench.pull_args == [0, 1, 6, 8]


def test_core_pull_check_fails_on_wrong_slot_core() -> None:
    from ci.autoresearch.lpc_pin_scan import check_core_pulls

    # Silicon reply from core 9e8be02 (IOCON indexed by GPIO bit).
    bench = _PullBench("0:up=0,down=1 1:up=0,down=1 6:up=0,down=1 8:up=0,down=1 ")
    assert check_core_pulls(bench, "t") is False


def test_core_pull_check_skips_when_unbound() -> None:
    from ci.autoresearch.lpc_pin_scan import check_core_pulls
    from ci.autoresearch.rpc_bench import METHOD_NOT_FOUND

    assert check_core_pulls(_PullBench(METHOD_NOT_FOUND), "t") is None
