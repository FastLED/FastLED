"""Guard the units at the ESP-IDF PARLIO TX wait boundary."""

from pathlib import Path


SOURCE = (
    Path(__file__).resolve().parents[2]
    / "src/platforms/esp/32/drivers/parlio/parlio_peripheral_esp.cpp.hpp"
)


def test_parlio_tx_wait_preserves_milliseconds_for_esp_idf() -> None:
    source = SOURCE.read_text(encoding="utf-8")
    wait = source.split("bool ParlioPeripheralESPImpl::waitAllDone(", 1)[1]
    wait = wait.split("\n}", 1)[0]
    assert "pdMS_TO_TICKS" not in wait
    assert "parlio_tx_unit_wait_all_done(mTxUnit, sdk_timeout_ms)" in wait


def test_parlio_teardown_passes_a_one_second_timeout_to_esp_idf() -> None:
    source = SOURCE.read_text(encoding="utf-8")
    destructor = source.split("ParlioPeripheralESPImpl::~ParlioPeripheralESPImpl()", 1)[
        1
    ]
    destructor = destructor.split("\n}", 1)[0]
    assert "parlio_tx_unit_wait_all_done(mTxUnit, 1000)" in destructor
