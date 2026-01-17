#ifndef __INC_FASTSPI_ARM_STM32_H
#define __INC_FASTSPI_ARM_STM32_H

#ifndef FASTLED_FORCE_SOFTWARE_SPI

// Check if STM32 HAL is available
#if defined(HAL_SPI_MODULE_ENABLED)
    #define FASTLED_STM32_USE_HAL 1
    #include <SPI.h>
#else
    #define FASTLED_STM32_USE_HAL 0
#endif

#include "fastspi_types.h"

namespace fl {

/*
 * STM32 Hardware SPI Driver
 *
 * This hardware SPI implementation supports STM32F1, STM32F4, and other
 * STM32 variants using either the HAL library or direct register access.
 *
 * To enable hardware SPI, add the following line *before* including FastLED.h:
 *
 * #define FASTLED_ALL_PINS_HARDWARE_SPI
 *
 * This driver attempts to use the appropriate SPI peripheral based on the
 * specified clock and data pins. If pins don't match a hardware SPI peripheral,
 * software SPI will be used as fallback.
 *
 * Supported SPI peripherals: SPI1, SPI2, SPI3 (availability varies by chip)
 */

// STM32 SPI Output class template
template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint32_t SPI_SPEED>
class STM32SPIOutput {
private:
#if FASTLED_STM32_USE_HAL
    // Arduino Mbed framework: SPIClass is abstract, use concrete MbedSPI
    // MbedSPI requires pin parameters (MISO, MOSI, SCK) in constructor
    #ifdef ARDUINO_ARCH_MBED
        arduino::MbedSPI* m_spi;
    #else
        SPIClass m_spi;
    #endif
    bool m_initialized;
#endif
    Selectable* m_pSelect;

public:
    // Verify that the pins are valid
    static_assert(FastPin<DATA_PIN>::validpin(), "Invalid data pin specified");
    static_assert(FastPin<CLOCK_PIN>::validpin(), "Invalid clock pin specified");

    STM32SPIOutput()
        : m_pSelect(nullptr)
#if FASTLED_STM32_USE_HAL
        #ifdef ARDUINO_ARCH_MBED
            , m_spi(nullptr)
        #endif
        , m_initialized(false)
#endif
    {
    }

    STM32SPIOutput(Selectable* pSelect)
        : m_pSelect(pSelect)
#if FASTLED_STM32_USE_HAL
        #ifdef ARDUINO_ARCH_MBED
            , m_spi(nullptr)
        #endif
        , m_initialized(false)
#endif
    {
    }

#if FASTLED_STM32_USE_HAL && defined(ARDUINO_ARCH_MBED)
    // Destructor for Arduino Mbed - clean up allocated SPIClass
    ~STM32SPIOutput() {
        if (m_spi) {
            delete m_spi;
            m_spi = nullptr;
        }
    }
#endif

    // Set the object representing the selectable
    void setSelect(Selectable* pSelect) {
        m_pSelect = pSelect;
    }

    // Initialize the SPI subsystem
    void init() {
#if FASTLED_STM32_USE_HAL
        if (!m_initialized) {
            // Initialize SPI with specified pins
            // Note: STM32duino's SPI.begin() uses the default SPI pins
            // For custom pins, we rely on the board's pin mapping
            #ifdef ARDUINO_ARCH_MBED
                if (!m_spi) {
                    // Arduino Mbed requires MbedSPI with pin parameters
                    // SPIClass is abstract, so we use MbedSPI(MISO, MOSI, SCK)
                    m_spi = new arduino::MbedSPI(SPI_MISO, SPI_MOSI, SPI_SCK);
                }
                m_spi->begin();
            #else
                m_spi.begin();
            #endif
            m_initialized = true;
        }
#else
        // Set pins to output mode
        FastPin<DATA_PIN>::setOutput();
        FastPin<CLOCK_PIN>::setOutput();

        // For non-HAL builds, we'll use software SPI
        // This is a fallback when HAL is not available
        FastPin<CLOCK_PIN>::lo();
        FastPin<DATA_PIN>::lo();
#endif
        release();
    }

    // Stop the SPI output
    static void stop() {
        // NOP for now - could disable SPI peripheral if needed
    }

    // Wait until the SPI subsystem is ready for more data
    static void wait() __attribute__((always_inline)) {
        // For basic implementation, this is a NOP
        // Could check SPI busy flag if needed
    }

    static void waitFully() __attribute__((always_inline)) {
        wait();
    }

    // Write byte variants
    void writeByteNoWait(uint8_t b) __attribute__((always_inline)) {
        writeByte(b);
    }

    void writeBytePostWait(uint8_t b) __attribute__((always_inline)) {
        writeByte(b);
        wait();
    }

    // Write a 16-bit word
    void writeWord(uint16_t w) __attribute__((always_inline)) {
        writeByte(static_cast<uint8_t>(w >> 8));
        writeByte(static_cast<uint8_t>(w & 0xFF));
    }

