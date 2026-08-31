from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SDFAT_ROOT = PROJECT_ROOT / "src/platforms/arm/teensy/sdfat"


def test_teensy_sdfat_has_no_arduino_spi_fallback() -> None:
    forbidden = (
        "FL_SDFAT_USE_ARDUINO_SPI",
        "SPIClass",
        "SPISettings",
        "m_spi = &SPI",
    )
    violations: list[str] = []

    for path in SDFAT_ROOT.rglob("*"):
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8")
        for token in forbidden:
            if token in text:
                violations.append(f"{path.relative_to(PROJECT_ROOT)}: {token}")

    assert not violations, "Arduino SPI fallback remains:\n" + "\n".join(violations)
