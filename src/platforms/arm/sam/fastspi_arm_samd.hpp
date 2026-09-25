#pragma once

// IWYU pragma: private

namespace fl {

template <u8 DATA_PIN, u8 CLOCK_PIN, u32 SPI_CLOCK_DIVIDER>
SERCOM *SAMDHardwareSPIOutput<DATA_PIN, CLOCK_PIN,
                             SPI_CLOCK_DIVIDER>::peripheral() FL_NO_EXCEPT {
    return &PERIPH_SPI;
}

template <u8 DATA_PIN, u8 CLOCK_PIN, u32 SPI_CLOCK_DIVIDER>
u32 SAMDHardwareSPIOutput<DATA_PIN, CLOCK_PIN,
                          SPI_CLOCK_DIVIDER>::clockHz() FL_NO_EXCEPT {
    const u32 clock_hz = F_CPU / SPI_CLOCK_DIVIDER;
    return clock_hz > 24000000 ? 24000000 : clock_hz;
}

template <u8 DATA_PIN, u8 CLOCK_PIN, u32 SPI_CLOCK_DIVIDER>
void SAMDHardwareSPIOutput<DATA_PIN, CLOCK_PIN,
                           SPI_CLOCK_DIVIDER>::configureClock() FL_NO_EXCEPT {
    // PERIPH_SPI is shared by every template instance. Restore this
    // controller's requested rate whenever it acquires the peripheral.
    peripheral()->disableSPI();
    peripheral()->initSPI(PAD_SPI_TX, PAD_SPI_RX, SPI_CHAR_SIZE_8_BITS,
                          MSB_FIRST);
    peripheral()->initSPIClock(SERCOM_SPI_MODE_0, clockHz());
    peripheral()->enableSPI();
}

template <u8 DATA_PIN, u8 CLOCK_PIN, u32 SPI_CLOCK_DIVIDER>
void SAMDHardwareSPIOutput<DATA_PIN, CLOCK_PIN,
                           SPI_CLOCK_DIVIDER>::init() FL_NO_EXCEPT {
    if (mInitialized) {
        return;
    }

    pinPeripheral(PIN_SPI_MISO, g_APinDescription[PIN_SPI_MISO].ulPinType);
    pinPeripheral(PIN_SPI_SCK, g_APinDescription[PIN_SPI_SCK].ulPinType);
    pinPeripheral(PIN_SPI_MOSI, g_APinDescription[PIN_SPI_MOSI].ulPinType);
    configureClock();

    mInitialized = true;
}

template <u8 DATA_PIN, u8 CLOCK_PIN, u32 SPI_CLOCK_DIVIDER>
void SAMDHardwareSPIOutput<DATA_PIN, CLOCK_PIN,
                           SPI_CLOCK_DIVIDER>::select() FL_NO_EXCEPT {
    configureClock();
    if (mPSelect != nullptr) {
        mPSelect->select();
    }
}

} // namespace fl
