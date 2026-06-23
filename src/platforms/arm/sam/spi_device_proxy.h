#pragma once

// IWYU pragma: private

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

#include "platforms/arm/samd/is_samd.h"

#if defined(FL_IS_SAMD21) || defined(FL_IS_SAMD51)

#include "fl/stl/unique_ptr.h"
#include "fl/stl/vector.h"
#include "platforms/shared/spi_manager.h"
#include "platforms/arm/sam/fastspi_arm_sam.h"
#include "fl/stl/stdint.h"
#include "fl/stl/stddef.h"
#include "fl/stl/noexcept.h"

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
template<u8 DATA_PIN, u8 CLOCK_PIN, u32 SPI_CLOCK_DIVIDER>
class SPIDeviceProxy {
private:
    SPIBusHandle mHandle;                    // Handle from SPIBusManager
    SPIBusManager* mBusManager;              // Pointer to global bus manager
    fl::unique_ptr<SAMDHardwareSPIOutput<DATA_PIN, CLOCK_PIN, SPI_CLOCK_DIVIDER>> mSingleSPI;
    fl::vector<u8> mWriteBuffer;        // Buffered writes (for Dual/Quad-SPI)
    bool mInitialized;                       // Whether init() was called
    bool mBusInitialized;                    // Whether bus manager has been initialized
    bool mInTransaction;                     // Whether select() was called

public:
    /// Constructor - just stores pins, actual setup happens in init()
    SPIDeviceProxy()
        : mHandle()
        , mBusManager(nullptr)
        , mInitialized(false)
        , mBusInitialized(false)
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

        mSingleSPI.reset();
    }

    /// Initialize SPI device and register with bus manager
    /// Called by LED controller's init() method
    void init() FL_NOEXCEPT {
        if (mInitialized) {
            return;  // Already initialized
        }

        // Get global bus manager
        mBusManager = &getSPIBusManager();

        // Register with bus manager
        // NOTE: Bus manager will determine if we use Single/Dual/Quad SPI
        // based on how many devices share our clock pin
        // Calculate SPI speed from clock divider: speed_hz = F_CPU / divider
        constexpr u32 spi_speed_hz = F_CPU / SPI_CLOCK_DIVIDER;
        mHandle = mBusManager->registerDevice(CLOCK_PIN, DATA_PIN, spi_speed_hz, this);

        if (!mHandle.is_valid) {
            FL_WARN("SPIDeviceProxy: Failed to register with bus manager (pin "
                    << static_cast<int>(CLOCK_PIN) << ":" << static_cast<int>(DATA_PIN) << ")");
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
    void ensureBusInitialized() FL_NOEXCEPT {
        if (mBusInitialized || !mBusManager || !mHandle.is_valid) {
            return;
        }

        // Initialize bus manager if not already done (idempotent)
        mBusManager->initialize();
        mBusInitialized = true;

        // Check what backend we were assigned
        const SPIBusInfo* bus = mBusManager->getBusInfo(mHandle.bus_id);
        if (bus && bus->bus_type == SPIBusType::SINGLE_SPI && !mSingleSPI) {
            // We're using single-SPI - create owned SAMDHardwareSPIOutput instance
            mSingleSPI = fl::make_unique<SAMDHardwareSPIOutput<DATA_PIN, CLOCK_PIN, SPI_CLOCK_DIVIDER>>();
            mSingleSPI->init();
        }
        // For Dual/Quad-SPI, bus manager handles hardware - we just buffer writes
    }

    /// Begin SPI transaction
    /// Mirrors SAMHardwareSPIOutput::select()
    void select() FL_NOEXCEPT {
        if (!mInitialized) {
            return;
        }

        // Ensure bus is initialized before first transaction
        ensureBusInitialized();

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
    void release() FL_NOEXCEPT {
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
    void writeByte(u8 b) FL_NOEXCEPT {
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
            // Buffer for Dual/Quad-SPI (flushed in finalizeTransmission)
            mWriteBuffer.push_back(b);
        }
    }

    /// Write 16-bit word
    /// Mirrors SAMHardwareSPIOutput::writeWord()
    void writeWord(u16 w) FL_NOEXCEPT {
        if (!mInitialized || !mInTransaction) {
            return;
        }
        ensureBusInitialized();
        if (mSingleSPI) {
            mSingleSPI->writeWord(w);
        } else {
            // Buffer as two bytes (big-endian)
            writeByte(static_cast<u8>(w >> 8));
            writeByte(static_cast<u8>(w & 0xFF));
        }
    }

    /// Wait for SPI to be ready
    /// Mirrors SAMHardwareSPIOutput::waitFully()
    void waitFully() FL_NOEXCEPT {
        if (mSingleSPI) {
            mSingleSPI->waitFully();
        }
        // Dual/Quad-SPI waits in finalizeTransmission()
    }

    /// Finalize transmission - flush buffered Dual/Quad-SPI writes
    /// Must be called after all pixel data is written
    /// Called by chipset controller at end of showPixels()
    void finalizeTransmission() FL_NOEXCEPT {
        if (!mInitialized) {
            return;
        }

        // Ensure bus is initialized
        ensureBusInitialized();

        // Only needed for Dual/Quad-SPI (single-SPI writes directly)
        if (!mSingleSPI && !mWriteBuffer.empty()) {
            // Send buffered data to bus manager for Dual/Quad-SPI transmission
            mBusManager->transmit(mHandle, mWriteBuffer.data(), mWriteBuffer.size());
            mBusManager->finalizeTransmission(mHandle);
            mWriteBuffer.clear();
        }
    }

    /// Check if device is enabled (not disabled due to conflicts)
    bool isEnabled() const FL_NOEXCEPT {
        if (!mBusManager || !mHandle.is_valid) {
            return false;
        }
        return mBusManager->isDeviceEnabled(mHandle);
    }

    /// Get bus type for debugging/testing
    SPIBusType getBusType() const FL_NOEXCEPT {
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

