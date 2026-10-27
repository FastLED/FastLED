from ci.memory_report import ALL_BOARDS, MARKER, plan, render


def test_render_is_one_table_with_size_then_colored_delta() -> None:
    base = {
        "uno": {"image_flash": 3600, "total_ram": 134},
        "esp32s3": {"image_flash": 326_023, "total_ram": 39_193},
    }
    head = {
        "uno": {"image_flash": 3660, "total_ram": 134},
        "esp32s3": {"image_flash": 320_655, "total_ram": 38_623},
    }
    out = render(base, head, "Blink")
    assert out.startswith(MARKER)
    # One table only: the absolute sizes live in the same cell as the delta.
    assert out.count("|---|---:|---:|") == 1
    assert "Absolute sizes" not in out
    # Increase is red, decrease is green, unchanged is green.
    assert "| uno | 3,660 (🔴 +60) | 134 (🟢 0) |" in out
    assert "| esp32s3 | 320,655 (🟢 -5,368) | 38,623 (🟢 -570) |" in out


def test_render_lists_missing_boards() -> None:
    base = {
        "esp32c6": {"image_flash": 366_614, "total_ram": 19_544},
        "rp2040": {"image_flash": 1, "total_ram": 1},
    }
    head = {"esp32c6": {"image_flash": 348_182, "total_ram": 19_544}}
    out = render(base, head, "Blink")
    assert "| esp32c6 | 348,182 (🟢 -18,432) | 19,544 (🟢 0) |" in out
    assert "`rp2040`" in out


def test_plan_measures_only_the_changed_platform() -> None:
    assert plan(["src/platforms/esp/8266/fastspi_esp8266_hspi.h"], [], False) == [
        "esp8266"
    ]
    assert plan(["src/platforms/arm/teensy/fastpin.h"], [], False) == ["teensy41"]
    assert plan(["src/platforms/esp/32/rmt.h"], [], False) == ["esp32dev", "esp32s3"]


def test_plan_common_code_falls_back_to_avr() -> None:
    assert plan(["src/fl/chipsets/apa102.h"], [], False) == ["attiny85"]
    assert plan(["src/third_party/minimp3/minimp3.h"], [], False) == ["attiny85"]
    # Common plus a platform change measures both.
    assert plan(
        ["src/fl/chipsets/apa102.h", "src/platforms/esp/8266/fastpin_esp8266.h"],
        [],
        False,
    ) == ["attiny85", "esp8266"]


def test_plan_ignores_stub_and_wasm() -> None:
    assert plan(["src/platforms/wasm/fastpin_wasm.h"], [], False) == []
    assert plan(["src/platforms/stub/led_sysdefs_stub.h"], [], False) == []
    assert (
        plan(
            ["src/platforms/wasm/fastpin_wasm.h", "src/platforms/stub/clock_stub.h"],
            [],
            False,
        )
        == []
    )


def test_plan_labels_are_additive() -> None:
    boards = plan(["src/platforms/esp/8266/fastpin_esp8266.h"], ["ci-full"], False)
    assert boards == sorted(ALL_BOARDS)
    assert plan(["src/fl/x.h"], ["ci-platform:teensy41"], False) == [
        "attiny85",
        "teensy41",
    ]
    assert plan(["src/fl/x.h"], ["ci-platform:esp*"], False) == [
        "attiny85",
        "esp32dev",
        "esp32s3",
        "esp8266",
    ]
    assert plan(["src/fl/x.h"], ["ci-test:bloat"], False) == sorted(ALL_BOARDS)


def test_plan_everything_is_the_dispatch_path() -> None:
    assert plan([], [], True) == list(ALL_BOARDS)
