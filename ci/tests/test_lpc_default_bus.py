"""LPC DefaultBus names only channel drivers that exist (#5260).

`FastLED.add(cfg)` binds `BusTraits<DefaultBus<Chipset>::value>` when no bus
was enabled, so a DefaultBus with no matching BusTraits breaks every LPC build.
LPC has no channels-API SPI driver, and only LPC845 has a clockless engine
(USART+DMA, or SCT+DMA under FASTLED_LPC_PWM_DMA).
"""

from pathlib import Path


ROOT: Path = Path(__file__).resolve().parents[2]


def _lpc_block() -> str:
    text: str = (ROOT / "src" / "fl" / "channels" / "bus.h").read_text(encoding="utf-8")
    # The LPC branch ends at the next platform branch (RP2040 / RP2350).
    lpc: str = text.split("#elif defined(FL_IS_ARM_LPC)\n", 1)[1]
    return lpc.split("#elif defined(FL_IS_RP2040)", 1)[0]


def test_lpc_has_no_spi_default_bus() -> None:
    assert "DefaultBus<SpiChipsetConfig>" not in _lpc_block().replace(
        "DefaultBus<SpiChipsetConfig> stays", ""
    )


def test_lpc_clockless_default_bus_only_on_lpc845_engines() -> None:
    code: list[str] = [
        line.strip()
        for line in _lpc_block().splitlines()
        if line.strip() and not line.strip().startswith("//")
    ]
    assert code == [
        "#if defined(FL_IS_ARM_LPC_845) && FASTLED_LPC_UART_DMA",
        "template<> struct DefaultBus<ClocklessChipset> {",
        "static constexpr Bus value = Bus::UART;",
        "};",
        "#elif defined(FL_IS_ARM_LPC_845) && defined(FASTLED_LPC_PWM_DMA)",
        "template<> struct DefaultBus<ClocklessChipset> {",
        "static constexpr Bus value = Bus::BIT_BANG;",
        "};",
        "#endif",
    ]
