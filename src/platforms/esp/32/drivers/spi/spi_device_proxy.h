#pragma once

/// @file spi_device_proxy.h
/// @brief ESP32-specific SPI device proxy for transparent Single/Quad-SPI routing
///
/// This proxy sits between LED controllers (APA102, SK9822, etc.) and the actual
/// SPI hardware. It intelligently routes SPI operations to:
/// - Hardware Single-SPI (ESP32SPIOutput) for single strips
/// - Hardware Quad-SPI (SPIQuadESP32 via SPIBusManager) for parallel strips
/// - Software SPI (fallback) if hardware unavailable
///
/// The proxy provides a transparent interface that mirrors ESP32SPIOutput,
/// allowing chipset controllers to work without modification.

#if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32P4)

#include "fl/stl/vector.h"
#include "platforms/shared/spi_bus_manager.h"
#include "platforms/esp/32/core/fastspi_esp32.h"
#include "fl/stl/stdint.h"
#include "fl/stddef.h"
#include "fl/log.h"

namespace fl {

/// SPI Device Proxy - Routes SPI calls to appropriate backend
///
/// This class acts as a drop-in replacement for ESP32SPIOutput in LED controllers.
/// It automatically:
/// 1. Registers with SPIBusManager on init()
/// 2. Routes writes to appropriate backend (Single/Quad/Soft SPI)
/// 3. Buffers data for Quad-SPI and flushes on finalizeTransmission()
///
/// @tparam DATA_PIN GPIO pin for SPI data (MOSI)
/// @tparam CLOCK_PIN GPIO pin for SPI clock (SCK)
/// @tparam SPI_SPEED SPI clock speed in Hz
template<uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint32_t SPI_SPEED>
class SPIDeviceProxy {
private:
    SPIBusHandle mHandle;                    // Handle from SPIBusManager
    SPIBusManager* mBusManager;              // Pointer to global bus manager
    ESP32SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED>* mSingleSPI;  // Owned single-SPI backend
    fl::vector<uint8_t> mWriteBuffer;        // Buffered writes (for Quad-SPI)
    bool mInitialized;                       // Whether init() was called
    bool mBusInitialized;                    // Whether bus manager has been initialized
    bool mInTransaction;                     // Whether select() was called

public:
    /// Constructor - just stores pins, actual setup happens in init()
    SPIDeviceProxy()
        : mHandle()
        , mBusManager(nullptr)
        , mSingleSPI(nullptr)
        , mInitialized(false)
        , mBusInitialized(false)
        , mInTransaction(false)
    {
    }

    /// Destructor - cleanup owned resources and unregister from bus manager
    ~SPIDeviceProxy() {
        // Unregister from bus manager (releases Quad-SPI if last device)
        if (mBusManager && mHandle.is_valid) {
            mBusManager->unregisterDevice(mHandle);
            mHandle = SPIBusHandle();  // Invalidate handle
        }

        // Clean up owned single-SPI backend
        if (mSingleSPI) {
            delete mSingleSPI;
            mSingleSPI = nullptr;
        }
    }

    /// Initialize SPI device and register with bus manager
    /// Called by LED controller's init() method
    void init() {
        if (mInitialized) {
            return;  // Already initialized
        }

        // Get global bus manager
        mBusManager = &getSPIBusManager();

        // Register with bus manager
        // NOTE: Bus manager will determine if we use Single/Quad/Soft SPI
        // based on how many devices share our clock pin
        mHandle = mBusManager->registerDevice(CLOCK_PIN, DATA_PIN, SPI_SPEED, this);

        if (!mHandle.is_valid) {
            FL_LOG_SPI("Failed to register with bus manager (pin "
                    << CLOCK_PIN << ":" << DATA_PIN << ")");
            return;
        }

        // IMPORTANT: DO NOT initialize bus manager here!
        // We defer initialization until the first transmit() call (lazy initialization).
        // This allows all devices on the same clock pin to register before the bus
        // decides whether to use Single-SPI, Dual-SPI, or Quad-SPI mode.
        // If we initialize here, the first device gets SINGLE_SPI mode before other
        // devices have a chance to register, preventing promotion to multi-lane SPI.

        mInitialized = true;
    }

    /// Initialize bus manager (lazy initialization)
    /// Called on first transmit to allow all devices to register
    void ensureBusInitialized() {
        if (mBusInitialized || !mBusManager || !mHandle.is_valid) {
            return;
        }

        // Initialize bus manager if not already done (idempotent)
        mBusManager->initialize();
        mBusInitialized = true;

        // Check what backend we were assigned
        const SPIBusInfo* bus = mBusManager->getBusInfo(mHandle.bus_id);
        if (bus && bus->bus_type == SPIBusType::SINGLE_SPI && !mSingleSPI) {
            // We're using single-SPI - create owned ESP32SPIOutput instance
            mSingleSPI = new ESP32SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED>();
            mSingleSPI->init();
        }
        // For Quad-SPI, bus manager handles hardware - we just buffer writes
    }

