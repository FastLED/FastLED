#pragma once

/// @file spi_device_proxy.h
/// @brief STM32-specific SPI device proxy for transparent Single/Multi-SPI routing
///
/// This proxy sits between LED controllers (APA102, SK9822, etc.) and the actual
/// SPI hardware. It intelligently routes SPI operations to:
/// - Hardware Single-SPI (STM32SPIOutput) for single strips
/// - Hardware Dual/Quad/Octal-SPI (via SPIBusManager) for parallel strips
/// - Software SPI (fallback) if hardware unavailable
///
/// The proxy provides a transparent interface that mirrors STM32SPIOutput,
/// allowing chipset controllers to work without modification.

#include "platforms/is_platform.h"

#ifdef FL_IS_STM32

#include "fl/stl/vector.h"
#include "platforms/shared/spi_bus_manager.h"
#include "platforms/arm/stm32/fastspi_arm_stm32.h"
#include "fl/stl/stdint.h"
#include "fl/stddef.h"

namespace fl {

/// SPI Device Proxy - Routes SPI calls to appropriate backend
///
/// This class acts as a drop-in replacement for STM32SPIOutput in LED controllers.
/// It automatically:
/// 1. Registers with SPIBusManager on init()
/// 2. Routes writes to appropriate backend (Single/Dual/Quad/Octal SPI)
/// 3. Buffers data for Multi-lane SPI and flushes on finalizeTransmission()
///
/// @tparam DATA_PIN GPIO pin for SPI data (MOSI)
/// @tparam CLOCK_PIN GPIO pin for SPI clock (SCK)
/// @tparam SPI_SPEED SPI clock speed in Hz
template<uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint32_t SPI_SPEED>
class SPIDeviceProxy {
private:
    SPIBusHandle mHandle;                    // Handle from SPIBusManager
    SPIBusManager* mBusManager;              // Pointer to global bus manager
    STM32SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED>* mSingleSPI;  // Owned single-SPI backend
    fl::vector<uint8_t> mWriteBuffer;        // Buffered writes (for Multi-lane SPI)
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
        // Unregister from bus manager (releases Multi-lane SPI if last device)
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
        // NOTE: Bus manager will determine if we use Single/Dual/Quad/Octal SPI
        // based on how many devices share our clock pin
        mHandle = mBusManager->registerDevice(CLOCK_PIN, DATA_PIN, SPI_SPEED, this);

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
            // We're using single-SPI - create owned STM32SPIOutput instance
            mSingleSPI = new STM32SPIOutput<DATA_PIN, CLOCK_PIN, SPI_SPEED>();
            mSingleSPI->init();
        }
        // For Multi-lane SPI (Dual/Quad/Octal), bus manager handles hardware - we just buffer writes

        mInitialized = true;
    }

    /// Begin SPI transaction
    /// Mirrors STM32SPIOutput::select()
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
        // Multi-lane SPI doesn't need select (DMA handles it)
    }

    /// End SPI transaction
    /// Mirrors STM32SPIOutput::release()
    void release() {
        if (!mInitialized || !mInTransaction) {
            return;
        }

        // Route to backend
        if (mSingleSPI) {
            mSingleSPI->release();
        }
        // Multi-lane SPI flush happens in finalizeTransmission()

        mInTransaction = false;
    }

    /// Write single byte
    /// Mirrors STM32SPIOutput::writeByte()
    void writeByte(uint8_t b) {
        if (!mInitialized || !mInTransaction) {
            return;
        }

        // Route based on backend type
        if (mSingleSPI) {
            // Direct passthrough to single-SPI hardware
            mSingleSPI->writeByte(b);
        } else {
            // Buffer for Multi-lane SPI (flushed in finalizeTransmission)
            mWriteBuffer.push_back(b);
        }
    }

    /// Write 16-bit word (big-endian)
    /// Mirrors STM32SPIOutput::writeWord()
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

    /// Finalize transmission - flush buffered Multi-lane SPI writes
    /// Must be called after all pixel data is written
    /// Called by chipset controller at end of showPixels()
    void finalizeTransmission() {
        if (!mInitialized) {
            return;
        }

        // Only needed for Multi-lane SPI (single-SPI writes directly)
        if (!mSingleSPI && !mWriteBuffer.empty()) {
            // Send buffered data to bus manager for Multi-lane SPI transmission
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

#endif  // FL_IS_STM32
