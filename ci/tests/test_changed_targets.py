from ci.changed_targets import COMMON, affected, affects_platform, classify


def test_platform_directories_select_their_family() -> None:
    assert classify("src/platforms/esp/8266/fastpin_esp8266.h") == "esp8266"
    assert classify("src/platforms/esp/32/rmt/rmt_5.h") == "esp32"
    assert classify("src/platforms/avr/fastpin_avr.h") == "avr"
    assert classify("src/platforms/arm/teensy/fastpin_teensy.h") == "teensy"
    assert classify("src/platforms/arm/rp/rp2040/fastpin_rp2040.h") == "rp"
    assert classify("src/platforms/arm/nrf52/clock.h") == "nrf52"
    assert classify("src/platforms/arm/stm32/clock.h") == "stm32"


def test_shared_esp_headers_affect_both_esp_families() -> None:
    assert classify("src/platforms/esp/clockless.h") == "esp"


def test_common_code_covers_dispatchers_shared_and_libraries() -> None:
    assert classify("src/fl/chipsets/apa102.h") == COMMON
    assert classify("src/third_party/minimp3/minimp3.h") == COMMON
    assert classify("src/platforms/spi_output_template.h") == COMMON
    assert classify("src/platforms/shared/spi_pixel_writer.h") == COMMON
    # An unrecognized platform directory is measured rather than skipped.
    assert classify("src/platforms/arm/kinetis/clock.h") == COMMON


def test_stub_and_wasm_are_ignored() -> None:
    assert classify("src/platforms/wasm/fastpin_wasm.h") is None
    assert classify("src/platforms/stub/led_sysdefs_stub.h") is None


def test_affected_splits_common_from_platforms() -> None:
    assert affected(["src/platforms/esp/8266/fastpin_esp8266.h"]) == (
        False,
        frozenset({"esp8266"}),
    )
    common, platforms = affected(
        ["src/fl/chipsets/apa102.h", "src/platforms/arm/teensy/fastpin_teensy.h"]
    )
    assert common is True
    assert platforms == frozenset({"teensy"})
    assert affected(["src/platforms/wasm/x.h"]) == (False, frozenset())


def test_affects_platform_counts_common_code() -> None:
    assert affects_platform(["src/platforms/esp/8266/fastpin_esp8266.h"], "esp8266")
    assert not affects_platform(["src/platforms/esp/8266/fastpin_esp8266.h"], "esp32")
    # Common code can move any platform's image.
    assert affects_platform(["src/fl/chipsets/apa102.h"], "esp32")
    assert not affects_platform(["src/platforms/wasm/x.h"], "esp32")
