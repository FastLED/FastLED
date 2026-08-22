#pragma once

// IWYU pragma: private

/// @file platforms/arm/teensy/kinetis_spi/dspi_bus.h
/// Minimal full-duplex SPI bus control for the Kinetis Teensys (3.x and LC),
/// so FastLED's SdFat transport does not depend on the Arduino `SPI` library.
///
/// Why this exists: the Teensyduino `SPI` library is only *compiled* when the
/// PlatformIO library finder selects it from an unconditional sketch-level
/// `#include <SPI.h>`. FastLED cannot add that include (it breaks the host
/// example build, which has no `SPI.h`), so `SPIClass::begin`,
/// `SPIClass::transfer`, the global `SPI` object and
/// `SPISettings::ctar_clock_table` / `ctar_div_table` all resolve as headers
/// and then come up undefined at link time (FastLED#3970/#3972). The library
/// also cannot be vendored: it is GPLv2/LGPLv2.1 and FastLED is MIT.
///
/// This is the Teensy 3.x/LC counterpart of
/// `teensy4_common/lpspi/lpspi_bus.h`, and exposes the identical surface:
///
///     begin / end                        clock gate + pad mux
///     beginTransaction / endTransaction  baud-rate programming
///     transfer(u8)                       blocking full-duplex byte exchange
///     transfer(tx, rx, count)            blocking block exchange
///     setTransferWriteFill               fill byte for a null tx buffer
///
/// NOTHING here is derived from Arduino's `SPI.h`/`SPI.cpp`. In particular the
/// frequency-to-CTAR mapping is *computed* from the K20/K64/K66 reference
/// manual baud-rate equation rather than transcribed from upstream's
/// `ctar_clock_table` / `ctar_div_table`, which are GPL/LGPL. See
/// `dspiClockBits()` for the derivation.
///
/// Two silicon families share this class because they share exactly one call
/// site (the vendored SdFat driver):
///   * Teensy 3.0-3.6 (MK20/MK64/MK66) have the DSPI module -- MCR/CTARn/
///     PUSHR/POPR with a TX+RX FIFO. This is the interesting path and the one
///     the SD card actually runs on (3.5/3.6 are the only Kinetis Teensys with
///     enough RAM for `Fx/FxSdCard`).
///   * Teensy LC (MKL26) has the far simpler non-DSPI SPI module -- C1/C2/BR/
///     S/DL. It is carried here so the SdFat tree, which compiles for every
///     Teensy, has one uniform port type and no `SPIClass` reference is left
///     behind. Nothing links it in practice: the SD example is filtered off
///     the LC for memory, and `fl.system.sd+.cpp.o` is dropped unless a sketch
///     calls `beginSd()`.
///
/// Like `LpspiBus`, `beginTransaction`/`endTransaction` do NOT save and
/// restore NVIC masks -- that path only matters for `usingInterrupt()`, which
/// FastLED never calls.

#include "fl/stl/int.h"
#include "fl/stl/singleton.h"
#include "platforms/arm/teensy/is_teensy.h"  // ok platform headers

// SPI mode constants, normally supplied by the Arduino <SPI.h> we no longer
// include. Guarded because a sketch may still pull the framework header in.
#ifndef SPI_MODE0
#define SPI_MODE0 0x00
#define SPI_MODE1 0x04
#define SPI_MODE2 0x08
#define SPI_MODE3 0x0C
#endif

#if defined(FL_IS_TEENSY_3X) || defined(FL_IS_TEENSY_LC)

namespace fl {
namespace platforms {
namespace teensy {

#if defined(FL_IS_TEENSY_LC)
/// MKL26 lightweight SPI register block.
typedef KINETISL_SPI_t DspiRegs_t;
#else
/// MK20/MK64/MK66 DSPI register block.
typedef KINETISK_SPI_t DspiRegs_t;
#endif

/// Transaction parameters. Fills the role Arduino's `SPISettings` plays for
/// `SPIClass`, but stores only what this bus programs: the requested SCK
/// frequency and the polarity/phase/bit-order bits, already positioned for the
/// target register (CTAR0 on DSPI, C1 on the LC).
class DspiSettings {
  public:
    DspiSettings() : mClock(4000000) { init(MSBFIRST, SPI_MODE0); }

