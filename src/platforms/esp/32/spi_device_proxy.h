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

#include "fl/namespace.h"
#include "fl/vector.h"
#include "platforms/shared/spi_bus_manager.h"
#include "platforms/esp/32/fastspi_esp32.h"
#include <stdint.h>
#include <stddef.h>

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
    bool mInTransaction;                     // Whether select() was called

public:
    /// Constructor - just stores pins, actual setup happens in init()
    SPIDeviceProxy()
        : mHandle()
        , mBusManager(nullptr)
        , mSingleSPI(nullptr)
        , mInitialized(false)
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
        mHandle = mBusManager->registerDevice(CLOCK_PIN, DATA_PIN, this);

        if (!mHandle.is_valid) {
            FL_WARN("SPIDeviceProxy: Failed to register with bus manager (pin "
                    << CLOCK_PIN << ":" << DATA_PIN << ")");
            return;
        }

        // Initialize bus manager (idempotent - only runs once globally)
        mBusManager->initialize();

        // Check what backend we were assigned
        const SPIBusInfo* bus = mBusManager->getBusInfo(mHandle.bus_id);
        if (bus && bus->bus_type == SPIBusType::SINGLE_SPI) {
            // We're using single-SPI - create owned ESP32SPIOutput instance
            mSingleSPI = new ESP32SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED>();
            mSingleSPI->init();
        }
        // For Quad-SPI, bus manager handles hardware - we just buffer writes

        mInitialized = true;
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

    /// Write single byte
    /// Mirrors ESP32SPIOutput::writeByte()
    void writeByte(uint8_t b) {
        if (!mInitialized || !mInTransaction) {
            return;
        }

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

    /// Write byte without wait (same as writeByte for proxy)
    void writeByteNoWait(uint8_t b) {
        writeByte(b);
    }

    /// Write byte with post-wait (same as writeByte for proxy)
    void writeBytePostWait(uint8_t b) {
        writeByte(b);
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
