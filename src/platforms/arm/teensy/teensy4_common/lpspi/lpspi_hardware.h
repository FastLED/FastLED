#pragma once

// IWYU pragma: private

/// @file platforms/arm/teensy/teensy4_common/lpspi/lpspi_hardware.h
/// Per-bus LPSPI pin/mux tables for Teensy 4.x (IMXRT1062).
///
/// VENDORED from the Teensyduino SPI library, `libraries/SPI/SPI.cpp`
/// (Teensyduino 1.60 / framework-arduinoteensy 1.160.0), MIT licensed by
/// Paul Stoffregen / PJRC. Same local-fork precedent as
/// `platforms/arm/teensy/audio/pjrc_*` and `platforms/arm/teensy/sdfat/`.
///
/// Why fork rather than link the framework library: fbuild will not let a
/// local library's headers select a framework library, so `<SPI.h>` simply
/// never entered the link and every `SpiHw*MXRT1062` symbol came up undefined
/// (FastLED#3775). Declaring the dependency is also not what we want -- the
/// framework's three global `SPIClass` objects cost ~600 bytes of .bss each
/// and cannot be stripped once anything names them, which is why the drivers
/// now select a bus by integer index instead (FastLED#3777).
///
/// The tables below are copied VERBATIM so the register values stay the
/// battle-tested ones. Two deliberate deviations, both marked inline:
///   1. The DMA triple (`tx_dma_channel`, `rx_dma_channel`, `dma_rxisr`) is
///      dropped. Nothing on the FastLED path reads it, and keeping the
///      `_spi_dma_rxISR*` function pointers would anchor the upstream DMA and
///      EventResponder machinery into the link for no benefit.
///   2. Types are FastLED's `fl::` fixed-width aliases.
/// Everything else -- pin numbers, mux values, select-input registers, the
/// T4.0/T4.1 split -- is unchanged. Do not "tidy" these numbers; a wrong mux
/// links fine and silently drives nothing.

#include "fl/stl/int.h"
#include "fl/stl/compiler_control.h"

namespace fl {
namespace platforms {
namespace teensy {

// Pin-count arities differ between T4.0 and T4.1: the T4.1 exposes a second
// set of LPSPI pins on the memory connectors and the SD socket. Upstream
// SPI.h:1079-1090.
#if defined(ARDUINO_TEENSY41)
static const fl::u8 CNT_MISO_PINS = 2;
static const fl::u8 CNT_MOSI_PINS = 2;
static const fl::u8 CNT_SCK_PINS = 2;
static const fl::u8 CNT_CS_PINS = 3;
#else
static const fl::u8 CNT_MISO_PINS = 1;
static const fl::u8 CNT_MOSI_PINS = 1;
static const fl::u8 CNT_SCK_PINS = 1;
static const fl::u8 CNT_CS_PINS = 1;
#endif

/// Upstream `SPIClass::SPI_Hardware_t` (SPI.h:1091-1122), minus the DMA
/// triple. Field ORDER is preserved so the table initializers below stay a
/// 1:1 copy of upstream.
struct LpspiHardware {
    volatile fl::u32 &clock_gate_register;
    const fl::u32 clock_gate_mask;

    // MISO pins
    const fl::u8 miso_pin[CNT_MISO_PINS];
    const fl::u32 miso_mux[CNT_MISO_PINS];
    const fl::u8 miso_select_val[CNT_MISO_PINS];
    volatile fl::u32 &miso_select_input_register;

    // MOSI pins
    const fl::u8 mosi_pin[CNT_MOSI_PINS];
    const fl::u32 mosi_mux[CNT_MOSI_PINS];
    const fl::u8 mosi_select_val[CNT_MOSI_PINS];
    volatile fl::u32 &mosi_select_input_register;

    // SCK pins
    const fl::u8 sck_pin[CNT_SCK_PINS];
    const fl::u32 sck_mux[CNT_SCK_PINS];
    const fl::u8 sck_select_val[CNT_SCK_PINS];
    volatile fl::u32 &sck_select_input_register;

