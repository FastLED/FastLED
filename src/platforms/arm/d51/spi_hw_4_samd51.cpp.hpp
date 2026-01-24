/// @file spi_hw_4_samd51.cpp
/// @brief SAMD51 implementation of Quad-SPI using native QSPI peripheral
///
/// This file provides the SPIQuadSAMD51 class and factory for SAMD51 platforms.
/// Uses the native QSPI peripheral with DMA support for true quad-lane SPI.
/// All class definition and implementation is contained in this single file.
///
/// @warning IMPORTANT: QSPI peripheral is NOT recommended for LED driving!
///
/// The SAMD51 QSPI peripheral is designed for SPI flash memory access with a
/// command/address/data protocol (INSTRFRAME mode). This introduces significant
/// overhead for continuous LED data streaming:
///
/// **QSPI Limitations for LEDs:**
/// - INSTRFRAME protocol adds latency to each transfer
/// - Designed for memory command/address/data sequences, not continuous streams
/// - Limited status flags (ENABLE, CSSTATUS only in STATUS register)
/// - INTFLAG register provides better flags (DRE, TXC, INSTREND, RXC, ERROR)
/// - Polling-based implementation cannot achieve optimal throughput
/// - DMA support requires complex memory-mode configuration
///
/// **Recommended Alternative:**
/// For LED driving applications, use SERCOM SPI + DMA instead (see spi_hw_2_samd51.cpp).
/// Adafruit_NeoPXL8 library demonstrates this approach with excellent performance.
///
/// **This Implementation:**
/// Provides basic QSPI functionality for compatibility and testing purposes.
/// Uses INTFLAG register for proper synchronization (DRE, TXC, INSTREND flags).
/// Suitable for proof-of-concept but SERCOM SPI is preferred for production.

#if defined(__SAMD51G19A__) || defined(__SAMD51J19A__) || defined(__SAME51J19A__) || \
    defined(__SAMD51P19A__) || defined(__SAMD51P20A__)

#include "platforms/shared/spi_hw_4.h"
#include "fl/warn.h"
#include "fl/stl/time.h"
#include "fl/numeric_limits.h"
#include <Arduino.h>  // ok include
#include <wiring_private.h>
#include "platforms/shared/spi_bus_manager.h"  // For DMABuffer, TransmitMode, SPIError

namespace fl {

// ============================================================================
// SPIQuadSAMD51 Class Definition
// ============================================================================

/// SAMD51 hardware driver for Quad-SPI DMA transmission using native QSPI
///
/// Implements SpiHw4 interface for SAMD51 platforms using:
/// - Native QSPI peripheral for true 4-lane SPI
/// - DMA for non-blocking asynchronous transfers
/// - Configurable clock frequency up to 60 MHz
///
/// @note SAMD51 is unique among M0+/M4 platforms with native QSPI support
/// @note Data pins use dedicated QSPI pins (not configurable via PADs)
///
/// @warning NOT RECOMMENDED FOR LED DRIVING - See file header for details
///
/// **Why QSPI is Not Ideal for LEDs:**
/// - INSTRFRAME protocol designed for flash memory (command/address/data)
/// - Each transfer has overhead from INSTRFRAME setup
/// - STATUS register has limited flags (ENABLE, CSSTATUS only)
/// - INTFLAG provides better flags (DRE, TXC, INSTREND) but still suboptimal
/// - Current implementation is polling-based (blocking)
/// - DMA would require complex memory-mode configuration
///
/// **Recommended Alternative:**
/// Use SERCOM SPI peripherals instead (see spi_hw_2_samd51.cpp).
/// SERCOM provides:
/// - Continuous streaming without command overhead
/// - Better DMA integration for non-blocking transfers
/// - More suitable for LED timing requirements
/// - Proven approach (see Adafruit_NeoPXL8 library)
///
/// **This Class:**
/// Provided for completeness and testing. Works correctly but SERCOM
/// SPI is the better choice for production LED applications.
class SPIQuadSAMD51 : public SpiHw4 {
public:
    /// @brief Construct a new SPIQuadSAMD51 controller
    /// @param bus_id Logical bus identifier (always 0 - only one QSPI peripheral)
    /// @param name Human-readable name for this controller
    explicit SPIQuadSAMD51(int bus_id = -1, const char* name = "Unknown");

    /// @brief Destroy the controller and release all resources
    ~SPIQuadSAMD51();

    /// @brief Initialize the SPI controller with specified configuration
    /// @param config Configuration including pins, clock speed, and bus number
    /// @return true if initialization successful, false on error
    /// @note Validates pin assignments and allocates QSPI/DMA resources
    /// @note Auto-detects 1/2/4-lane mode based on active pins
    bool begin(const SpiHw4::Config& config) override;

