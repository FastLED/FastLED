"""AVR8JS verdicts must distinguish intentional smoke duration from failures."""

import pytest

from ci.runners.avr8js_runner import (
    NOISE_HASH_MISMATCH,
    NOISE_HASH_OK,
    POLYGON_HASH_MISMATCH,
    POLYGON_HASH_OK,
    avr8js_run_succeeded,
)


OK = f"{POLYGON_HASH_OK}\n{NOISE_HASH_OK}"
BAD = f"polygon scene 3 exact 0x0 {POLYGON_HASH_MISMATCH}, host 0x1"
NOISE_BAD = f"noise case 1 0x0 {NOISE_HASH_MISMATCH}, host 0x1"


@pytest.mark.parametrize(
    ("example", "code", "output", "expected"),
    [
        ("Test", 0, f"Test setup starting\n{OK}\nTest loop!", True),
        ("Test", 0, "Test setup starting", False),
        ("Test", 1, f"Test setup starting\n{OK}\nTest loop!", False),
        # The drawPolygon AVR-vs-host hash check must run and match (#5324).
        ("Test", 0, "Test setup starting\nTest loop!", False),
        ("Test", 0, f"Test setup starting\n{BAD}\n{OK}\nTest loop!", False),
        # The fill_noise16 AVR-vs-host check must run and match (#5330).
        ("Test", 0, f"Test setup starting\n{POLYGON_HASH_OK}\nTest loop!", False),
        ("Test", 0, f"Test setup starting\n{NOISE_BAD}\n{OK}\nTest loop!", False),
        ("Blink", 0, "avr8js test-emu passed", True),
        ("Blink", 1, "avr8js ATmega328P test-emu failed: timed out", True),
        ("Test", 1, "avr8js ATmega328P test-emu failed: timed out", False),
        ("Blink", 1, "build failed", False),
        ("Blink", 124, "avr8js ATmega328P test-emu failed: timed out", False),
    ],
)
def test_avr8js_run_verdict(
    example: str, code: int, output: str, expected: bool
) -> None:
    assert avr8js_run_succeeded(example, code, output) is expected