    // Write a single byte via SPI
    void writeByte(uint8_t b) {
#if FASTLED_STM32_USE_HAL
        #ifdef ARDUINO_ARCH_MBED
            if (m_spi) {
                m_spi->transfer(b);
            }
        #else
            m_spi.transfer(b);
        #endif
#else
        // Software SPI fallback - bitbang implementation
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (b & 0x80) {
                FastPin<DATA_PIN>::hi();
            } else {
                FastPin<DATA_PIN>::lo();
            }
            FastPin<CLOCK_PIN>::hi();
            b <<= 1;
            FastPin<CLOCK_PIN>::lo();
        }
#endif
    }

    // Select the SPI output (begin transaction)
    void select() {
#if FASTLED_STM32_USE_HAL
        // Calculate SPI settings based on SPI_SPEED
        // STM32 SPI can typically handle up to 18 MHz on APB1, 36 MHz on APB2
        uint32_t spi_speed = SPI_SPEED;
        if (spi_speed > 36000000) spi_speed = 36000000; // Cap at 36 MHz

        #ifdef ARDUINO_ARCH_MBED
            if (m_spi) {
                m_spi->beginTransaction(SPISettings(spi_speed, MSBFIRST, SPI_MODE0));
            }
        #else
            m_spi.beginTransaction(SPISettings(spi_speed, MSBFIRST, SPI_MODE0));
        #endif
#endif
        if (m_pSelect != nullptr) {
            m_pSelect->select();
        }
    }

    // Release the SPI line (end transaction)
    void release() {
        if (m_pSelect != nullptr) {
            m_pSelect->release();
        }
#if FASTLED_STM32_USE_HAL
        #ifdef ARDUINO_ARCH_MBED
            if (m_spi) {
                m_spi->endTransaction();
            }
        #else
            m_spi.endTransaction();
        #endif
#endif
    }

    void endTransaction() {
        waitFully();
        release();
    }

    // Write out len bytes of the given value
    void writeBytesValue(uint8_t value, int len) {
        select();
        writeBytesValueRaw(value, len);
        release();
    }

    void writeBytesValueRaw(uint8_t value, int len) {
        while (len--) {
#if FASTLED_STM32_USE_HAL
            #ifdef ARDUINO_ARCH_MBED
                if (m_spi) {
                    m_spi->transfer(value);
                }
            #else
                m_spi.transfer(value);
            #endif
#else
            writeByte(value);
#endif
        }
    }

    // Write a block of bytes with per-byte data modifier
    template <class D>
    void writeBytes(FASTLED_REGISTER uint8_t* data, int len) {
        select();
        uint8_t* end = data + len;
        while (data != end) {
            writeByte(D::adjust(*data++));
        }
        D::postBlock(len, this);
        release();
    }

    // Default version - write a block of data with no modifications
    void writeBytes(FASTLED_REGISTER uint8_t* data, int len) {
        writeBytes<DATA_NOP>(data, len);
    }

    /// Finalize transmission (no-op for standard STM32 SPI)
    /// This method exists for compatibility with Quad-SPI implementations
    /// For standard single-line SPI, no additional operations are needed
    static void finalizeTransmission() { }

    // Write a single bit out
    template <uint8_t BIT>
    inline void writeBit(uint8_t b) {
#if FASTLED_STM32_USE_HAL
        // For single bit writes, we need to use GPIO directly
        // This is typically only used for specific LED chipsets
        waitFully();
        if (b & (1 << BIT)) {
            FastPin<DATA_PIN>::hi();
        } else {
            FastPin<DATA_PIN>::lo();
        }
        FastPin<CLOCK_PIN>::hi();
        FastPin<CLOCK_PIN>::lo();
#else
        if (b & (1 << BIT)) {
            FastPin<DATA_PIN>::hi();
        } else {
            FastPin<DATA_PIN>::lo();
        }
        FastPin<CLOCK_PIN>::hi();
        FastPin<CLOCK_PIN>::lo();
#endif
    }

    // Write pixels with color order and optional flags
    template <uint8_t FLAGS, class D, EOrder RGB_ORDER>
    __attribute__((noinline))
    void writePixels(PixelController<RGB_ORDER> pixels, void* context = nullptr) {
        select();
        int len = pixels.mLen;

        while (pixels.has(1)) {
            if (FLAGS & FLAG_START_BIT) {
                writeBit<0>(1);
            }
            writeByte(D::adjust(pixels.loadAndScale0()));
            writeByte(D::adjust(pixels.loadAndScale1()));
            writeByte(D::adjust(pixels.loadAndScale2()));
            pixels.advanceData();
            pixels.stepDithering();
        }
        D::postBlock(len, context);
        release();
    }
};

}  // namespace fl

#endif // #ifndef FASTLED_FORCE_SOFTWARE_SPI

#endif // #ifndef __INC_FASTSPI_ARM_STM32_H
