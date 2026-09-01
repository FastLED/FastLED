from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "ci" / "codec_tables" / "generate_minimp3_fixed_tables.py"
SPEC = importlib.util.spec_from_file_location("minimp3_fixed_tables", MODULE_PATH)
assert SPEC and SPEC.loader
TABLES = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = TABLES
SPEC.loader.exec_module(TABLES)


def test_generated_constants_agree_with_upstream_floats() -> None:
    """The generator derives every constant from its ISO formula; upstream
    minimp3's own CC0 float literals are the independent witness that the
    formula was read correctly. A mistranslation lands orders of magnitude out,
    not a few ulp, so this is the check that would catch it."""
    assert TABLES.cross_check() == []


def test_committed_header_matches_the_generator() -> None:
    """`minimp3_fixed_tables.h` is generated. If it drifts from the generator,
    the constants in the build stop being the ones the formulas produce, and
    the cross-check above stops meaning anything."""
    assert TABLES.OUTPUT.exists(), f"{TABLES.OUTPUT} is missing; run --write"
    assert TABLES.OUTPUT.read_text(encoding="utf-8") == TABLES.render()


def test_normalize_round_trips_across_the_decoder_dynamic_range() -> None:
    """Scalefactor gains span roughly 2**-181 to 2**10 and x**(4/3) reaches
    165000; the mantissa/exponent split has to stay exact across all of it."""
    for exponent in range(-200, 32):
        value = 2.0**exponent * 1.3
        mant, exp = TABLES.normalize(value)
        assert (1 << 30) <= abs(mant) < (1 << 31)
        assert abs(mant * 2.0 ** (exp - 30) - value) / value < 2.0**-29

    assert TABLES.normalize(0.0) == (0, 0)

    # Negative inputs matter: the x**(4/3) table folds the sign into the value.
    mant, exp = TABLES.normalize(-16.0)
    assert mant < 0
    assert mant * 2.0 ** (exp - 30) == -16.0
