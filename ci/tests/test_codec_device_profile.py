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
    assert got.l3_us == 44992
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