    DspiSettings(fl::u32 clock, fl::u8 bit_order, fl::u8 data_mode)
        : mClock(clock) {
        init(bit_order, data_mode);
    }

    fl::u32 clock() const { return mClock; }

    /// CPOL / CPHA / LSB-first bits for the mode register.
    fl::u32 modeBits() const { return mModeBits; }

  private:
    void init(fl::u8 bit_order, fl::u8 data_mode) {
        // Arduino's SPI_MODEn encoding: bit 3 is CPOL, bit 2 is CPHA.
        mModeBits = 0;
#if defined(FL_IS_TEENSY_LC)
        if (bit_order == LSBFIRST) {
            mModeBits |= SPI_C1_LSBFE;
        }
        if (data_mode & 0x08) {
            mModeBits |= SPI_C1_CPOL;
        }
        if (data_mode & 0x04) {
            mModeBits |= SPI_C1_CPHA;
        }
#else
        if (bit_order == LSBFIRST) {
            mModeBits |= SPI_CTAR_LSBFE;
        }
        if (data_mode & 0x08) {
            mModeBits |= SPI_CTAR_CPOL;
        }
        if (data_mode & 0x04) {
            mModeBits |= SPI_CTAR_CPHA;
        }
#endif
    }

    fl::u32 mClock;
    fl::u32 mModeBits = 0;
};

#if defined(FL_IS_TEENSY_LC)

/// Baud-rate bits for the MKL26 SPI module.
///
/// MKL26 reference manual, "SPI baud rate generation":
///
///     SCK = f_BUS / ((SPPR + 1) * 2^(SPR + 1))
///
/// with SPPR in 0..7 and SPR in 0..8. Pick the fastest setting that does not
/// exceed `hz`; if even the slowest divider is too fast, use the slowest.
inline fl::u32 dspiClockBits(fl::u32 hz) {
    fl::u8 best_sppr = 7;
    fl::u8 best_spr = 8;
    fl::u32 best_f = 0;
    for (fl::u8 sppr = 0; sppr < 8; ++sppr) {
        for (fl::u8 spr = 0; spr <= 8; ++spr) {
            const fl::u32 divisor = static_cast<fl::u32>(sppr + 1)
                                    << (spr + 1);
            const fl::u32 f = F_BUS / divisor;
            if (f <= hz && f > best_f) {
                best_f = f;
                best_sppr = sppr;
                best_spr = spr;
            }
        }
    }
    return SPI_BR_SPPR(best_sppr) | SPI_BR_SPR(best_spr);
}

#else  // DSPI (Teensy 3.x)

/// Baud-rate portion of CTAR0 for the DSPI module.
///
/// K20/K64/K66 reference manual, "Baud rate generator" and the CTAR
/// DBR/PBR/BR field descriptions:
///
///     SCK = (f_BUS / PBR_value) * ((1 + DBR) / BR_value)
///
/// with
///     PBR field 0..3  -> prescaler 2, 3, 5, 7
///     BR  field 0..15 -> scaler    2, 4, 6, 8, 16, 32, ... 32768
///     DBR             -> doubles the rate
///
/// The manual notes the 50/50 duty cycle is only preserved for DBR = 1 when
/// the BR scaler is 2, so DBR is only considered there -- an SD card tolerates
/// an asymmetric clock but there is no reason to hand it one.
///
/// The search picks the fastest combination at or below `hz`, falling back to
/// the slowest available divider when `hz` is below even that. This replaces
/// Arduino's `SPISettings::ctar_clock_table` / `ctar_div_table` lookup, which
/// is GPL/LGPL and must not be copied.
inline fl::u32 dspiClockBits(fl::u32 hz) {
    static const fl::u32 kPbrValue[4] = {2, 3, 5, 7};
    static const fl::u32 kBrValue[16] = {2,    4,    6,     8,     16,   32,
                                         64,   128,  256,   512,   1024, 2048,
                                         4096, 8192, 16384, 32768};

    fl::u8 best_pbr = 3;  // prescaler 7
    fl::u8 best_br = 15;  // scaler 32768
    fl::u8 best_dbr = 0;
    fl::u32 best_f = 0;

    for (fl::u8 dbr = 0; dbr < 2; ++dbr) {
        for (fl::u8 pbr = 0; pbr < 4; ++pbr) {
            for (fl::u8 br = 0; br < 16; ++br) {
                if (dbr && kBrValue[br] != 2) {
                    continue;  // duty cycle only guaranteed at BR scaler 2
                }
                const fl::u32 f =
                    (F_BUS * (1u + dbr)) / (kPbrValue[pbr] * kBrValue[br]);
                if (f <= hz && f > best_f) {
                    best_f = f;
                    best_pbr = pbr;
                    best_br = br;
                    best_dbr = dbr;
                }
            }
        }
    }

    // CSSCK reuses the BR field value, matching the existing FastLED K20/K66
    // LED path.
    //
    // Note the two fields do NOT share an encoding, so this is not "half a
    // clock": BR field n selects scaler {2,4,6,8,16,32,...} while CSSCK field
    // n selects 2^(n+1) uniformly. At the 3.75 MHz run setting (BR field 3)
    // that inserts 16 bus cycles of tCSC, roughly 0.27 us against a 2.13 us
    // byte. It is harmless -- SdFat drives chip select as a plain GPIO, so the
    // PCS-to-SCK delay is never actually needed -- but it is not free either.
    // Left as-is rather than switched to CSSCK(0) because the throughput cost
    // is small and no Teensy 3.x hardware was available to re-validate the
    // timing change; see FastLED#3972 follow-ups.
    fl::u32 ctar = SPI_CTAR_PBR(best_pbr) | SPI_CTAR_BR(best_br) |
                   SPI_CTAR_CSSCK(best_br);
    if (best_dbr) {
        ctar |= SPI_CTAR_DBR;
    }
    return ctar;
}

#endif  // FL_IS_TEENSY_LC

/// One SPI peripheral. Obtain via `DspiBus::get(index)`.
///
/// Only bus 0 -- SPI0 on pins 11 (MOSI) / 12 (MISO) / 13 (SCK), the peripheral
/// the Arduino global `SPI` object wraps on every Kinetis Teensy -- is wired
/// up, because that is the only one the SdFat transport asks for. `get()`
/// keeps the index parameter so the call sites match `LpspiBus::get()`.
class DspiBus {
  public:
    /// The default bus. Any index maps to bus 0 (SPI0).
    static DspiBus &get(fl::u8 index);

