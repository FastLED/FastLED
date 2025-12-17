#pragma once

/// @file spi_device_proxy.h
/// @brief NRF52-specific SPI device proxy for transparent Single/Dual/Quad/Octal-SPI routing
///
/// This proxy sits between LED controllers (APA102, SK9822, etc.) and the actual
/// SPI hardware. It intelligently routes SPI operations to:
/// - Hardware Single-SPI (NRF52SPIOutput) for single strips
/// - Hardware Multi-lane SPI (SpiHw2/SpiHw4/SpiHw8 via SPIBusManager) for parallel strips
///
/// The proxy provides a transparent interface that mirrors NRF52SPIOutput,
/// allowing chipset controllers to work without modification.
///
/// ## NRF52 Hardware Approach
///
/// Unlike ESP32/RP2040 which have native multi-lane SPI hardware, NRF52 uses
/// GPIOTE + TIMER + PPI to synchronize multiple SPIM peripherals:
///
/// - **NRF52832**: 3× SPIM (8 MHz max) + GPIOTE + PPI
/// - **NRF52840**: 4× SPIM (SPIM3 @ 32 MHz, others @ 8 MHz) + GPIOTE + PPI
///
/// The parallel SPI implementation (when created) will use:
/// - SPIM0/1/2(/3) for data transmission (EasyDMA)
/// - TIMER for clock synchronization
/// - GPIOTE for GPIO control via tasks
/// - PPI to route TIMER events to GPIOTE tasks (hardware-level sync)

#if defined(NRF52) || defined(NRF52832) || defined(NRF52840) || defined(NRF52833)

#include "fl/stl/vector.h"
#include "platforms/shared/spi_bus_manager.h"
#include "platforms/arm/nrf52/fastspi_arm_nrf52.h"
#include "fl/stl/stdint.h"
#include "fl/stddef.h"

