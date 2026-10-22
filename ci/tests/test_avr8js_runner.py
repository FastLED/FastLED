"""AVR8JS verdicts must distinguish intentional smoke duration from failures."""

import pytest

from ci.runners.avr8js_runner import avr8js_run_succeeded


@pytest.mark.parametrize(
    ("example", "code", "output", "expected"),
    [
        ("Test", 0, "Test setup starting\nTest loop!", True),
        ("Test", 0, "Test setup starting", False),
        ("Test", 1, "Test setup starting\nTest loop!", False),
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