    void begin();
    void end();

    void beginTransaction(const DspiSettings &settings);
    void endTransaction();

    /// Blocking full-duplex byte exchange.
    fl::u8 transfer(fl::u8 data);

    /// Byte substituted for the transmit stream when `transfer()` is handed a
    /// null tx buffer.
    void setTransferWriteFill(fl::u8 fill) { mTransferWriteFill = fill; }

    /// Blocking full-duplex block exchange, a plain loop over the single-byte
    /// path. `tx == nullptr` clocks out the fill byte; `rx == nullptr`
    /// discards what comes back. No DMA and no async completion -- SdFat's
    /// driver is synchronous.
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
    DspiRegs_t &port() { return *mPort; }

  private:
    DspiBus() = default;

    void configure(DspiRegs_t *port) { mPort = port; }

    DspiRegs_t *mPort = nullptr;

    fl::u32 mClock = 0;
    fl::u32 mClockBits = 0;

    fl::u8 mTransferWriteFill = 0xFF;

    friend struct DspiBusStorage;
};

/// Storage for the bus.
///
/// Held in a `fl::Singleton` rather than at namespace scope so an unused SPI
/// backend is fully strippable -- the whole point of not linking the framework
/// library, whose global `SPIClass` objects the linker can never drop once
/// anything names them (FastLED#3777).
struct DspiBusStorage {
    DspiBus bus;
    bool initialized = false;

