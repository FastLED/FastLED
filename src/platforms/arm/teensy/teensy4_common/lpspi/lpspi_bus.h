#pragma once

// IWYU pragma: private

/// @file platforms/arm/teensy/teensy4_common/lpspi/lpspi_bus.h
/// Minimal LPSPI bus control for Teensy 4.x, forked from Teensyduino's
/// `SPIClass` so FastLED does not depend on the Arduino `SPI` library.
///
/// See lpspi_hardware.h for provenance and the reasoning. This header carries
/// only what FastLED's SPI drivers actually call:
///
///     setSCK / setMOSI / setMISO   pad mux + input-select routing
///     begin / end                  clock gate + pad drive strength
///     beginTransaction / endTransaction   CCR divisor + CR/CFGR1/TCR
///
///     transfer                     blocking byte / block exchange
///     setTransferWriteFill         fill byte for a null tx buffer
///
/// Everything else in upstream SPIClass -- DMA, async, setCS, usingInterrupt,
/// the deprecated setters -- is deliberately absent. FastLED's LED drivers
/// drive the data path themselves via direct register access (see
/// spi_hw_4_mxrt1062.cpp.hpp's transmit loop polling LPSPI_SR_TDF); the
/// blocking `transfer` overloads exist for the SdFat transport, which needs
/// a real full-duplex read.
///
/// One behavioural simplification vs upstream: `beginTransaction` /
/// `endTransaction` do NOT save and restore the NVIC interrupt masks. That
/// path is gated on `interruptMasksUsed`, which upstream only ever sets from
/// `usingInterrupt()`. FastLED never calls it, so the masks are always zero
/// and the whole block is dead. Dropping it keeps `endTransaction` a no-op.

#include "fl/stl/int.h"
#include "fl/stl/singleton.h"
#include "platforms/arm/teensy/teensy4_common/lpspi/lpspi_hardware.h"

// SPI mode constants, normally supplied by the Arduino <SPI.h> we no longer
// include. Values are upstream's (SPI.h:51-54) and are load-bearing: the TCR
// assembly below tests bit 3 for CPOL and bit 2 for CPHA. Guarded because the
// SD-card path still pulls in the framework header, so both can be in scope
// in one translation unit.
#ifndef SPI_MODE0
#define SPI_MODE0 0x00
#define SPI_MODE1 0x04
#define SPI_MODE2 0x08
#define SPI_MODE3 0x0C
#endif

namespace fl {
namespace platforms {
namespace teensy {

/// Transaction parameters. Replaces upstream `SPISettings`
/// (SPI.h:1045-1075); the TCR assembly is copied from its
/// `init_AlwaysInline`.
class LpspiSettings {
  public:
    LpspiSettings() : mClock(4000000) { init(MSBFIRST, SPI_MODE0); }

    LpspiSettings(fl::u32 clock, fl::u8 bit_order, fl::u8 data_mode)
        : mClock(clock) {
        init(bit_order, data_mode);
    }

    fl::u32 clock() const { return mClock; }
    fl::u32 tcr() const { return mTcr; }

  private:
    void init(fl::u8 bit_order, fl::u8 data_mode) {
        mTcr = LPSPI_TCR_FRAMESZ(7);  // TCR carries polarity and bit order too
        if (bit_order == LSBFIRST) {
            mTcr |= LPSPI_TCR_LSBF;
        }
        if (data_mode & 0x08) {
            mTcr |= LPSPI_TCR_CPOL;
        }
        if (data_mode & 0x04) {
            mTcr |= LPSPI_TCR_CPHA;
        }
    }

    fl::u32 mClock;
    fl::u32 mTcr = 0;
};

/// One LPSPI peripheral. Construct via `LpspiBus::get(index)`.
///
/// `index` matches the legacy Arduino naming the drivers already use:
///   0 -> LPSPI4 (was `SPI`), 1 -> LPSPI3 (was `SPI1`), 2 -> LPSPI1 (was `SPI2`)
class LpspiBus {
  public:
    /// Bus for a legacy Arduino SPI index. Returns bus 0 for out-of-range.
    static LpspiBus &get(fl::u8 index);

    void setSCK(fl::u8 pin);
    void setMOSI(fl::u8 pin);
    void setMISO(fl::u8 pin);

    void begin();
    void end();

    void beginTransaction(const LpspiSettings &settings);
    void endTransaction();

    /// Blocking single-byte exchange. Upstream SPI.h:1246-1257.
    fl::u8 transfer(fl::u8 data) {
        mPort->TDR = data;
        while (true) {
            const fl::u32 fifo = (mPort->FSR >> 16) & 0x1F;
            if (fifo > 0) {
                return static_cast<fl::u8>(mPort->RDR);
            }
        }
    }

    /// Byte substituted for the transmit stream when `transfer()` is handed a
    /// null tx buffer. Mirrors upstream `SPIClass::setTransferWriteFill`.
    void setTransferWriteFill(fl::u8 fill) { mTransferWriteFill = fill; }

