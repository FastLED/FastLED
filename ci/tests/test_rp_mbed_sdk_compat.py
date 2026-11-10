"""Source contracts supplement the real Mbed and Arduino-Pico consumer builds."""

import re
from pathlib import Path


ROOT: Path = Path(__file__).resolve().parents[2]
RP: Path = ROOT / "src/platforms/arm/rp"


def test_sdk_family_is_not_inferred_from_shared_rp2040_target() -> None:
    sdk: str = (RP / "rpcommon/sdk_compat.h").read_text(encoding="utf-8")
    # Both cores can define TARGET_RP2040; only Mbed renames these SDK APIs.
    assert "#if defined(ARDUINO_ARCH_MBED)" in sdk
    assert "#if defined(TARGET_RP2040)" not in sdk
    mbed: str = sdk.split("#if defined(ARDUINO_ARCH_MBED)", 1)[1].split("#else", 1)[0]
    pico: str = sdk.split("#else", 1)[1].split("#endif", 1)[0]
    assert "_gpio_init(pin)" in mbed
    assert "_spi_init(spi, baud)" in mbed
    assert "uart_get_dreq(uart, tx)" in mbed
    assert "GPIO_FUNC_UART" in mbed
    assert "gpio_init(pin)" in pico
    assert "spi_init(spi, baud)" in pico
    assert "uart_get_dreq_num(uart, tx)" in pico
    assert "UART_FUNCSEL_NUM(uart, pin)" in pico


def test_disabled_sdk_alarm_pool_is_an_explicit_timer_limitation() -> None:
    sdk: str = (RP / "rpcommon/sdk_compat.h").read_text(encoding="utf-8")
    isr: str = (RP / "isr_rp2040.hpp").read_text(encoding="utf-8")
    disabled: str = sdk.split("#if PICO_TIME_DEFAULT_ALARM_POOL_DISABLED", 1)[1].split(
        "#else", 1
    )[0]
    assert "timerAvailable()" in disabled and "return false;" in disabled
    assert "add_alarm_in_us(" not in disabled
    assert "cancel_alarm(" not in disabled
    assert "#undef PICO_TIME_DEFAULT_ALARM_POOL_DISABLED" not in sdk
    assert re.search(r"if \(!rp_sdk::timerAvailable\(\)\).*?return -6;", isr, re.DOTALL)
    assert 'case -6: return "Timer alarm pool unavailable";' in isr
    assert "rp_sdk::timerAvailable() ? 1000000 : 0" in isr
    assert "rp_sdk::timerAvailable() ? 1 : 0" in isr


def test_platform_consumers_use_the_sdk_dispatch_boundary() -> None:
    paths: tuple[str, ...] = (
        "pin_rp.hpp",
        "pin_rp_native.hpp",
        "isr_rp2040.hpp",
        "rpcommon/channel_engine_rp_bitbang.cpp.hpp",
        "rpcommon/rp_spi_peripheral.cpp.hpp",
        "rpcommon/rp_uart_peripheral.cpp.hpp",
        "rpcommon/rx_pio_channel.cpp.hpp",
    )
    raw_call: re.Pattern[str] = re.compile(
        r"\b(?:gpio_init|spi_init|uart_get_dreq_num|UART_FUNCSEL_NUM|add_alarm_in_us|cancel_alarm)\s*\("
    )
    for path in paths:
        source: str = (RP / path).read_text(encoding="utf-8")
        assert '"platforms/arm/rp/rpcommon/sdk_compat.h"' in source, path
        assert raw_call.search(source) is None, path
