"""Regression contract for ESP32 RMT4 reset/latch timing."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
HEADER = (
    ROOT
    / "src"
    / "platforms"
    / "esp"
    / "32"
    / "drivers"
    / "rmt"
    / "rmt_4"
    / "channel_driver_rmt4.h"
)
IMPLEMENTATION = HEADER.with_suffix(".cpp.hpp")
CLOCKLESS = HEADER.parent / "idf4_clockless_rmt_esp32.h"


def test_rmt4_keeps_idle_low_until_chipset_reset_time_elapses() -> None:
    header = HEADER.read_text(encoding="utf-8")
    implementation = IMPLEMENTATION.read_text(encoding="utf-8")
    tx_done = header[
        header.index("FL_NO_INLINE IRAM_ATTR void onTxDoneInterrupt") : header.index(
            "FL_NO_INLINE IRAM_ATTR void fillNextBuffer"
        )
    ]

    assert "resetStartTimeUs" in header
    assert "resetDurationUs" in header
    assert "fl::atomic_bool transmissionComplete" in header
    assert "gpio_matrix_out" not in tx_done
    assert "transmissionComplete.store(true, fl::memory_order_release)" in tx_done
    assert "state->resetDurationUs = data->getTiming().reset_us;" in implementation
    assert "transmissionComplete.load(fl::memory_order_acquire)" in implementation
    assert "resetElapsedUs < state.resetDurationUs" in implementation


def test_rmt4_legacy_wait_time_can_only_extend_trait_reset_time() -> None:
    source = CLOCKLESS.read_text(encoding="utf-8")

    assert "static_cast<u32>(WAIT_TIME) > timing.reset_us" in source
    assert "timing.reset_us = static_cast<u32>(WAIT_TIME);" in source