    /// @brief Deinitialize the controller and release resources
    void end() override;

    /// @brief Acquire a DMA buffer for zero-copy transmission
    /// @param bytes_per_lane Number of bytes per lane to allocate
    /// @return DMABuffer containing buffer span or error
    /// @note Waits for previous transaction to complete if still active
    /// @note Buffer is automatically released after waitComplete()
    DMABuffer acquireDMABuffer(size_t bytes_per_lane) override;

    /// @brief Start non-blocking transmission using previously acquired DMA buffer
    /// @return true if transfer started successfully, false on error
    /// @note Requires acquireDMABuffer() to have been called first
    /// @note Returns immediately - use waitComplete() to block until done
    bool transmit(TransmitMode mode = TransmitMode::ASYNC) override;

    /// @brief Wait for current transmission to complete
    /// @param timeout_ms Maximum time to wait in milliseconds (fl::numeric_limits<uint32_t>::max() = infinite)
    /// @return true if transmission completed, false on timeout
    bool waitComplete(uint32_t timeout_ms = fl::numeric_limits<uint32_t>::max()) override;

    /// @brief Check if transmission is currently in progress
    /// @return true if busy, false if idle
    bool isBusy() const override;

    /// @brief Check if controller has been initialized
    /// @return true if initialized, false otherwise
    bool isInitialized() const override;

    /// @brief Get the bus identifier for this controller
    /// @return Bus ID (always 0 for QSPI)
    int getBusId() const override;

    /// @brief Get the human-readable name for this controller
    /// @return Controller name string
    const char* getName() const override;

private:
    /// @brief Release all allocated resources (QSPI, DMA, buffers)
    void cleanup();

    int mBusId;  ///< Logical bus identifier (always 0)
    const char* mName;

    // QSPI resources
    // Note: SAMD51 has one QSPI peripheral instance
    // Accessed via QSPI->... registers

    // DMA resources
    // Note: SAMD51 DMA implementation would go here
    // For now, we'll use a simplified polling implementation

    // State
    bool mTransactionActive;
    bool mInitialized;
    uint8_t mActiveLanes;

    // Configuration
    uint8_t mClockPin;
    uint8_t mData0Pin;
    uint8_t mData1Pin;
    uint8_t mData2Pin;
    uint8_t mData3Pin;

    // DMA buffer management
    fl::span<uint8_t> mDMABuffer;    // Allocated DMA buffer (interleaved format for quad-lane)
    size_t mMaxBytesPerLane;         // Max bytes per lane we've allocated for
    size_t mCurrentTotalSize;        // Current transmission size (bytes_per_lane * num_lanes)
    bool mBufferAcquired;