    void ensure() {
        if (initialized) {
            return;
        }
        initialized = true;
#if defined(FL_IS_TEENSY_LC)
        bus.configure(&KINETISL_SPI0);
#else
        bus.configure(&KINETISK_SPI0);
#endif
    }
};

inline DspiBus &DspiBus::get(fl::u8 index) {
    (void)index;
    DspiBusStorage &storage = fl::Singleton<DspiBusStorage>::instance();
    storage.ensure();
    return storage.bus;
}

#if defined(FL_IS_TEENSY_LC)

inline void DspiBus::begin() {
    SIM_SCGC4 |= SIM_SCGC4_SPI0;

    // MOSI = 11, MISO = 12, SCK = 13, all ALT2 -- the same defaults the LC
    // LED SPI path uses (teensy_lc/fastspi_arm_kl26.h).
    CORE_PIN11_CONFIG = PORT_PCR_MUX(2);
    CORE_PIN12_CONFIG = PORT_PCR_MUX(2);
    CORE_PIN13_CONFIG = PORT_PCR_MUX(2);

    port().C1 = SPI_C1_MSTR | SPI_C1_SPE;
    port().C2 = 0;  // 8-bit frames

    beginTransaction(DspiSettings());
    endTransaction();
}

inline void DspiBus::beginTransaction(const DspiSettings &settings) {
    if (settings.clock() != mClock) {
        mClock = settings.clock();
        mClockBits = dspiClockBits(mClock);
    }
    port().BR = static_cast<fl::u8>(mClockBits);
    port().C2 = 0;
    port().C1 =
        static_cast<fl::u8>(SPI_C1_MSTR | SPI_C1_SPE | settings.modeBits());
    // Drain any byte a previous user left sitting in the read buffer: reading
    // S then DL is what clears SPRF on this module.
    if (port().S & SPI_S_SPRF) {
        (void)port().DL;
    }
}

inline fl::u8 DspiBus::transfer(fl::u8 data) {
    while (!(port().S & SPI_S_SPTEF)) {
    }
    port().DL = data;
    while (!(port().S & SPI_S_SPRF)) {
    }
    return port().DL;
}

inline void DspiBus::end() {
    port().C1 = 0;
    CORE_PIN11_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1);
    CORE_PIN12_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1);
    CORE_PIN13_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1);
}

#else  // DSPI (Teensy 3.x)

/// MCR value used whenever the module is running: master mode, all PCS lines
/// idle high (SdFat owns chip select as a GPIO), FIFOs enabled, not halted.
#define FL_DSPI_MCR_RUN (SPI_MCR_MSTR | SPI_MCR_PCSIS(0x1F))

inline void DspiBus::begin() {
    SIM_SCGC6 |= SIM_SCGC6_SPI0;

    beginTransaction(DspiSettings());
    endTransaction();
}