    /// Blocking full-duplex block exchange, a plain loop over the single-byte
    /// path above. `tx == nullptr` clocks out the fill byte set by
    /// `setTransferWriteFill()`; `rx == nullptr` discards what comes back.
    /// Deliberately no DMA and no async completion -- SdFat's driver is
    /// synchronous, and the FIFO-batched form can be added later if profiling
    /// asks for it.
    void transfer(const void *tx, void *rx, fl::size count) {
        const fl::u8 *src = static_cast<const fl::u8 *>(tx);
        fl::u8 *dst = static_cast<fl::u8 *>(rx);
        for (fl::size i = 0; i < count; ++i) {
            const fl::u8 received = transfer(src ? src[i] : mTransferWriteFill);
            if (dst) {
                dst[i] = received;
            }
        }
    }

    /// The peripheral register block, for drivers that drive data directly.
    IMXRT_LPSPI_t &port() { return *mPort; }

  private:
    LpspiBus() = default;

    void configure(IMXRT_LPSPI_t *port, const LpspiHardware *hw) {
        mPort = port;
        mHw = hw;
    }

    const LpspiHardware &hardware() const { return *mHw; }

    IMXRT_LPSPI_t *mPort = nullptr;
    const LpspiHardware *mHw = nullptr;

    fl::u8 mMisoPinIndex = 0;
    fl::u8 mMosiPinIndex = 0;
    fl::u8 mSckPinIndex = 0;

    fl::u32 mClock = 0;
    fl::u32 mCcr = 0;

    fl::u8 mTransferWriteFill = 0xFF;

    friend struct LpspiBusStorage;
};



/// Storage for the three buses.
///
/// Held in a `fl::Singleton` rather than at namespace scope so an unused SPI
/// backend is fully strippable -- the whole point of not linking the
/// framework library, whose three global `SPIClass` objects the linker can
/// never drop once anything names them (FastLED#3777, and the
/// SingletonElisionChecker rule generally).
struct LpspiBusStorage {
    LpspiBus buses[3];
    bool initialized = false;