    /// Begin SPI transaction
    /// Mirrors ESP32SPIOutput::select()
    void select() {
        if (!mInitialized) {
            return;
        }

        mInTransaction = true;
        mWriteBuffer.clear();  // Reset buffer for new frame

        // Route to backend
        if (mSingleSPI) {
            mSingleSPI->select();
        }
        // Quad-SPI doesn't need select (DMA handles it)
    }

    /// End SPI transaction
    /// Mirrors ESP32SPIOutput::release()
    void release() {
        if (!mInitialized || !mInTransaction) {
            return;
        }

        // Route to backend
        if (mSingleSPI) {
            mSingleSPI->release();
        }
        // Quad-SPI flush happens in finalizeTransmission()

        mInTransaction = false;
    }

    /// End SPI transaction (alias for release)
    /// Added to match the new endTransaction() API used by chipset controllers
    void endTransaction() {
        release();
    }

    /// Write single byte
    /// Mirrors ESP32SPIOutput::writeByte()
    void writeByte(uint8_t b) {
        if (!mInitialized || !mInTransaction) {
            return;
        }

        // Ensure bus is initialized on first transmit
        ensureBusInitialized();

        // Route based on backend type
        if (mSingleSPI) {
            // Direct passthrough to single-SPI hardware
            mSingleSPI->writeByte(b);
        } else {
            // Buffer for Quad-SPI (flushed in finalizeTransmission)
            mWriteBuffer.push_back(b);
        }
    }

    /// Write 16-bit word (big-endian)
    /// Mirrors ESP32SPIOutput::writeWord()
    void writeWord(uint16_t w) {
        writeByte(static_cast<uint8_t>(w >> 8));
        writeByte(static_cast<uint8_t>(w & 0xFF));
    }

    /// Write the same byte value repeatedly
    /// Mirrors ESP32SPIOutput::writeBytesValueRaw()
    void writeBytesValueRaw(uint8_t value, int len) {
        for (int i = 0; i < len; i++) {
            writeByte(value);
        }
    }

    /// Write the same byte value repeatedly with select/release
    /// Mirrors ESP32SPIOutput::writeBytesValue()
    void writeBytesValue(uint8_t value, int len) {
        select();
        writeBytesValueRaw(value, len);
        release();
    }

    /// Write byte without wait (same as writeByte for proxy)
    void writeByteNoWait(uint8_t b) {
        writeByte(b);
    }

    /// Write byte with post-wait (same as writeByte for proxy)
    void writeBytePostWait(uint8_t b) {
        writeByte(b);
    }

    /// Write a single bit (for hardware SPI, tests the specified bit and transmits 0xFF or 0x00)
    /// Note: Hardware SPI transmits full bytes, not individual bits. This tests bit BIT
    /// in the input byte and sends 0xFF if the bit is set, 0x00 if clear.
    /// This matches the behavior of other platform implementations (AVR, ARM, etc.)
    /// @tparam BIT the bit index in the byte to test
    /// @param b the byte to test
    template <uint8_t BIT = 0>
    void writeBit(uint8_t b) {
        // Test bit BIT in value b, send 0xFF if set, 0x00 if clear
        // This matches the behavior of other platforms (AVR, ARM, etc.)
        writeByte((b & (1 << BIT)) ? 0xFF : 0x00);
    }

    /// Wait for SPI to be ready (NOP for buffered writes)
    static void wait() {}
    static void waitFully() {}
    static void stop() {}

    /// Finalize transmission - flush buffered Quad-SPI writes
    /// Must be called after all pixel data is written
    /// Called by chipset controller at end of showPixels()
    void finalizeTransmission() {
        if (!mInitialized) {
            return;
        }

        // Ensure bus is initialized
        ensureBusInitialized();

        // Only needed for Quad-SPI (single-SPI writes directly)
        if (!mSingleSPI && !mWriteBuffer.empty()) {
            // Send buffered data to bus manager for Quad-SPI transmission
            mBusManager->transmit(mHandle, mWriteBuffer.data(), mWriteBuffer.size());
            mBusManager->finalizeTransmission(mHandle);
            mWriteBuffer.clear();
        }
    }

    /// Check if device is enabled (not disabled due to conflicts)
    bool isEnabled() const {
        if (!mBusManager || !mHandle.is_valid) {
            return false;
        }
        return mBusManager->isDeviceEnabled(mHandle);
    }

    /// Get bus type for debugging/testing
    SPIBusType getBusType() const {
        if (!mBusManager || !mHandle.is_valid) {
            return SPIBusType::SOFT_SPI;
        }
        const SPIBusInfo* bus = mBusManager->getBusInfo(mHandle.bus_id);
        return bus ? bus->bus_type : SPIBusType::SOFT_SPI;
    }

private:
    // Disable copy/move
    SPIDeviceProxy(const SPIDeviceProxy&) = delete;
    SPIDeviceProxy& operator=(const SPIDeviceProxy&) = delete;
};

}  // namespace fl

#endif  // ESP32 variants