inline void DspiBus::beginTransaction(const DspiSettings &settings) {
    if (settings.clock() != mClock) {
        mClock = settings.clock();
        mClockBits = dspiClockBits(mClock);
    }

    // Re-mux the pads on every transaction, not once in begin().
    //
    // FastLED's own LED driver calls `disable_pins()` from `release()` at the
    // end of every `show()` (fastspi_arm_k66.h:147,385 and the K20 twin),
    // which hard-writes pins 11 and 13 back to `PORT_PCR_MUX(1)` -- GPIO. A
    // one-time mux in begin() therefore survives only until the first
    // `FastLED.show()` on a shared SPI0. After that the DSPI still completes
    // frames internally, so `transfer()` returns on schedule and nothing
    // hangs, but no SCK or MOSI reaches the card and MISO floats: 0xFF or
    // garbage, surfacing as CMD0/ACMD41 timeouts and silently corrupt reads.
    // These are idempotent register stores, so paying them per transaction is
    // cheap next to a 512-byte block.
    //
    // MOSI = 11, MISO = 12, SCK = 13, all ALT2. These are SPI0's default pads
    // on every Teensy 3.x; the alternates (7/8/14) are not exposed here
    // because SdFat's pin-override branch is compiled out.
    CORE_PIN11_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);
    CORE_PIN12_CONFIG = PORT_PCR_MUX(2);
    CORE_PIN13_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);

    // Halt, then flush -- in two writes, and without MDIS.
    //
    // MDIS gates the clock to the DSPI's non-memory-mapped logic, which is
    // where the FIFO counters live, so `CLR_TXF`/`CLR_RXF` raised in the same
    // write as `MDIS` are not guaranteed to take effect. NXP's KSDK
    // `DSPI_FlushFifo` and Teensyduino both flush under `HALT` alone. CTAR0 is
    // memory-mapped and `HALT` alone satisfies the "module stopped"
    // requirement for writing it.
    port().MCR = FL_DSPI_MCR_RUN | SPI_MCR_HALT;
    // `HALT` stops the module at the next frame boundary, not instantly. The
    // LED path has `writeByteNoWait` variants that return with a frame still
    // in flight, so wait for the Stopped state before rewriting CTAR0 --
    // otherwise that frame finishes under a half-changed clock config.
    while (port().SR & SPI_SR_TXRXS) {
    }
    port().MCR = FL_DSPI_MCR_RUN | SPI_MCR_HALT | SPI_MCR_CLR_TXF |
                 SPI_MCR_CLR_RXF;

    port().CTAR0 = SPI_CTAR_FMSZ(7) | mClockBits | settings.modeBits();

    // Clear the sticky status flags. Flushing the RX FIFO above matters for
    // the same reason it does on Teensy 4: FastLED's LED driver pushes frames
    // without ever popping POPR, so a shared SPI0 would otherwise hand the
    // first `transfer()` a stale byte instead of the card's reply.
    port().SR = SPI_SR_TCF | SPI_SR_EOQF | SPI_SR_TFUF | SPI_SR_RFOF |
                SPI_SR_RFDF;

    port().MCR = FL_DSPI_MCR_RUN;
}

inline fl::u8 DspiBus::transfer(fl::u8 data) {
    // CTAS = 0 selects CTAR0; no PCS bits, so chip select stays under GPIO
    // control. The frame lands in the RX FIFO as it is shifted out, so the
    // reply is ready once RFDF (receive FIFO not empty) asserts.
    port().PUSHR = SPI_PUSHR_CTAS(0) | static_cast<fl::u32>(data);
    while (!(port().SR & SPI_SR_RFDF)) {
    }
    const fl::u32 received = port().POPR;
    port().SR = SPI_SR_RFDF;  // write-1-to-clear; re-asserts if more queued
    return static_cast<fl::u8>(received);
}

inline void DspiBus::end() {
    port().MCR = FL_DSPI_MCR_RUN | SPI_MCR_MDIS | SPI_MCR_HALT;
    CORE_PIN11_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
    CORE_PIN12_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
    CORE_PIN13_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
}

#endif  // FL_IS_TEENSY_LC

// Upstream `SPIClass::endTransaction` only unwinds the NVIC masks that
// `usingInterrupt()` would have set; FastLED never calls it, so this is a
// no-op kept for call-site symmetry.
inline void DspiBus::endTransaction() {}

}  // namespace teensy
}  // namespace platforms
}  // namespace fl

#endif  // FL_IS_TEENSY_3X || FL_IS_TEENSY_LC