    void ensure() {
        if (initialized) {
            return;
        }
        initialized = true;
        buses[0].configure(&IMXRT_LPSPI4_S, &lpspi_hardware<4>());
        buses[1].configure(&IMXRT_LPSPI3_S, &lpspi_hardware<3>());
        buses[2].configure(&IMXRT_LPSPI1_S, &lpspi_hardware<1>());
    }
};

inline LpspiBus &LpspiBus::get(fl::u8 index) {
    LpspiBusStorage &storage = fl::Singleton<LpspiBusStorage>::instance();
    storage.ensure();
    if (index > 2) {
        index = 0;
    }
    return storage.buses[index];
}

// Upstream SPI.cpp:1450-1467.
inline void LpspiBus::setSCK(fl::u8 pin) {
    if (pin == hardware().sck_pin[mSckPinIndex]) {
        return;
    }
    for (fl::u8 i = 0; i < CNT_SCK_PINS; i++) {
        if (pin != hardware().sck_pin[i]) {
            continue;
        }
        if (hardware().clock_gate_register & hardware().clock_gate_mask) {
            // Upstream note: unlike t3.x there is no "unused" mux setting, so
            // the previous pin is left as-is.
            const fl::u32 fastio = IOMUXC_PAD_DSE(7) | IOMUXC_PAD_SPEED(2);
            *(portControlRegister(hardware().sck_pin[i])) = fastio;
            *(portConfigRegister(hardware().sck_pin[i])) = hardware().sck_mux[i];
            hardware().sck_select_input_register = hardware().sck_select_val[i];
        }
        mSckPinIndex = i;
        return;
    }
}

// Upstream SPI.cpp:1412-1429.
inline void LpspiBus::setMOSI(fl::u8 pin) {
    if (pin == hardware().mosi_pin[mMosiPinIndex]) {
        return;
    }
    for (fl::u8 i = 0; i < CNT_MOSI_PINS; i++) {
        if (pin != hardware().mosi_pin[i]) {
            continue;
        }
        if (hardware().clock_gate_register & hardware().clock_gate_mask) {
            const fl::u32 fastio = IOMUXC_PAD_DSE(7) | IOMUXC_PAD_SPEED(2);
            *(portControlRegister(hardware().mosi_pin[i])) = fastio;
            *(portConfigRegister(hardware().mosi_pin[i])) =
                hardware().mosi_mux[i];
            hardware().mosi_select_input_register =
                hardware().mosi_select_val[i];
        }
        mMosiPinIndex = i;
        return;
    }
}

// Upstream SPI.cpp:1431-1448.
inline void LpspiBus::setMISO(fl::u8 pin) {
    if (pin == hardware().miso_pin[mMisoPinIndex]) {
        return;
    }
    for (fl::u8 i = 0; i < CNT_MISO_PINS; i++) {
        if (pin != hardware().miso_pin[i]) {
            continue;
        }
        if (hardware().clock_gate_register & hardware().clock_gate_mask) {
            const fl::u32 fastio = IOMUXC_PAD_DSE(7) | IOMUXC_PAD_SPEED(2);
            *(portControlRegister(hardware().miso_pin[i])) = fastio;
            *(portConfigRegister(hardware().miso_pin[i])) =
                hardware().miso_mux[i];
            hardware().miso_select_input_register =
                hardware().miso_select_val[i];
        }
        mMisoPinIndex = i;
        return;
    }
}

// Upstream SPI.cpp:1271-1316.
inline void LpspiBus::begin() {
    // CBCMR[LPSPI_CLK_SEL] -> PLL2 = 528 MHz, CBCMR[LPSPI_PODF] -> div4.
    hardware().clock_gate_register &= ~hardware().clock_gate_mask;

    CCM_CBCMR = (CCM_CBCMR & ~(CCM_CBCMR_LPSPI_PODF_MASK |
                               CCM_CBCMR_LPSPI_CLK_SEL_MASK)) |
                CCM_CBCMR_LPSPI_PODF(2) | CCM_CBCMR_LPSPI_CLK_SEL(1);  // pg 714

    const fl::u32 fastio = IOMUXC_PAD_DSE(7) | IOMUXC_PAD_SPEED(2);
    *(portControlRegister(hardware().miso_pin[mMisoPinIndex])) = fastio;
    *(portControlRegister(hardware().mosi_pin[mMosiPinIndex])) = fastio;
    *(portControlRegister(hardware().sck_pin[mSckPinIndex])) = fastio;

    hardware().clock_gate_register |= hardware().clock_gate_mask;
    *(portConfigRegister(hardware().miso_pin[mMisoPinIndex])) =
        hardware().miso_mux[mMisoPinIndex];
    *(portConfigRegister(hardware().mosi_pin[mMosiPinIndex])) =
        hardware().mosi_mux[mMosiPinIndex];
    *(portConfigRegister(hardware().sck_pin[mSckPinIndex])) =
        hardware().sck_mux[mSckPinIndex];

    hardware().sck_select_input_register = hardware().sck_select_val[mSckPinIndex];
    hardware().miso_select_input_register =
        hardware().miso_select_val[mMisoPinIndex];
    hardware().mosi_select_input_register =
        hardware().mosi_select_val[mMosiPinIndex];

    port().CR = LPSPI_CR_RST;

    // Transmit FIFO watermark = FIFO size - 1 (upstream assumes 16).
    port().FCR = LPSPI_FCR_TXWATER(15);

    // Leave the peripheral in a known state.
    beginTransaction(LpspiSettings());
    endTransaction();
}

// Upstream SPI.h:1176-1243, minus the NVIC mask save/restore (see header).
inline void LpspiBus::beginTransaction(const LpspiSettings &settings) {
    if (settings.clock() != mClock) {
        static const fl::u32 clk_sel[4] = {664615384,   // PLL3 PFD1
                                           720000000,   // PLL3 PFD0
                                           528000000,   // PLL2
                                           396000000};  // PLL2 PFD2

        mClock = settings.clock();

        const fl::u32 cbcmr = CCM_CBCMR;
        // LPSPI peripheral clock
        const fl::u32 clkhz =
            clk_sel[(cbcmr >> 4) & 0x03] / (((cbcmr >> 26) & 0x07) + 1);

        fl::u32 d = mClock ? clkhz / mClock : clkhz;
        if (d && clkhz / d > mClock) {
            d++;
        }
        if (d > 257) {
            d = 257;  // max div
        }
        const fl::u32 div = (d > 2) ? (d - 2) : 0;

        mCcr = LPSPI_CCR_SCKDIV(div) | LPSPI_CCR_DBT(div / 2) |
               LPSPI_CCR_PCSSCK(div / 2);
    }

    port().CR = 0;
    port().CFGR1 = LPSPI_CFGR1_MASTER | LPSPI_CFGR1_SAMPLE;
    port().CCR = mCcr;
    port().TCR = settings.tcr();
    port().CR = LPSPI_CR_MEN;
}

// Upstream SPI.h:1308-1324. The body there only unwinds the NVIC masks that
// `usingInterrupt()` would have set; FastLED never calls it, so this is a
// no-op kept for call-site symmetry.
inline void LpspiBus::endTransaction() {}

// Upstream SPI.cpp:1805-1813.
inline void LpspiBus::end() {
    // Disable the peripheral and return the pads to a safe state.
    port().CR = 0;
    // `::pinMode` explicitly: we are inside namespace fl, where an unqualified
    // call would bind FastLED's own typed fl::pinMode(PinMode) rather than the
    // Teensy core's, and INPUT_DISABLE is a core constant.
    ::pinMode(hardware().miso_pin[mMisoPinIndex], INPUT_DISABLE);
    ::pinMode(hardware().mosi_pin[mMosiPinIndex], INPUT_DISABLE);
    ::pinMode(hardware().sck_pin[mSckPinIndex], INPUT_DISABLE);
}

}  // namespace teensy
}  // namespace platforms
}  // namespace fl
