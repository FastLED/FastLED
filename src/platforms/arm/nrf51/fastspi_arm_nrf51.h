// ok no namespace fl
#ifndef __INC_FASTSPI_NRF_H
#define __INC_FASTSPI_NRF_H

#include "platforms/arm/nrf52/is_nrf52.h"

#ifdef FL_IS_NRF52

#ifndef FASTLED_FORCE_SOFTWARE_SPI
#define FASTLED_ALL_PINS_HARDWARE_SPI

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
        fl::u32 enable;
    } mSavedData;

    void saveSPIData() {
        mSavedData.sck = NRF_SPI0->PSELSCK;
        mSavedData.mosi = NRF_SPI0->PSELMOSI;
        mSavedData.miso = NRF_SPI0->PSELMISO;
        mSavedData.freq = NRF_SPI0->FREQUENCY;
        mSavedData.enable = NRF_SPI0->ENABLE;
    }

    void restoreSPIData() {
        NRF_SPI0->PSELSCK = mSavedData.sck;
        NRF_SPI0->PSELMOSI = mSavedData.mosi;
        NRF_SPI0->PSELMISO = mSavedData.miso;
        NRF_SPI0->FREQUENCY = mSavedData.freq;
        mSavedData.enable = NRF_SPI0->ENABLE;
    }

public:
    NRF51SPIOutput() { FastPin<_DATA_PIN>::setOutput(); FastPin<_CLOCK_PIN>::setOutput(); }
    NRF51SPIOutput(Selectable *pSelect) {  FastPin<_DATA_PIN>::setOutput(); FastPin<_CLOCK_PIN>::setOutput();  }

    // set the object representing the selectable
    void setSelect(Selectable *pSelect) { /* TODO */ }

    // initialize the SPI subssytem
    void init() {
        FastPin<_DATA_PIN>::setOutput();
        FastPin<_CLOCK_PIN>::setOutput();
        NRF_SPI0->PSELSCK = _CLOCK_PIN;
        NRF_SPI0->PSELMOSI = _DATA_PIN;
        NRF_SPI0->PSELMISO = 0xFFFFFFFF;
        NRF_SPI0->FREQUENCY = 0x80000000;
        NRF_SPI0->ENABLE = 1;
        NRF_SPI0->EVENTS_READY = 0;
    }

    // latch the CS select
    void select() { saveSPIData(); init(); }

    // release the CS select
    void release() { shouldWait(); restoreSPIData(); }

    void endTransaction() {
        waitFully();
        release();
    }

    static bool shouldWait(bool wait = false) __attribute__((always_inline)) __attribute__((always_inline)) {
        // static bool sWait=false;
        // bool oldWait = sWait;
        // sWait = wait;
        // never going to bother with waiting since we're always running the spi clock at max speed on the rfduino
        // TODO: When we set clock rate, implement/fix waiting properly, otherwise the world hangs up
        return false;
    }
    
    // wait until all queued up data has been written
    static void waitFully() __attribute__((always_inline)){ if(shouldWait()) { while(NRF_SPI0->EVENTS_READY==0); } NRF_SPI0->INTENCLR; }
    static void wait() __attribute__((always_inline)){ if(shouldWait()) { while(NRF_SPI0->EVENTS_READY==0); } NRF_SPI0->INTENCLR; }

    // write a byte out via SPI (returns immediately on writing register)
    static void writeByte(fl::u8 b) __attribute__((always_inline)) { wait(); NRF_SPI0->TXD = b; NRF_SPI0->INTENCLR; shouldWait(true); }

    // write a word out via SPI (returns immediately on writing register)
    static void writeWord(fl::u16 w) __attribute__((always_inline)){ writeByte(w>>8); writeByte(w & 0xFF);  }

    // A raw set of writing byte values, assumes setup/init/waiting done elsewhere (static for use by adjustment classes)
    static void writeBytesValueRaw(fl::u8 value, int len) { while(len--) { writeByte(value);  } }

    // A full cycle of writing a value for len bytes, including select, release, and waiting
    void writeBytesValue(fl::u8 value, int len) {
        select();
        while(len--) {
            writeByte(value);
        }
        waitFully();
        release();
    }

    // A full cycle of writing a raw block of data out, including select, release, and waiting
    template<class D> void writeBytes(fl::u8 *data, int len) {
        fl::u8 *end = data + len;
        select();
        while(data != end) {
            writeByte(D::adjust(*data++));
        }
        D::postBlock(len);
        waitFully();
        release();
    }

    void writeBytes(fl::u8 *data, int len) {
        writeBytes<DATA_NOP>(data, len);
    }

    // write a single bit out, which bit from the passed in byte is determined by template parameter
    template <fl::u8 BIT> inline static void writeBit(fl::u8 b) {
        waitFully();
        NRF_SPI0->ENABLE = 0;
        if(b & 1<<BIT) {
            FastPin<_DATA_PIN>::hi();
        } else {
            FastPin<_DATA_PIN>::lo();
        }
        FastPin<_CLOCK_PIN>::toggle();
        FastPin<_CLOCK_PIN>::toggle();
        NRF_SPI0->ENABLE = 1;
    }

    template <fl::u8 FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels, void* context = nullptr) {
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

#endif
#endif

#endif
