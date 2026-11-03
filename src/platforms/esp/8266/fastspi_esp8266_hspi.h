#pragma once

// IWYU pragma: private

/// @file platforms/esp/8266/fastspi_esp8266_hspi.h
/// @brief Native register-level ESP8266 HSPI (SPI1) output for clocked chipsets
///
/// Drives the hardware HSPI peripheral directly: GPIO13 (MOSI) and GPIO14
/// (SCK) are switched to their HSPI alternate function, bytes are staged in
/// the SPI1W0..15 FIFO and one SPI1CMD kick shifts up to 64 bytes at once.
/// No Arduino <SPI.h> is involved, and the clock divider comes from the
/// chipset's requested SPI_SPEED rather than a hard-coded value.
///
/// This is the default path for the fixed HSPI pin pair (13/14); other pin
/// pairs still use the generic software SPI bit-bang. The dispatch layer
/// resolves the active pin-order mode to these real GPIO numbers, so D1 and
/// NodeMCU numbering (7/5) selects the same pads.
///
/// The earlier opt-in `ESP8266SPIOutput` in fastspi_esp8266.h wrapped the
/// Arduino SPI library, which pinned the bus to 3.2 MHz. See issue #4925.

#include "eorder.h"
#include "fastspi_types.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "fl/system/fastpin_base.h"  // Selectable
#include "pixel_controller.h"
#include "platforms/cpu_frequency.h"
#include "platforms/esp/8266/hspi_regs.h"
#include "platforms/esp/8266/spi_hspi_clock.h"
#include "platforms/shared/spi_pixel_writer.h"

namespace fl {

/// Hardware HSPI output for the ESP8266's fixed SPI pin pair.
///
/// @tparam DATA_PIN MOSI pin; must be GPIO13 (the only HSPI MOSI pad)
/// @tparam CLOCK_PIN SCK pin; must be GPIO14 (the only HSPI SCK pad)
/// @tparam SPI_SPEED the chipset's FL_DATA_RATE_MHZ value: a frequency in Hz
///         with FASTLED_ALL_PINS_HARDWARE_SPI, CPU cycles per bit otherwise
template <u8 DATA_PIN, u8 CLOCK_PIN, u32 SPI_SPEED>
class ESP8266HSPIOutput {
    FL_STATIC_ASSERT(DATA_PIN == 13 && CLOCK_PIN == 14,
                     "ESP8266 HSPI is only available on GPIO13 (MOSI) / GPIO14 (SCK)");

    /// SPI1W0..15 = 16 words = 512 bits = 64 bytes per transaction.
    static constexpr u16 kMaxBatchBytes = esp8266::kHspiMaxBatchBytes;

    /// The SPI1CLK value for this chipset's rate.
    ///
    /// FL_DATA_RATE_MHZ() is a frequency in Hz on ESP8266 only when the user
    /// opted into hardware SPI; otherwise it is the software path's
    /// CPU-cycles-per-bit value (see fastspi.h). Convert that into the HSPI
    /// clock domain, which is fixed at 80 MHz regardless of whether the CPU
    /// runs at 80 or 160 MHz, so the bus stays at or below the request.
    static constexpr esp8266::HspiClock clock() FL_NO_EXCEPT {
#if defined(FASTLED_ALL_PINS_HARDWARE_SPI)
        return esp8266::hspiClockFor(SPI_SPEED);
#else
        return esp8266::hspiClockForMinDivider(
            esp8266::hspiDividerForCpuCycles(SPI_SPEED + 1, FL_CPU_FREQUENCY()));
#endif
    }

    Selectable* mPSelect;
    u8 mBuf[kMaxBatchBytes];
    u16 mFill;

public:
    ESP8266HSPIOutput() FL_NO_EXCEPT : mPSelect(nullptr), mBuf(), mFill(0) {}
    ESP8266HSPIOutput(Selectable* pSelect) FL_NO_EXCEPT : mPSelect(pSelect), mBuf(), mFill(0) {}
    void setSelect(Selectable* pSelect) FL_NO_EXCEPT { mPSelect = pSelect; }

    void init() FL_NO_EXCEPT {
        mFill = 0;
        esp8266::hspiConfigure(clock());
        release();
    }

    /// Return GPIO13/14 to the plain GPIO function so the pads are usable again
    /// (mirrors the core's end(): both pins end up as inputs).
    static void stop() FL_NO_EXCEPT { esp8266::hspiRelease(); }

    /// No-op: there is no asynchronous write queue to drain.
    static void wait() FL_NO_EXCEPT {}

    /// Shift out any staged bytes and block until the bus is idle.
    void waitFully() FL_NO_EXCEPT {
        flush();
        esp8266::hspiWaitIdle();
    }

    void writeByteNoWait(u8 b) FL_NO_EXCEPT { writeByte(b); }
    void writeBytePostWait(u8 b) FL_NO_EXCEPT { writeByte(b); wait(); }
    void writeWord(u16 w) FL_NO_EXCEPT { writeByte(static_cast<u8>(w >> 8)); writeByte(static_cast<u8>(w & 0xFF)); }

    /// Stage one byte into the SPI FIFO batch.
    void writeByte(u8 b) FL_NO_EXCEPT {
        if (mFill >= kMaxBatchBytes) {
            flush();
        }
        mBuf[mFill++] = b;
    }

    /// Write a single bit out, MSB-first (used for chipsets with a start bit).
    template <u8 BIT> void writeBit(u8 b) FL_NO_EXCEPT {
        flush();
        esp8266::hspiWriteBit(((b >> BIT) & 1) != 0);
    }

    void select() FL_NO_EXCEPT {
        if (mPSelect != nullptr) {
            mPSelect->select();
        }
    }

    void release() FL_NO_EXCEPT {
        flush();
        if (mPSelect != nullptr) {
            mPSelect->release();
        }
    }

    void endTransaction() FL_NO_EXCEPT {
        waitFully();
        release();
    }

    void writeBytesValue(u8 value, int len) FL_NO_EXCEPT {
        select();
        writeBytesValueRaw(value, len);
        release();
    }

    void writeBytesValueRaw(u8 value, int len) FL_NO_EXCEPT {
        while (len-- > 0) {
            writeByte(value);
        }
    }

    template <class D> void writeBytes(FASTLED_REGISTER u8* data, int len) FL_NO_EXCEPT {
        select();
        u8* end = data + len;
        while (data != end) {
            writeByte(D::adjust(*data++));
        }
        D::postBlock(len, this);
        release();
    }

    void writeBytes(FASTLED_REGISTER u8* data, int len) FL_NO_EXCEPT { writeBytes<DATA_NOP>(data, len); }

    /// Compatibility shim: buffers are flushed by release()/waitFully().
    static void finalizeTransmission() FL_NO_EXCEPT {}

    template <u8 FLAGS, class D, EOrder RGB_ORDER>
    FL_NO_INLINE void writePixels(PixelController<RGB_ORDER> pixels, void* context = nullptr) FL_NO_EXCEPT {
        fl::writePixelsToSPI<FLAGS, D, RGB_ORDER>(pixels, *this, context);
    }

private:
    /// Write the staged bytes into the FIFO and shift them out.
    void flush() FL_NO_EXCEPT {
        if (mFill == 0) {
            return;
        }
        const u16 nbytes = mFill;
        mFill = 0;
        esp8266::hspiWriteChunk(mBuf, nbytes);
    }
};

}  // namespace fl
