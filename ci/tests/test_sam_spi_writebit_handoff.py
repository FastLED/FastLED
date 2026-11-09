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
    lines: list[str] = SAM_SPI_HEADER.read_text(encoding="utf-8").splitlines()
    start: int = next(i for i, line in enumerate(lines) if SIGNATURE in line)
    end: int = next(i for i in range(start + 1, len(lines)) if lines[i] == "\t}")
    body: list[str] = lines[start : end + 1]
    return "\n".join(re.sub(r"//.*", "", line) for line in body)


def test_write_bit_reenables_spi_before_releasing_pads() -> None:
    body: str = _write_bit_body()
    assert "PIO_PDR" in body, "writeBit must hand the pads back to peripheral A"
    assert "enableSPI();" in body, "writeBit must re-enable the SPI"
    pdr: int = body.index("PIO_PDR")
    enable: int = body.rindex("enableSPI();")
    assert enable < pdr, (
        "enableSPI() must run before PIO_PDR so SCK is never undriven (#5181)"
    )
    assert "disableSPI();" not in body[enable:pdr], (
        "the SPI must stay enabled between enableSPI() and PIO_PDR (#5181)"
    )
    csr: int = body.rfind("setTransferBits(")
    assert 0 <= csr < enable, (
        "channel 0's CSR (CPOL=0) must be restored before enableSPI() (#5181)"
    )
    assert body.startswith("setTransferBits(8);", csr), (
        "channel 0's CSR must be restored to 8-bit width (#5181)"
    )
    mask: int = body.rfind("__disable_irq();")
    assert 0 <= mask < csr, "interrupts must be masked across the handoff (#5181)"
    assert "__get_PRIMASK()" in body[:mask], (
        "the caller's interrupt state must be saved before masking (#5181)"
    )
    assert body.find("__set_PRIMASK(", pdr) > pdr, (
        "the caller's interrupt state must be restored after PIO_PDR (#5181)"
    )
    assert "readyTransferBits(" not in body[:enable], (
        "readyTransferBits() polls TDRE, which is zero while the SPI is "
        "disabled, so it would hang before enableSPI() (#5181)"
    )
    assert body.rindex("FastPin<_CLOCK_PIN>::lo();") < pdr, (
        "SCK must be driven low before the pads are released (#5181)"
    )