    // CS pins -- retained so the table layout matches upstream exactly, even
    // though FastLED drives chip-select itself and never calls setCS().
    const fl::u8 cs_pin[CNT_CS_PINS];
    const fl::u32 cs_mux[CNT_CS_PINS];
    const fl::u8 cs_mask[CNT_CS_PINS];
    const fl::u8 pcs_select_val[CNT_CS_PINS];
    volatile fl::u32 *pcs_select_input_register[CNT_CS_PINS];
};

// ============================================================================
// Hardware tables -- VERBATIM from upstream SPI.cpp, DMA line removed.
// LPSPI4 = SPI (index 0), LPSPI3 = SPI1 (index 1), LPSPI1 = SPI2 (index 2).
// ============================================================================

// Upstream SPI.cpp:1504-1548
#if defined(ARDUINO_TEENSY41)
/// Bus index -> hardware table. Deliberately a TEMPLATE with a function-local
/// `static`: that is the only shape that stays strippable. A namespace-scope
/// table (even `inline const`) is emitted and kept, which grew Teensy 4.1
/// Blink from 58,688 to 106,816 bytes of .text. See README.md.
template <int BUS>
const LpspiHardware &lpspi_hardware();

template <>
inline const LpspiHardware &lpspi_hardware<4>() {
    static const LpspiHardware kHw = {
        CCM_CCGR1, CCM_CCGR1_LPSPI4(CCM_CCGR_ON),
        {12, 255},  // MISO
        {3 | 0x10, 0},
        {0, 0},
        IOMUXC_LPSPI4_SDI_SELECT_INPUT,
        {11, 255},  // MOSI
        {3 | 0x10, 0},
        {0, 0},
        IOMUXC_LPSPI4_SDO_SELECT_INPUT,
        {13, 255},  // SCK
        {3 | 0x10, 0},
        {0, 0},
        IOMUXC_LPSPI4_SCK_SELECT_INPUT,
        {10, 37, 36},  // CS
        {3 | 0x10, 2 | 0x10, 2 | 0x10},
        {1, 2, 3},
        {0, 0, 0},
        {&IOMUXC_LPSPI4_PCS0_SELECT_INPUT, 0, 0}};
    return kHw;
}
#else
/// Bus index -> hardware table. Deliberately a TEMPLATE with a function-local
/// `static`: that is the only shape that stays strippable. A namespace-scope
/// table (even `inline const`) is emitted and kept, which grew Teensy 4.1
/// Blink from 58,688 to 106,816 bytes of .text. See README.md.
template <int BUS>
const LpspiHardware &lpspi_hardware();

template <>
inline const LpspiHardware &lpspi_hardware<4>() {
    static const LpspiHardware kHw = {
        CCM_CCGR1, CCM_CCGR1_LPSPI4(CCM_CCGR_ON),
        {12},
        {3 | 0x10},
        {0},
        IOMUXC_LPSPI4_SDI_SELECT_INPUT,
        {11},
        {3 | 0x10},
        {0},
        IOMUXC_LPSPI4_SDO_SELECT_INPUT,
        {13},
        {3 | 0x10},
        {0},
        IOMUXC_LPSPI4_SCK_SELECT_INPUT,
        {10},
        {3 | 0x10},
        {1},
        {0},
        {&IOMUXC_LPSPI4_PCS0_SELECT_INPUT}};
    return kHw;
}
#endif

// Upstream SPI.cpp:1559-1603
#if defined(ARDUINO_TEENSY41)
template <>
inline const LpspiHardware &lpspi_hardware<3>() {
    static const LpspiHardware kHw = {
        CCM_CCGR1, CCM_CCGR1_LPSPI3(CCM_CCGR_ON),
        {1, 39},
        {7 | 0x10, 2 | 0x10},
        {0, 1},
        IOMUXC_LPSPI3_SDI_SELECT_INPUT,
        {26, 255},
        {2 | 0x10, 0},
        {1, 0},
        IOMUXC_LPSPI3_SDO_SELECT_INPUT,
        {27, 255},
        {2 | 0x10, 0},
        {1, 0},
        IOMUXC_LPSPI3_SCK_SELECT_INPUT,
        {0, 38, 255},
        {7 | 0x10, 2 | 0x10, 0},
        {1, 1, 0},
        {0, 1, 0},
        {&IOMUXC_LPSPI3_PCS0_SELECT_INPUT, &IOMUXC_LPSPI3_PCS0_SELECT_INPUT,
         0}};
    return kHw;
}
#else
template <>
inline const LpspiHardware &lpspi_hardware<3>() {
    static const LpspiHardware kHw = {
        CCM_CCGR1, CCM_CCGR1_LPSPI3(CCM_CCGR_ON),
        {1},
        {7 | 0x10},
        {0},
        IOMUXC_LPSPI3_SDI_SELECT_INPUT,
        {26},
        {2 | 0x10},
        {1},
        IOMUXC_LPSPI3_SDO_SELECT_INPUT,
        {27},
        {2 | 0x10},
        {1},
        IOMUXC_LPSPI3_SCK_SELECT_INPUT,
        {0},
        {7 | 0x10},
        {1},
        {0},
        {&IOMUXC_LPSPI3_PCS0_SELECT_INPUT}};
    return kHw;
}
#endif

// Upstream SPI.cpp:1609-1653.
//
// Note the T4.0 pin set (34/35/37) differs from T4.1 (42/43/45). The previous
// FastLED code hardcoded the T4.1 numbers unconditionally in
// spi_hw_2_mxrt1062.cpp.hpp, which was a latent T4.0 bug; going through this
// table fixes it.
#if defined(ARDUINO_TEENSY41)
template <>
inline const LpspiHardware &lpspi_hardware<1>() {
    static const LpspiHardware kHw = {
        CCM_CCGR1, CCM_CCGR1_LPSPI1(CCM_CCGR_ON),
        {42, 54},
        {4 | 0x10, 3 | 0x10},
        {1, 0},
        IOMUXC_LPSPI1_SDI_SELECT_INPUT,
        {43, 50},
        {4 | 0x10, 3 | 0x10},
        {1, 0},
        IOMUXC_LPSPI1_SDO_SELECT_INPUT,
        {45, 49},
        {4 | 0x10, 3 | 0x10},
        {1, 0},
        IOMUXC_LPSPI1_SCK_SELECT_INPUT,
        {44, 255, 255},
        {4 | 0x10, 0, 0},
        {1, 0, 0},
        {0, 0, 0},
        {&IOMUXC_LPSPI1_PCS0_SELECT_INPUT, 0, 0}};
    return kHw;
}
#else
template <>
inline const LpspiHardware &lpspi_hardware<1>() {
    static const LpspiHardware kHw = {
        CCM_CCGR1, CCM_CCGR1_LPSPI1(CCM_CCGR_ON),
        {34},
        {4 | 0x10},
        {1},
        IOMUXC_LPSPI1_SDI_SELECT_INPUT,
        {35},
        {4 | 0x10},
        {1},
        IOMUXC_LPSPI1_SDO_SELECT_INPUT,
        {37},
        {4 | 0x10},
        {1},
        IOMUXC_LPSPI1_SCK_SELECT_INPUT,
        {36},
        {4 | 0x10},
        {1},
        {0},
        {&IOMUXC_LPSPI1_PCS0_SELECT_INPUT}};
    return kHw;
}
#endif

}  // namespace teensy
}  // namespace platforms
}  // namespace fl