    SPIQuadSAMD51(const SPIQuadSAMD51&) = delete;
    SPIQuadSAMD51& operator=(const SPIQuadSAMD51&) = delete;
};

// ============================================================================
// SPIQuadSAMD51 Implementation
// ============================================================================

SPIQuadSAMD51::SPIQuadSAMD51(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mTransactionActive(false)
    , mInitialized(false)
    , mActiveLanes(1)
    , mClockPin(0)
    , mData0Pin(0)
    , mData1Pin(0)
    , mData2Pin(0)
    , mData3Pin(0)
    , mDMABuffer()
    , mMaxBytesPerLane(0)
    , mCurrentTotalSize(0)
    , mBufferAcquired(false) {
}

SPIQuadSAMD51::~SPIQuadSAMD51() {
    cleanup();
}

bool SPIQuadSAMD51::begin(const SpiHw4::Config& config) {
    if (mInitialized) {
        return true;  // Already initialized
    }

    // Validate bus_num against mBusId if driver has pre-assigned ID
    // SAMD51 only has one QSPI peripheral (bus 0)
    if (mBusId != -1 && config.bus_num != static_cast<uint8_t>(mBusId)) {
        FL_WARN("SPIQuadSAMD51: Bus ID mismatch");
        return false;
    }

    // Validate pin assignments - at least clock and D0 must be set
    if (config.clock_pin < 0 || config.data0_pin < 0) {
        FL_WARN("SPIQuadSAMD51: Invalid pin configuration (clock and D0 required)");
        return false;
    }

    // Store pin configuration
    mClockPin = config.clock_pin;
    mData0Pin = config.data0_pin;
    mData1Pin = config.data1_pin;
    mData2Pin = config.data2_pin;
    mData3Pin = config.data3_pin;

    // Count active data pins to determine SPI mode (1-4 lanes)
    mActiveLanes = 1;  // data0 always present
    if (config.data1_pin >= 0) mActiveLanes++;
    if (config.data2_pin >= 0) mActiveLanes++;
    if (config.data3_pin >= 0) mActiveLanes++;

    // Configure QSPI peripheral
    // Note: SAMD51 has dedicated QSPI pins (not configurable via SERCOM PADs):
    // - QSPI_SCK: Clock
    // - QSPI_DATA0: D0/MOSI
    // - QSPI_DATA1: D1/MISO
    // - QSPI_DATA2: D2/WP
    // - QSPI_DATA3: D3/HOLD

    // 1. Enable QSPI peripheral clocks
    MCLK->APBCMASK.bit.QSPI_ = 1;      // Enable QSPI APB clock
    MCLK->AHBMASK.bit.QSPI_ = 1;       // Enable QSPI AHB clock
    MCLK->AHBMASK.bit.QSPI_2X_ = 0;    // Disable 2x clock (use standard clock)

    // 2. Software reset QSPI to ensure clean state
    QSPI->CTRLA.bit.SWRST = 1;
    while (QSPI->CTRLA.bit.SWRST) {
        // Wait for reset to complete
    }

    // 3. Configure pin multiplexing for QSPI function (PIO_COM = peripheral H)
    // Note: Arduino provides pinPeripheral() function for pin mux
    pinPeripheral(mClockPin, PIO_COM);
    pinPeripheral(mData0Pin, PIO_COM);
    if (mActiveLanes >= 2) {
        pinPeripheral(mData1Pin, PIO_COM);
    }
    if (mActiveLanes >= 3) {
        pinPeripheral(mData2Pin, PIO_COM);
    }
    if (mActiveLanes >= 4) {
        pinPeripheral(mData3Pin, PIO_COM);
    }

    // 4. Calculate baud rate divider
    // QSPI baud rate = MCU_CLOCK / (2 * (BAUD + 1))
    // For requested clock_speed_hz, we need: BAUD = (MCU_CLOCK / (2 * clock_speed_hz)) - 1
    uint32_t mcu_clock = F_CPU;  // Arduino defines F_CPU as main clock frequency
    uint32_t target_clock = config.clock_speed_hz > 0 ? config.clock_speed_hz : 4000000;  // Default 4 MHz

    // Clamp to maximum safe clock (60 MHz for SAMD51)
    if (target_clock > 60000000) {
        target_clock = 60000000;
    }

    uint32_t baud_div = (mcu_clock / (2 * target_clock)) - 1;
    if (baud_div > 255) {
        baud_div = 255;  // BAUD field is 8 bits
    }

    QSPI->BAUD.reg = QSPI_BAUD_BAUD(baud_div);

    // 5. Configure QSPI control registers
    // Use SPI mode (not memory mode) for driving LEDs
    // CTRLB configuration:
    // - MODE: 1 = SPI mode (0 = memory mode for XIP)
    // - DATALEN: 0 = 8-bit transfers
    // - CSMODE: 0 = NORELOAD (CS stays low during transfer)
    uint32_t ctrlb_value = QSPI_CTRLB_DATALEN(0) |  // 8-bit data length
                           QSPI_CTRLB_CSMODE(0);     // CS stays low during transfer

    // Set SPI mode bit (bit 0 of MODE field)
    ctrlb_value |= (1u << QSPI_CTRLB_MODE_Pos);  // MODE = 1 (SPI mode)

    QSPI->CTRLB.reg = ctrlb_value;

    // Synchronize CTRLB write
    while (QSPI->STATUS.bit.ENABLE) {
        // Wait for QSPI to be ready
    }

    // 6. Enable QSPI peripheral
    QSPI->CTRLA.bit.ENABLE = 1;

    // Wait for enable to take effect
    while (!QSPI->STATUS.bit.ENABLE) {
        // Wait for QSPI to enable
    }

    // Note: DMA setup will be done in transmit() on first transfer
    // This allows for lazy initialization and avoids holding DMA channels when not in use

    mInitialized = true;
    mTransactionActive = false;

    return true;
}

void SPIQuadSAMD51::end() {
    cleanup();
}

DMABuffer SPIQuadSAMD51::acquireDMABuffer(size_t bytes_per_lane) {
    if (!mInitialized) {
        return SPIError::NOT_INITIALIZED;
    }

    // Auto-wait if previous transmission still active
    if (mTransactionActive) {
        if (!waitComplete()) {
            return SPIError::BUSY;
        }
    }

    // For quad-lane SPI: total size = bytes_per_lane × 4 (interleaved)
    constexpr size_t num_lanes = 4;
    const size_t total_size = bytes_per_lane * num_lanes;

    // Validate size against platform max (256KB practical limit for embedded)
    constexpr size_t MAX_SIZE = 256 * 1024;
    if (total_size > MAX_SIZE) {
        return SPIError::BUFFER_TOO_LARGE;
    }

    // Reallocate buffer only if we need more capacity
    if (bytes_per_lane > mMaxBytesPerLane) {
        if (!mDMABuffer.empty()) {
            free(mDMABuffer.data());
            mDMABuffer = fl::span<uint8_t>();
        }

        // Allocate DMA-capable memory (SAMD51 uses regular malloc)
        uint8_t* ptr = static_cast<uint8_t*>(malloc(total_size));
        if (!ptr) {
            return SPIError::ALLOCATION_FAILED;
        }

        mDMABuffer = fl::span<uint8_t>(ptr, total_size);
        mMaxBytesPerLane = bytes_per_lane;
    }

    mBufferAcquired = true;
    mCurrentTotalSize = total_size;

    // Return span of current size (not max allocated size)
    return fl::span<uint8_t>(mDMABuffer.data(), total_size);
}

bool SPIQuadSAMD51::transmit(TransmitMode mode) {
    if (!mInitialized || !mBufferAcquired) {
        return false;
    }

    // Mode is a hint - platform may block
    (void)mode;

    if (mCurrentTotalSize == 0) {
        return true;  // Nothing to transmit
    }

    // Implementation Note:
    // SAMD51 QSPI peripheral is designed for SPI flash memory access with
    // command/address/data protocol. For continuous LED data streaming,
    // SERCOM SPI + DMA is the preferred approach (see Adafruit_NeoPXL8).
    //
    // This implementation provides a basic polling-based transfer for testing.
    // A production implementation should use SERCOM peripherals instead.
    //
    // For now, we use QSPI INSTRFRAME mode to send data, but this is not
    // optimal for high-speed LED driving due to command overhead.

    mTransactionActive = true;

    // Configure QSPI for data transmission using INSTRFRAME
    // INSTRFRAME controls how data is sent:
    // - TFRTYPE: Transfer type (read=0, write=2, read memory=1)
    // - WIDTH: Single/Dual/Quad (0=single, 1=dual, 2=quad)
    // - INSTREN: Enable instruction phase
    // - ADDREN: Enable address phase
    // - DATAEN: Enable data phase
    // - DUMMYLEN: Number of dummy cycles

    // For LED data streaming, we want:
    // - Write transfer (TFRTYPE=2)
    // - Width based on mActiveLanes (0=1-lane, 1=2-lane, 2=4-lane)
    // - Data phase enabled (DATAEN=1)
    // - No instruction, address, or dummy phases

    uint32_t width_mode = 0;  // Default: single lane
    if (mActiveLanes == 2) {
        width_mode = 1;  // Dual lane
    } else if (mActiveLanes >= 4) {
        width_mode = 2;  // Quad lane
    }

    // Build INSTRFRAME register value
    // TFRTYPE (bits 17-16): 2 = write
    // WIDTH (bits 13-12): 0/1/2 for 1/2/4 lanes
    // DATAEN (bit 9): 1 = data phase enabled
    uint32_t instrframe_value = (2u << 16) |         // TFRTYPE = write
                                (width_mode << 12) |  // WIDTH based on lanes
                                (1u << 9);            // DATAEN = 1

    // Write INSTRFRAME to configure transfer
    QSPI->INSTRFRAME.reg = instrframe_value;

    // Synchronize: datasheet recommends reading INSTRFRAME once
    (void)QSPI->INSTRFRAME.reg;

    // Transmit data bytes using TXDATA register with proper flag checking
    // INTFLAG register provides the following status flags:
    // - INTFLAG.bit.DRE: Data Register Empty (bit 1) - TX buffer ready for next byte
    // - INTFLAG.bit.TXC: Transmission Complete (bit 2) - transfer finished
    // - INTFLAG.bit.ERROR: Overrun Error (bit 3) - data lost due to buffer overrun
    // - INTFLAG.bit.INSTREND: Instruction End (bit 10) - INSTRFRAME sequence complete
    //
    // Note: This is polling-based (blocking) for simplicity.
    // A DMA-based implementation would be more efficient.
    const uint8_t* data_ptr = mDMABuffer.data();
    size_t remaining = mCurrentTotalSize;

    while (remaining > 0) {
        // Wait for DRE (Data Register Empty) flag before writing next byte
        // This ensures the TXDATA register is ready to accept new data
        while (!QSPI->INTFLAG.bit.DRE) {
            // Check for error condition
            if (QSPI->INTFLAG.bit.ERROR) {
                FL_WARN("QSPI ERROR flag set during transmission");
                // Clear ERROR flag
                QSPI->INTFLAG.reg = QSPI_INTFLAG_ERROR;
                mTransactionActive = false;
                return false;
            }
        }

        // Write next byte to TXDATA
        QSPI->TXDATA.reg = *data_ptr++;
        remaining--;
    }

    // Wait for transmission to complete using INSTREND flag
    // INSTREND indicates the entire INSTRFRAME sequence (including all data) is done
    while (!(QSPI->INTFLAG.bit.INSTREND)) {
        // Poll until instruction/transfer ends
    }

    // Clear the INSTREND flag by writing 1 to it
    QSPI->INTFLAG.reg = QSPI_INTFLAG_INSTREND;

    mTransactionActive = false;
    return true;
}

bool SPIQuadSAMD51::waitComplete(uint32_t timeout_ms) {
    if (!mTransactionActive) {
        return true;  // Nothing to wait for
    }

    // Implementation Note:
    // Current transmit() implementation is synchronous (polling-based),
    // waiting for QSPI INSTREND flag before returning.
    // Therefore, by the time waitComplete() is called, the transaction
    // is already complete.
    //
    // This timeout logic is provided for API consistency and future-proofing
    // in case async DMA implementation is added later.

    fl::u32 start_time = fl::millis();

    // Poll QSPI status to verify transmission actually completed
    // Check INSTREND (Instruction End) flag in INTFLAG register
    while (!QSPI->INTFLAG.bit.INSTREND) {
        if ((fl::millis() - start_time) >= timeout_ms) {
            FL_WARN("SPIQuadSAMD51: waitComplete timeout");
            return false;  // Timeout
        }
        // Check for error condition
        if (QSPI->INTFLAG.bit.ERROR) {
            FL_WARN("SPIQuadSAMD51: QSPI error during waitComplete");
            QSPI->INTFLAG.reg = QSPI_INTFLAG_ERROR;  // Clear error
            return false;
        }
    }

    mTransactionActive = false;

    // AUTO-RELEASE DMA buffer
    mBufferAcquired = false;
    mCurrentTotalSize = 0;

    return true;
}

bool SPIQuadSAMD51::isBusy() const {
    if (!mInitialized) {
        return false;
    }
    return mTransactionActive;
}

bool SPIQuadSAMD51::isInitialized() const {
    return mInitialized;
}

int SPIQuadSAMD51::getBusId() const {
    return mBusId;
}

const char* SPIQuadSAMD51::getName() const {
    return mName;
}

void SPIQuadSAMD51::cleanup() {
    if (mInitialized) {
        // Wait for any pending transmission
        if (mTransactionActive) {
            waitComplete();
        }

        // Free DMA buffer
        if (!mDMABuffer.empty()) {
            free(mDMABuffer.data());
            mDMABuffer = fl::span<uint8_t>();
            mMaxBytesPerLane = 0;
            mCurrentTotalSize = 0;
            mBufferAcquired = false;
        }

        // Disable QSPI peripheral
        QSPI->CTRLA.bit.ENABLE = 0;
        while (QSPI->STATUS.bit.ENABLE) {
            // Wait for QSPI to disable
        }

        // Release DMA resources (placeholder for future DMA implementation)
        // Note: DMA is not currently implemented (polling-based transfers only)
        // When async DMA is added, cleanup steps would include:
        // 1. Disable DMA channel(s) via DMAC registers
        // 2. Free DMA descriptor memory if dynamically allocated
        // 3. Release channel ID(s) back to allocator

        // Disable peripheral clocks
        MCLK->APBCMASK.bit.QSPI_ = 0;
        MCLK->AHBMASK.bit.QSPI_ = 0;

        mInitialized = false;
    }
}

// ============================================================================
// Static Registration - New Polymorphic Pattern
// ============================================================================

namespace platform {

/// @brief Initialize SAMD51 SpiHw4 instances
///
/// This function is called lazily by SpiHw4::getAll() on first access.
/// It replaces the old FL_INIT-based static initialization.
void initSpiHw4Instances() {
    // SAMD51 has one QSPI peripheral
    static auto controller0 = fl::make_shared<SPIQuadSAMD51>(0, "QSPI");
    SpiHw4::registerInstance(controller0);
}

}  // namespace platform

}  // namespace fl

#endif  // __SAMD51G19A__ || __SAMD51J19A__ || ...
