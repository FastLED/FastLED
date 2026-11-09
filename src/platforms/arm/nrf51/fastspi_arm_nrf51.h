// IWYU pragma: private

#ifndef __INC_FASTSPI_NRF_H
#define __INC_FASTSPI_NRF_H

#include "platforms/arm/nrf51/is_nrf51.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "platforms/arm/nrf51/fastpin_arm_nrf51.h"  // Nrf51HwPin, fastpin_base.h
#include "fastspi_types.h"
#include "fl/gfx/eorder.h"

#ifdef FL_IS_NRF51

#ifndef FASTLED_FORCE_SOFTWARE_SPI
#define FASTLED_ALL_PINS_HARDWARE_SPI

// IWYU pragma: begin_keep
#include <nrf51.h>            // vendor CMSIS: NRF_SPI0 / NRF_SPI_Type
#include <nrf51_bitfields.h>  // SPI_ENABLE_* / SPI_FREQUENCY_*
// IWYU pragma: end_keep

namespace fl {

// A nop/stub class, mostly to show the SPI methods that are needed/used by the various SPI chipset implementations.  Should
// be used as a definition for the set of methods that the spi implementation classes should use (since C++ doesn't support the
// idea of interfaces - it's possible this could be done with virtual classes, need to decide if i want that overhead)
template <fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class NRF51SPIOutput {

    struct saveData {
        fl::u32 sck;
        fl::u32 mosi;
        fl::u32 miso;
        fl::u32 freq;
        fl::u32 config;
        fl::u32 enable;
    } mSavedData;

    void saveSPIData() FL_NO_EXCEPT {
        mSavedData.sck = NRF_SPI0->PSELSCK;
        mSavedData.mosi = NRF_SPI0->PSELMOSI;
        mSavedData.miso = NRF_SPI0->PSELMISO;
        mSavedData.freq = NRF_SPI0->FREQUENCY;
        mSavedData.config = NRF_SPI0->CONFIG;
        mSavedData.enable = NRF_SPI0->ENABLE;
    }

    void restoreSPIData() FL_NO_EXCEPT {
        // PSEL*/FREQUENCY may only be changed while the SPI is disabled.
        NRF_SPI0->ENABLE = 0;
        NRF_SPI0->PSELSCK = mSavedData.sck;
        NRF_SPI0->PSELMOSI = mSavedData.mosi;
        NRF_SPI0->PSELMISO = mSavedData.miso;
        NRF_SPI0->FREQUENCY = mSavedData.freq;
        NRF_SPI0->CONFIG = mSavedData.config;
        NRF_SPI0->ENABLE = mSavedData.enable;
    }

public:
    NRF51SPIOutput() { Nrf51HwPin<_DATA_PIN>::setOutput(); Nrf51HwPin<_CLOCK_PIN>::setOutput(); }
    NRF51SPIOutput(Selectable *pSelect) {  Nrf51HwPin<_DATA_PIN>::setOutput(); Nrf51HwPin<_CLOCK_PIN>::setOutput();  }

    // set the object representing the selectable
    void setSelect(Selectable *pSelect) { /* TODO */ }

    FL_STATIC_ASSERT(_SPI_CLOCK_DIVIDER <= F_CPU / 125000UL, "nRF51 SPI0 cannot run below 125 kHz; use a faster DATA_RATE or the bit-bang SPI backend (#5188)");

    // nRF51 SPI FREQUENCY for F_CPU / _SPI_CLOCK_DIVIDER, rounded down to the
    // nearest supported rate (8 MHz max, 125 kHz min). Requests below 125 kHz
    // are rejected at compile time above (#5188).
    static constexpr fl::u32 spiFrequency() FL_NO_EXCEPT {
        return (_SPI_CLOCK_DIVIDER <= F_CPU / 8000000UL) ? SPI_FREQUENCY_FREQUENCY_M8
             : (_SPI_CLOCK_DIVIDER <= F_CPU / 4000000UL) ? SPI_FREQUENCY_FREQUENCY_M4
             : (_SPI_CLOCK_DIVIDER <= F_CPU / 2000000UL) ? SPI_FREQUENCY_FREQUENCY_M2
             : (_SPI_CLOCK_DIVIDER <= F_CPU / 1000000UL) ? SPI_FREQUENCY_FREQUENCY_M1
             : (_SPI_CLOCK_DIVIDER <= F_CPU / 500000UL) ? SPI_FREQUENCY_FREQUENCY_K500
             : (_SPI_CLOCK_DIVIDER <= F_CPU / 250000UL) ? SPI_FREQUENCY_FREQUENCY_K250
             : SPI_FREQUENCY_FREQUENCY_K125;
    }

    // Program SPI0 for this controller. Only called from select(), after the
    // previous SPI0 settings have been saved, so other SPI0 clients survive.
    void configure() FL_NO_EXCEPT {
        // PSEL*/FREQUENCY/CONFIG may only be changed while the SPI is disabled.
        NRF_SPI0->ENABLE = SPI_ENABLE_ENABLE_Disabled << SPI_ENABLE_ENABLE_Pos;
        NRF_SPI0->PSELSCK = _CLOCK_PIN;
        NRF_SPI0->PSELMOSI = _DATA_PIN;
        NRF_SPI0->PSELMISO = 0xFFFFFFFF;  // MISO not connected
        NRF_SPI0->FREQUENCY = spiFrequency();
        NRF_SPI0->CONFIG = 0;  // SPI mode 0, MSB first
        NRF_SPI0->EVENTS_READY = 0;
        NRF_SPI0->ENABLE = SPI_ENABLE_ENABLE_Enabled << SPI_ENABLE_ENABLE_Pos;
    }

    // initialize the SPI subssytem
    void init() FL_NO_EXCEPT {
        Nrf51HwPin<_DATA_PIN>::setOutput();
        Nrf51HwPin<_CLOCK_PIN>::setOutput();
    }

    // latch the CS select
    void select() { saveSPIData(); configure(); }

    // release the CS select
    void release() { restoreSPIData(); }

    void endTransaction() FL_NO_EXCEPT {
        waitFully();
        release();
    }

    // nRF51 legacy SPI master (Reference Manual, SPI chapter): TXD/RXD are
    // double-buffered and EVENTS_READY fires once per byte when its received
    // byte lands in RXD. RXD must be read for the next byte to complete, and
    // EVENTS_READY must be cleared by software. writeByte() therefore keeps
    // nothing in flight: it returns only after the byte has been clocked out,
    // so wait()/waitFully() have nothing left to wait for.
    static void waitFully() FL_NO_EXCEPT __attribute__((always_inline)) {}
    static void wait() FL_NO_EXCEPT __attribute__((always_inline)) {}

    // write a byte out via SPI and wait for it to be shifted out
    static void writeByte(fl::u8 b) FL_NO_EXCEPT __attribute__((always_inline)) {
        NRF_SPI0->TXD = b;
        while (NRF_SPI0->EVENTS_READY == 0) {
        }
        NRF_SPI0->EVENTS_READY = 0;
        (void)NRF_SPI0->RXD;  // pop RXD so the next transfer can complete
    }

    // write a word out via SPI (returns immediately on writing register)
    static void writeWord(fl::u16 w) __attribute__((always_inline)){ writeByte(w>>8); writeByte(w & 0xFF);  }

    // A raw set of writing byte values, assumes setup/init/waiting done elsewhere (static for use by adjustment classes)
    static void writeBytesValueRaw(fl::u8 value, int len) { while(len--) { writeByte(value);  } }

    // A full cycle of writing a value for len bytes, including select, release, and waiting
    void writeBytesValue(fl::u8 value, int len) FL_NO_EXCEPT {
        select();
        while(len--) {
            writeByte(value);
        }
        waitFully();
        release();
    }

    // A full cycle of writing a raw block of data out, including select, release, and waiting
    template<class D> void writeBytes(fl::u8 *data, int len) FL_NO_EXCEPT {
        fl::u8 *end = data + len;
        select();
        while(data != end) {
            writeByte(D::adjust(*data++));
        }
        D::postBlock(len);
        waitFully();
        release();
    }

    void writeBytes(fl::u8 *data, int len) FL_NO_EXCEPT {
        writeBytes<DATA_NOP>(data, len);
    }

    // write a single bit out, which bit from the passed in byte is determined by template parameter
    template <fl::u8 BIT> inline static void writeBit(fl::u8 b) FL_NO_EXCEPT {
        waitFully();
        NRF_SPI0->ENABLE = 0;
        if(b & 1<<BIT) {
            Nrf51HwPin<_DATA_PIN>::hi();
        } else {
            Nrf51HwPin<_DATA_PIN>::lo();
        }
        Nrf51HwPin<_CLOCK_PIN>::toggle();
        Nrf51HwPin<_CLOCK_PIN>::toggle();
        NRF_SPI0->ENABLE = 1;
    }

    template <fl::u8 FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels, void* context = nullptr) FL_NO_EXCEPT {
        select();
        int len = pixels.mLen;
        while(pixels.has(1)) {
            if(FLAGS & FLAG_START_BIT) {
                writeBit<0>(1);
            }
            writeByte(D::adjust(pixels.loadAndScale0()));
            writeByte(D::adjust(pixels.loadAndScale1()));
            writeByte(D::adjust(pixels.loadAndScale2()));

            pixels.advanceData();
            pixels.stepDithering();
        }
        D::postBlock(len);
        waitFully();
        release();
    }

    /// Finalize transmission (no-op for NRF51 SPI)
    /// This method exists for compatibility with other SPI implementations
    /// that may need to flush buffers or perform post-transmission operations
    static void finalizeTransmission() { }
};

}  // namespace fl

#endif  // FASTLED_FORCE_SOFTWARE_SPI
#endif  // FL_IS_NRF51

#endif