namespace fl {

/// SPI Device Proxy - Routes SPI calls to appropriate backend
///
/// This class acts as a drop-in replacement for NRF52SPIOutput in LED controllers.
/// It automatically:
/// 1. Registers with SPIBusManager on init()
/// 2. Routes writes to appropriate backend (Single/Dual/Quad/Octal SPI)
/// 3. Buffers data for multi-lane SPI and flushes on finalizeTransmission()
///
/// @tparam DATA_PIN GPIO pin for SPI data (MOSI)
/// @tparam CLOCK_PIN GPIO pin for SPI clock (SCK)
/// @tparam SPI_CLOCK_DIVIDER SPI clock divider (see NRF52SPIOutput for details)
template<uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint32_t SPI_CLOCK_DIVIDER>
class SPIDeviceProxy {
private:
    SPIBusHandle mHandle;                    // Handle from SPIBusManager
    SPIBusManager* mBusManager;              // Pointer to global bus manager
    NRF52SPIOutput<DATA_PIN, CLOCK_PIN, SPI_CLOCK_DIVIDER>* mSingleSPI;  // Owned single-SPI backend
    fl::vector<uint8_t> mWriteBuffer;        // Buffered writes (for multi-lane SPI)
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
        // Unregister from bus manager (releases multi-lane SPI if last device)
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
                    << static_cast<int>(CLOCK_PIN) << ":" << static_cast<int>(DATA_PIN) << ")");
            return;
        }

        // Initialize bus manager (idempotent - only runs once globally)
        mBusManager->initialize();

        // Check what backend we were assigned
        const SPIBusInfo* bus = mBusManager->getBusInfo(mHandle.bus_id);
        if (bus && bus->bus_type == SPIBusType::SINGLE_SPI) {
            // We're using single-SPI - create owned NRF52SPIOutput instance
            mSingleSPI = new NRF52SPIOutput<DATA_PIN, CLOCK_PIN, SPI_CLOCK_DIVIDER>();
            mSingleSPI->init();
        }
        // For multi-lane SPI, bus manager handles hardware - we just buffer writes

        mInitialized = true;
    }

    /// Begin SPI transaction
    /// Mirrors NRF52SPIOutput::select()
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
    /// Mirrors NRF52SPIOutput::release()
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
    /// Mirrors NRF52SPIOutput::writeByte()
    void writeByte(uint8_t b) {
        if (!mInitialized || !mInTransaction) {
            return;
        }

        // Route based on backend type
        if (mSingleSPI) {
            // Direct passthrough to single-SPI hardware
            mSingleSPI->writeByte(b);
        } else {
            // Buffer for multi-lane SPI (flushed in finalizeTransmission)
            mWriteBuffer.push_back(b);
        }
    }

    /// Write 16-bit word (big-endian)
    /// Mirrors NRF52SPIOutput::writeWord()
    void writeWord(uint16_t w) {
        if (!mInitialized || !mInTransaction) {
            return;
        }

        // Route based on backend type
        if (mSingleSPI) {
            // Direct passthrough to single-SPI hardware
            mSingleSPI->writeWord(w);
        } else {
            // Buffer for multi-lane SPI (flushed in finalizeTransmission)
            mWriteBuffer.push_back(static_cast<uint8_t>(w >> 8));
            mWriteBuffer.push_back(static_cast<uint8_t>(w & 0xFF));
        }
    }

    /// Write byte values (repeated value)
    /// Mirrors NRF52SPIOutput::writeBytesValue()
    void writeBytesValue(uint8_t value, int len) {
        if (!mInitialized) {
            return;
        }

        // Route based on backend type
        if (mSingleSPI) {
            // Direct passthrough to single-SPI hardware
            mSingleSPI->writeBytesValue(value, len);
        } else {
            // Must be in transaction for buffered writes
            if (!mInTransaction) {
                return;
            }
            // Buffer for multi-lane SPI (flushed in finalizeTransmission)
            for (int i = 0; i < len; i++) {
                mWriteBuffer.push_back(value);
            }
        }
    }

    /// Write byte buffer
    /// Mirrors NRF52SPIOutput::writeBytes()
    void writeBytes(uint8_t* data, int len) {
        if (!mInitialized) {
            return;
        }

        // Route based on backend type
        if (mSingleSPI) {
            // Direct passthrough to single-SPI hardware
            mSingleSPI->writeBytes(data, len);
        } else {
            // Must be in transaction for buffered writes
            if (!mInTransaction) {
                return;
            }
            // Buffer for multi-lane SPI (flushed in finalizeTransmission)
            for (int i = 0; i < len; i++) {
                mWriteBuffer.push_back(data[i]);
            }
        }
    }

    /// Write byte buffer with adjustment
    /// Mirrors NRF52SPIOutput::writeBytes<D>()
    template<class D>
    void writeBytes(uint8_t* data, int len) {
        if (!mInitialized) {
            return;
        }

        // Route based on backend type
        if (mSingleSPI) {
            // Direct passthrough to single-SPI hardware
            mSingleSPI->writeBytes<D>(data, len);
        } else {
            // Must be in transaction for buffered writes
            if (!mInTransaction) {
                return;
            }
            // Buffer for multi-lane SPI with adjustment (flushed in finalizeTransmission)
            for (int i = 0; i < len; i++) {
                mWriteBuffer.push_back(D::adjust(data[i]));
            }
            D::postBlock(len);
        }
    }

    /// Write a single bit
    /// Mirrors NRF52SPIOutput::writeBit()
    template <uint8_t BIT>
    void writeBit(uint8_t b) {
        if (!mInitialized || !mInTransaction) {
            return;
        }

        // Route based on backend type
        if (mSingleSPI) {
            // Direct passthrough to single-SPI hardware
            mSingleSPI->writeBit<BIT>(b);
        } else {
            // Multi-lane SPI doesn't support bit-level operations
            // This is typically only used for specific LED protocols
            FL_WARN("SPIDeviceProxy: writeBit() not supported for multi-lane SPI");
        }
    }

    /// Wait for SPI to be ready (NOP for buffered writes)
    static void wait() {
        // NOP for buffered multi-lane writes
        // Single-SPI instances handle their own waits
    }

    static void waitFully() {
        // NOP for buffered multi-lane writes
        // Single-SPI instances handle their own waits
    }

    /// Raw byte write value (static for use by adjustment classes)
    /// Mirrors NRF52SPIOutput::writeBytesValueRaw()
    static void writeBytesValueRaw(uint8_t value, int len) {
        // This is a static method used by adjustment classes
        // For the proxy, we can't easily support this in multi-lane mode
        // since we need the instance's buffer
        // This should only be called from within writeBytes<D>() which handles buffering
        FL_WARN("SPIDeviceProxy: writeBytesValueRaw() should not be called directly");
    }

    /// Finalize transmission - flush buffered multi-lane SPI writes
    /// Must be called after all pixel data is written
    /// Called by chipset controller at end of showPixels()
    void finalizeTransmission() {
        if (!mInitialized) {
            return;
        }

        // Only needed for multi-lane SPI (single-SPI writes directly)
        if (!mSingleSPI && !mWriteBuffer.empty()) {
            // Send buffered data to bus manager for multi-lane SPI transmission
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

#endif  // NRF52 variants
