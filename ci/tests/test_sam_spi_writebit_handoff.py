"""Regression contract for the SAM writeBit SPI handoff ordering (FastLED #5181).

``SAMHardwareSPIOutput::writeBit()`` hands SCK/MOSI to the PIO controller to
bit-bang one bit (#5117), then returns them to peripheral A. If ``PIO_PDR``
releases the pads before ``enableSPI()`` runs, the SCK pad is undriven for a
moment; a clock pull-up (or an interrupt that lengthens the window) can make an
extra rising edge, which an SM16716 counts as a data bit, shifting the RGB
bytes that follow.

This is a hardware-only ordering contract (Arduino Due + SM16716 strip) that
cannot run on the host, so it is checked at source level: the SPI must be
re-enabled, with SCK already driven low, before the pads are released.
"""

from __future__ import annotations

import re
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SAM_SPI_HEADER = REPO_ROOT / "src" / "platforms" / "arm" / "sam" / "fastspi_arm_sam.h"
SIGNATURE = "template <u8 BIT> inline void writeBit("


def _write_bit_body() -> str:
    lines = SAM_SPI_HEADER.read_text(encoding="utf-8").splitlines()
    start = next(i for i, line in enumerate(lines) if SIGNATURE in line)
    end = next(i for i in range(start + 1, len(lines)) if lines[i] == "\t}")
    body = lines[start : end + 1]
    return "\n".join(re.sub(r"//.*", "", line) for line in body)


def test_write_bit_reenables_spi_before_releasing_pads() -> None:
    body = _write_bit_body()
    assert "PIO_PDR" in body, "writeBit must hand the pads back to peripheral A"
    assert "enableSPI();" in body, "writeBit must re-enable the SPI"
    pdr = body.index("PIO_PDR")
    assert body.rindex("enableSPI();") < pdr, (
        "enableSPI() must run before PIO_PDR so SCK is never undriven (#5181)"
    )
    assert body.rindex("FastPin<_CLOCK_PIN>::lo();") < pdr, (
        "SCK must be driven low before the pads are released (#5181)"
    )
