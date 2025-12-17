#pragma once

/// @file spi_device_proxy.h
/// @brief SAMD21/SAMD51 SPI device proxy for transparent Single/Dual/Quad-SPI routing
///
/// This proxy sits between LED controllers (APA102, SK9822, etc.) and the actual
/// SPI hardware. It intelligently routes SPI operations to:
/// - Hardware Single-SPI (SAMDHardwareSPIOutput) for single strips
/// - Hardware Dual-SPI (SpiHw2 via SPIBusManager) for 2 parallel strips (future)
/// - Hardware Quad-SPI (SpiHw4 via SPIBusManager) for 3-4 parallel strips (SAMD51 only, future)
///
/// The proxy provides a transparent interface that mirrors SAMDHardwareSPIOutput,
/// allowing chipset controllers to work without modification.
///
/// Note: Currently operates in single-SPI mode. Dual/Quad-SPI support requires
/// additional hardware driver implementation and bus manager integration.

#if defined(__SAMD21G18A__) || defined(__SAMD21J18A__) || defined(__SAMD21E17A__) || \
    defined(__SAMD21E18A__) || defined(__SAMD51G19A__) || defined(__SAMD51J19A__) || \
    defined(__SAME51J19A__) || defined(__SAMD51P19A__) || defined(__SAMD51P20A__)

#include "fl/stl/vector.h"
#include "platforms/shared/spi_bus_manager.h"
#include "platforms/arm/sam/fastspi_arm_sam.h"
#include "fl/stl/stdint.h"
#include "fl/stddef.h"

namespace fl {

/// SPI Device Proxy - Routes SPI calls to appropriate backend
///
/// This class acts as a drop-in replacement for SAMHardwareSPIOutput in LED controllers.
/// It automatically:
/// 1. Registers with SPIBusManager on init()
/// 2. Routes writes to appropriate backend (Single/Dual/Quad SPI)
/// 3. Buffers data for Dual/Quad-SPI and flushes on finalizeTransmission()
///
/// @tparam DATA_PIN GPIO pin for SPI data (MOSI)
/// @tparam CLOCK_PIN GPIO pin for SPI clock (SCK)
/// @tparam SPI_CLOCK_DIVIDER SPI clock divider (matches SAMHardwareSPIOutput)
template<uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint32_t SPI_CLOCK_DIVIDER>
class SPIDeviceProxy {
private:
    SPIBusHandle mHandle;                    // Handle from SPIBusManager
    SPIBusManager* mBusManager;              // Pointer to global bus manager
    SAMDHardwareSPIOutput<DATA_PIN, CLOCK_PIN, SPI_CLOCK_DIVIDER>* mSingleSPI;
    fl::vector<uint8_t> mWriteBuffer;        // Buffered writes (for Dual/Quad-SPI)
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
        // Unregister from bus manager (releases Dual/Quad-SPI if last device)
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
        // NOTE: Bus manager will determine if we use Single/Dual/Quad SPI
        // based on how many devices share our clock pin
        // Calculate SPI speed from clock divider: speed_hz = F_CPU / divider
        constexpr uint32_t spi_speed_hz = F_CPU / SPI_CLOCK_DIVIDER;
        mHandle = mBusManager->registerDevice(CLOCK_PIN, DATA_PIN, spi_speed_hz, this);

        if (!mHandle.is_valid) {
            FL_WARN("SPIDeviceProxy: Failed to register with bus manager (pin "
                    << static_cast<int>(CLOCK_PIN) << ":" << static_cast<int>(DATA_PIN) << ")");
            return;
        }

        // Initialize bus manager (idempotent - only runs once globally)
        mBusManager->initialize();

        // Check what backend we were assigned
        const SPIBusInfo* bus = mBusManager->getBusInfo(mHandle.bus_id);
        if (bus && bus->bus_type == SPIBusType::SINGLE_SPI) {
            // We're using single-SPI - create owned SAMDHardwareSPIOutput instance
            mSingleSPI = new SAMDHardwareSPIOutput<DATA_PIN, CLOCK_PIN, SPI_CLOCK_DIVIDER>();
            mSingleSPI->init();
        }
        // For Dual/Quad-SPI, bus manager handles hardware - we just buffer writes

        mInitialized = true;
    }

    /// Begin SPI transaction
    /// Mirrors SAMHardwareSPIOutput::select()
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
        // Dual/Quad-SPI doesn't need select (DMA handles it)
    }

    /// End SPI transaction
    /// Mirrors SAMHardwareSPIOutput::release()
    void release() {
        if (!mInitialized || !mInTransaction) {
            return;
        }

        // Route to backend
        if (mSingleSPI) {
            mSingleSPI->release();
        }
        // Dual/Quad-SPI flush happens in finalizeTransmission()

        mInTransaction = false;
    }

    /// Write single byte
    /// Mirrors SAMHardwareSPIOutput::writeByte()
    void writeByte(uint8_t b) {
        if (!mInitialized || !mInTransaction) {
            return;
        }

        // Route based on backend type
        if (mSingleSPI) {
            // Direct passthrough to single-SPI hardware
            mSingleSPI->writeByte(b);
        } else {
            // Buffer for Dual/Quad-SPI (flushed in finalizeTransmission)
            mWriteBuffer.push_back(b);
        }
    }

    /// Write 16-bit word
    /// Mirrors SAMHardwareSPIOutput::writeWord()
    void writeWord(uint16_t w) {
        if (mSingleSPI) {
            mSingleSPI->writeWord(w);
        } else {
            // Buffer as two bytes (big-endian)
            writeByte(static_cast<uint8_t>(w >> 8));
            writeByte(static_cast<uint8_t>(w & 0xFF));
        }
    }

    /// Wait for SPI to be ready
    /// Mirrors SAMHardwareSPIOutput::waitFully()
    void waitFully() {
        if (mSingleSPI) {
            mSingleSPI->waitFully();
        }
        // Dual/Quad-SPI waits in finalizeTransmission()
    }

    /// Finalize transmission - flush buffered Dual/Quad-SPI writes
    /// Must be called after all pixel data is written
    /// Called by chipset controller at end of showPixels()
    void finalizeTransmission() {
        if (!mInitialized) {
            return;
        }

        // Only needed for Dual/Quad-SPI (single-SPI writes directly)
        if (!mSingleSPI && !mWriteBuffer.empty()) {
            // Send buffered data to bus manager for Dual/Quad-SPI transmission
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

#endif  // SAMD21/SAMD51 platforms

