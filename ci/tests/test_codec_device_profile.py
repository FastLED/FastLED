"""device_profile.py must accept a run without the Helix leg.

Helix is RPSL/RCSL-licensed and lives only on feat/helix-benchmark-reference,
so on master `bash autoresearch esp32c6 --mp3` prints the fixture decode time
and checksum but no "Layer III only ... helix" line. The parser used to demand
that line and failed every master device run with "could not parse the result
line", which made `bash mp3measure`'s device step -- the authority for speed
claims -- unusable on the branch people actually measure from.
"""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

import pytest


# Loaded by path like test_codec_cpu_audit.py: ci/codec_cpu is a script
# directory, not a package.
MODULE_PATH = Path(__file__).resolve().parents[1] / "codec_cpu" / "device_profile.py"
SPEC = importlib.util.spec_from_file_location("codec_cpu_device_profile", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
device_profile = importlib.util.module_from_spec(SPEC)
# dataclass resolves the module through sys.modules, so register it first.
sys.modules[SPEC.name] = device_profile
SPEC.loader.exec_module(device_profile)


PASS = (
    "MP3 CODEC TEST PASSED (2 streams, 11 frames, 20736 samples, "
    "combined_fnv1a=0xc6b632ab, bit-exact)\n"
    "   decode 44992 us for 235101 us of audio -> 5.23x real time\n"
)

WITH_HELIX = (
    "Layer III only -- minimp3 39839 us, helix 35284 us"
    "  ->  minimp3-fixed is 1.130x helix (13% slower)  [both decoded 8 frames]\n" + PASS
)


def test_master_run_without_helix_leg_is_a_measurement() -> None:
    got = device_profile.parse_run(PASS)
    assert isinstance(got, device_profile.Measurement)
    assert got.l3_us is None, "no Helix leg means no Layer III time"
    assert got.decode_us == 44992
    assert got.audio_us == 235101
    assert got.realtime == 5.23
    assert got.helix_us is None
    assert got.ratio is None
    assert got.fnv1a == "c6b632ab"


def test_helix_leg_is_used_when_present() -> None:
    got = device_profile.parse_run(WITH_HELIX)
    assert isinstance(got, device_profile.Measurement)
    assert got.l3_us == 39839
    assert got.helix_us == 35284
    assert got.ratio == 1.13
    assert got.decode_us == 44992


def test_missing_checksum_is_refused() -> None:
    out = PASS.replace("combined_fnv1a=0xc6b632ab, ", "")
    assert device_profile.parse_run(out) is None


def test_transport_failure_is_retryable() -> None:
    out = "MP3 CODEC TEST TIMEOUT\n"
    assert device_profile.parse_run(out) is device_profile.TRANSPORT_FAILURE


def test_suppressed_ratio_is_not_a_number() -> None:
    assert (
        device_profile.parse_run("not the same work, ratio suppressed\n" + PASS) is None
    )


def test_mixed_helix_and_non_helix_runs_are_refused(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """main() must refuse to average a Layer III leg with a fixture decode.

    A firmware does not grow or lose its Helix leg between runs of one
    invocation, so runs that disagree mean a truncated transcript. The two
    quantities differ by 13% here, which is an order of magnitude more than
    the deltas mp3measure exists to resolve."""
    transcripts = iter([WITH_HELIX, PASS])

    def run_once(board: str, timeout: int) -> object:
        return device_profile.parse_run(next(transcripts))

    monkeypatch.setattr(device_profile, "run_once", run_once)
    assert device_profile.main(["--runs", "2", "--retries", "0"]) == 1


def test_single_run_without_helix_reports_the_fixture_time(
    monkeypatch: pytest.MonkeyPatch, capsys: pytest.CaptureFixture[str]
) -> None:
    """The --runs 1 path has no second run to cross-check against, so the
    label has to be right on its own. l3_us is None here, and the summary must
    say so rather than presenting 44,992 us as a Layer III time."""
    monkeypatch.setattr(
        device_profile,
        "run_once",
        lambda board, timeout: device_profile.parse_run(PASS),
    )
    assert device_profile.main(["--runs", "1", "--retries", "0"]) == 0
    out = capsys.readouterr().out
    assert "44,992" in out
    assert "minimp3 fixt" in out, "a fixture time must not be labelled L3"
    assert "minimp3 L3" not in out


def test_truncated_helix_line_does_not_become_an_l3_time() -> None:
    """The regression that motivated splitting l3_us from decode_us: a
    Helix-enabled transcript whose Layer III line was lost on the serial link
    used to yield the whole-fixture time under the l3_us name."""
    got = device_profile.parse_run(PASS)
    assert isinstance(got, device_profile.Measurement)
    assert got.l3_us is None, "absent means absent, never a fixture-time stand-in"
    assert got.decode_us == 44992
