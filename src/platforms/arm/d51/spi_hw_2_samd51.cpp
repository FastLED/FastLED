/// @file spi_hw_2_samd51.cpp
/// @brief SAMD51 implementation of Dual-SPI using SERCOM
///
/// This file provides the SPIDualSAMD51 class and factory for SAMD51 platforms.
/// Uses SERCOM peripherals with DMA support for true dual-lane SPI.
/// All class definition and implementation is contained in this single file.
///
/// @note RECOMMENDED APPROACH for LED driving on SAMD51
///
/// **Why SERCOM is Better Than QSPI for LEDs:**
///
/// SERCOM peripherals are the ideal choice for LED data streaming because:
/// - **Continuous Streaming**: No command/address overhead between data bytes
/// - **Clean Protocol**: Standard SPI without flash memory command framing
/// - **Better DMA Integration**: Designed for continuous buffer transmission
/// - **Flexible Pin Assignment**: SERCOM PAD configuration allows pin choices
/// - **Multiple Instances**: Up to 8 SERCOM peripherals available (SAMD51)
/// - **Proven Performance**: Used by Adafruit_NeoPXL8 library with excellent results
///
/// **QSPI Limitations (see spi_hw_4_samd51.cpp):**
/// - QSPI designed for flash memory (INSTRFRAME protocol has overhead)
/// - Each transfer requires INSTRFRAME setup (adds latency)
/// - STATUS register has limited flags (ENABLE, CSSTATUS only)
/// - DMA requires complex memory-mode configuration
/// - Single QSPI peripheral vs 8 SERCOM peripherals
///
/// **Current Implementation:**
/// - Polling-based transmission (blocking) for initial testing
/// - Single-lane operation (true dual-lane requires dual-SERCOM or bit-banging)
/// - DMA infrastructure prepared but not yet activated
/// - Ready for enhancement with DMA for non-blocking transfers
///
/// **Future Enhancements:**
/// - Activate DMA for true asynchronous operation
/// - Implement dual-SERCOM synchronized mode for true dual-lane
/// - Add interrupt-driven completion notification

#if defined(__SAMD51G19A__) || defined(__SAMD51J19A__) || defined(__SAME51J19A__) || \
    defined(__SAMD51P19A__) || defined(__SAMD51P20A__)

#include "platforms/shared/spi_hw_2.h"
#include "fl/warn.h"
#include "fl/stl/time.h"
#include "fl/numeric_limits.h"
#include <Arduino.h>  // ok include
#include <wiring_private.h>
#include "platforms/shared/spi_bus_manager.h"  // For DMABuffer, TransmitMode, SPIError

namespace fl {

// ============================================================================
// SPIDualSAMD51 Class Definition
// ============================================================================

/// SAMD51 hardware driver for Dual-SPI DMA transmission using SERCOM
///
/// Implements SpiHw2 interface for SAMD51 platforms using:
/// - SERCOM peripherals for SPI communication
/// - DMA for non-blocking asynchronous transfers
/// - Configurable clock frequency up to 24 MHz
///
/// @note Each instance uses one SERCOM peripheral and one DMA channel
/// @note Data pins are configured via SERCOM PAD assignments
class SPIDualSAMD51 : public SpiHw2 {
public:
    /// @brief Construct a new SPIDualSAMD51 controller
    /// @param bus_id Logical bus identifier (0-7 for SERCOM0-7)
    /// @param name Human-readable name for this controller
    explicit SPIDualSAMD51(int bus_id = -1, const char* name = "Unknown");

    /// @brief Destroy the controller and release all resources
    ~SPIDualSAMD51();

    /// @brief Initialize the SPI controller with specified configuration
    /// @param config Configuration including pins, clock speed, and bus number
    /// @return true if initialization successful, false on error
    /// @note Validates pin assignments and allocates SERCOM/DMA resources
    bool begin(const SpiHw2::Config& config) override;

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
    /// @return Bus ID (0-7 for SERCOM0-7)
    int getBusId() const override;

    /// @brief Get the human-readable name for this controller
    /// @return Controller name string
    const char* getName() const override;

private:
    /// @brief Release all allocated resources (SERCOM, DMA, buffers)
    void cleanup();

    int mBusId;  ///< Logical bus identifier (SERCOM number)
    const char* mName;

    // SERCOM resources
    Sercom* mSercom;

    // DMA resources (legacy, for future DMA implementation)
    int mDmaChannel;                       ///< Allocated DMA channel (-1 if not allocated)
    DmacDescriptor* mDmaDescriptor;        ///< Pointer to writeback descriptor
    volatile bool mDmaComplete;            ///< Flag set by DMA interrupt
    static DmacDescriptor sDescriptors[DMAC_CH_NUM] __attribute__((aligned(16)));    ///< DMA descriptors in SRAM
    static DmacDescriptor sWritebackDescriptors[DMAC_CH_NUM] __attribute__((aligned(16))); ///< DMA writeback descriptors

    // State
    bool mTransactionActive;
    bool mInitialized;

    // Configuration
    uint8_t mClockPin;
    uint8_t mData0Pin;
    uint8_t mData1Pin;

    // DMA buffer management
    DMABuffer mDMABuffer;            // Current DMA buffer
    bool mBufferAcquired;

    SPIDualSAMD51(const SPIDualSAMD51&) = delete;
    SPIDualSAMD51& operator=(const SPIDualSAMD51&) = delete;
};

// ============================================================================
// SPIDualSAMD51 Implementation
// ============================================================================

SPIDualSAMD51::SPIDualSAMD51(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mSercom(nullptr)
    , mDmaChannel(-1)
    , mDmaDescriptor(nullptr)
    , mDmaComplete(false)
    , mTransactionActive(false)
    , mInitialized(false)
    , mClockPin(0)
    , mData0Pin(0)
    , mData1Pin(0)
    , mDMABuffer()
    , mBufferAcquired(false) {
}

SPIDualSAMD51::~SPIDualSAMD51() {
    cleanup();
}

bool SPIDualSAMD51::begin(const SpiHw2::Config& config) {
    if (mInitialized) {
        return true;  // Already initialized
    }

    // Validate bus_num against mBusId if driver has pre-assigned ID
    if (mBusId != -1 && config.bus_num != static_cast<uint8_t>(mBusId)) {
        FL_WARN("SPIDualSAMD51: Bus ID mismatch");
        return false;
    }

    // Validate pin assignments
    if (config.clock_pin < 0 || config.data0_pin < 0 || config.data1_pin < 0) {
        FL_WARN("SPIDualSAMD51: Invalid pin configuration");
        return false;
    }

    // Store pin configuration
    mClockPin = config.clock_pin;
    mData0Pin = config.data0_pin;
    mData1Pin = config.data1_pin;

    // SAMD51 has 8 SERCOM peripherals (0-7)
    // Map bus_num to SERCOM instance
    int sercom_num = (mBusId != -1) ? mBusId : config.bus_num;
    if (sercom_num < 0 || sercom_num > 7) {
        FL_WARN("SPIDualSAMD51: Invalid SERCOM number");
        return false;
    }

    // Get SERCOM instance
    mSercom = nullptr;
    switch (sercom_num) {
        case 0: mSercom = SERCOM0; break;
        case 1: mSercom = SERCOM1; break;
        case 2: mSercom = SERCOM2; break;
        case 3: mSercom = SERCOM3; break;
#if defined(SERCOM4)
        case 4: mSercom = SERCOM4; break;
#endif
#if defined(SERCOM5)
        case 5: mSercom = SERCOM5; break;
#endif
#if defined(SERCOM6)
        case 6: mSercom = SERCOM6; break;
#endif
#if defined(SERCOM7)
        case 7: mSercom = SERCOM7; break;
#endif
        default:
            FL_WARN("SPIDualSAMD51: Invalid SERCOM");
            return false;
    }

    if (mSercom == nullptr) {
        FL_WARN("SPIDualSAMD51: SERCOM not available");
        return false;
    }

    // Enable SERCOM clock via MCLK and GCLK
    // MCLK enables the peripheral bus clock
    // GCLK provides the core clock source
    switch (sercom_num) {
        case 0:
            MCLK->APBAMASK.bit.SERCOM0_ = 1;
            GCLK->PCHCTRL[SERCOM0_GCLK_ID_CORE].reg = GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN;
            break;
        case 1:
            MCLK->APBAMASK.bit.SERCOM1_ = 1;
            GCLK->PCHCTRL[SERCOM1_GCLK_ID_CORE].reg = GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN;
            break;
        case 2:
            MCLK->APBBMASK.bit.SERCOM2_ = 1;
            GCLK->PCHCTRL[SERCOM2_GCLK_ID_CORE].reg = GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN;
            break;
        case 3:
            MCLK->APBBMASK.bit.SERCOM3_ = 1;
            GCLK->PCHCTRL[SERCOM3_GCLK_ID_CORE].reg = GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN;
            break;
#if defined(SERCOM4)
        case 4:
            MCLK->APBDMASK.bit.SERCOM4_ = 1;
            GCLK->PCHCTRL[SERCOM4_GCLK_ID_CORE].reg = GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN;
            break;
#endif
#if defined(SERCOM5)
        case 5:
            MCLK->APBDMASK.bit.SERCOM5_ = 1;
            GCLK->PCHCTRL[SERCOM5_GCLK_ID_CORE].reg = GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN;
            break;
#endif
#if defined(SERCOM6)
        case 6:
            MCLK->APBDMASK.bit.SERCOM6_ = 1;
            GCLK->PCHCTRL[SERCOM6_GCLK_ID_CORE].reg = GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN;
            break;
#endif
#if defined(SERCOM7)
        case 7:
            MCLK->APBDMASK.bit.SERCOM7_ = 1;
            GCLK->PCHCTRL[SERCOM7_GCLK_ID_CORE].reg = GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN;
            break;
#endif
    }

    // Wait for clock synchronization
    while (GCLK->SYNCBUSY.reg & GCLK_SYNCBUSY_GENCTRL_GCLK0);

    // Reset SERCOM to ensure clean state
    mSercom->SPI.CTRLA.bit.SWRST = 1;
    while (mSercom->SPI.CTRLA.bit.SWRST || mSercom->SPI.SYNCBUSY.bit.SWRST);

    // Configure pin multiplexing
    // Note: User must provide pins compatible with SERCOM PAD assignments
    // SAMD51 restrictions: SCK must be on PAD 1, MOSI on PAD 0 or 3
    pinPeripheral(mClockPin, PIO_SERCOM_ALT);
    pinPeripheral(mData0Pin, PIO_SERCOM_ALT);
    pinPeripheral(mData1Pin, PIO_SERCOM_ALT);

    // Configure SERCOM for SPI Master mode
    // CTRLA register configuration:
    // - MODE = 0x3 (SPI Master)
    // - DOPO = 0x0 (Data Out on PAD[0], SCK on PAD[1])
    // - DIPO = 0x2 (Data In on PAD[2] - not used, but set for completeness)
    // - FORM = 0x0 (SPI Frame)
    // - CPHA = 0 (Sample on leading edge)
    // - CPOL = 0 (Clock idle low)
    // - DORD = 0 (MSB first)
    uint32_t ctrla_value = SERCOM_SPI_CTRLA_MODE(0x3) |      // SPI Master
                           SERCOM_SPI_CTRLA_DOPO(0x0) |      // PAD[0]=MOSI, PAD[1]=SCK
                           SERCOM_SPI_CTRLA_DIPO(0x2) |      // PAD[2]=MISO (unused)
                           SERCOM_SPI_CTRLA_FORM(0x0);       // SPI Frame

    mSercom->SPI.CTRLA.reg = ctrla_value;

    // Wait for synchronization
    while (mSercom->SPI.SYNCBUSY.bit.ENABLE);

    // Calculate baud rate
    // Baud = F_CPU / (2 * (BAUD + 1))
    // BAUD = (F_CPU / (2 * target_freq)) - 1
    uint32_t f_cpu = F_CPU;  // Typically 120 MHz for SAMD51
    uint32_t target_freq = config.clock_speed_hz;
    if (target_freq == 0) {
        target_freq = 10000000;  // Default 10 MHz
    }

    uint32_t baud_div = (f_cpu / (2 * target_freq)) - 1;
    if (baud_div > 255) {
        baud_div = 255;  // Clamp to maximum
    }

    mSercom->SPI.BAUD.reg = static_cast<uint8_t>(baud_div);

    // Configure CTRLB register
    // - CHSIZE = 0 (8-bit data)
    // - RXEN = 0 (Receiver disabled for transmit-only)
    mSercom->SPI.CTRLB.reg = SERCOM_SPI_CTRLB_CHSIZE(0);

    // Wait for synchronization
    while (mSercom->SPI.SYNCBUSY.bit.CTRLB);

    // Enable SERCOM
    mSercom->SPI.CTRLA.bit.ENABLE = 1;
    while (mSercom->SPI.SYNCBUSY.bit.ENABLE);

    // Initialize DMA for asynchronous transfers
    // Note: For a complete DMA implementation, we would:
    // 1. Allocate a DMA channel
    // 2. Configure the channel for SERCOM TX trigger
    // 3. Set up transfer descriptors
    // 4. Enable transfer complete interrupts
    //
    // This requires:
    // - Access to DMAC (DMA Controller) registers
    // - DMA descriptor memory (must be in SRAM)
    // - Interrupt handler registration
    //
    // For now, we'll use polling mode and document the DMA requirements
    mDmaChannel = -1;  // Mark as not using DMA

    FL_WARN("SPIDualSAMD51: Initialized on SERCOM" << sercom_num
            << " at " << (f_cpu / (2 * (baud_div + 1)) / 1000000.0) << " MHz"
            << " (polling mode - DMA can be added later)");

    mInitialized = true;
    return true;
}

void SPIDualSAMD51::end() {
    cleanup();
}

DMABuffer SPIDualSAMD51::acquireDMABuffer(size_t bytes_per_lane) {
    if (!mInitialized) {
        return DMABuffer(SPIError::NOT_INITIALIZED);
    }

    // Auto-wait if previous transmission still active
    if (mTransactionActive) {
        if (!waitComplete()) {
            return DMABuffer(SPIError::BUSY);
        }
    }

    // For dual-lane SPI: total size = bytes_per_lane × 2 (interleaved)
    constexpr size_t num_lanes = 2;
    const size_t total_size = bytes_per_lane * num_lanes;

    // Validate size against platform max (256KB practical limit for embedded)
    constexpr size_t MAX_SIZE = 256 * 1024;
    if (total_size > MAX_SIZE) {
        return DMABuffer(SPIError::BUFFER_TOO_LARGE);
    }

    // Allocate new DMABuffer - it will manage its own memory
    mDMABuffer = DMABuffer(total_size);
    if (!mDMABuffer.ok()) {
        return DMABuffer(SPIError::ALLOCATION_FAILED);
    }

    mBufferAcquired = true;

    // Return the DMABuffer
    return mDMABuffer;
}

bool SPIDualSAMD51::transmit(TransmitMode mode) {
    if (!mInitialized || !mBufferAcquired) {
        return false;
    }

    // Mode is a hint - platform may block
    (void)mode;

    if (!mDMABuffer.ok() || mDMABuffer.size() == 0) {
        return true;  // Nothing to transmit
    }

    // For dual-lane SPI, we use SERCOM to send data on the primary lane (data0_pin)
    // Note: True dual-lane requires two separate SERCOM instances running in parallel
    // or a more complex bit-banging approach. This implementation provides
    // single-lane SPI as a starting point.
    //
    // TODO: Implement true dual-lane by either:
    // 1. Using two SERCOM instances with synchronized DMA, or
    // 2. Implementing bit-banging with precise timing
    //
    // For now, we use polling-based transmission on a single lane

    // Get the buffer span
    fl::span<uint8_t> buffer_span = mDMABuffer.data();

    mTransactionActive = true;

    // Transmit each byte via SERCOM SPI DATA register
    for (size_t i = 0; i < buffer_span.size(); ++i) {
        // Wait for Data Register Empty flag
        while (!mSercom->SPI.INTFLAG.bit.DRE);

        // Write byte to DATA register
        mSercom->SPI.DATA.reg = buffer_span[i];
    }

    // Wait for Transmit Complete flag
    while (!mSercom->SPI.INTFLAG.bit.TXC);

    mTransactionActive = false;
    return true;
}

bool SPIDualSAMD51::waitComplete(uint32_t timeout_ms) {
    if (!mTransactionActive) {
        return true;  // Nothing to wait for
    }

    // Implementation Note:
    // Current transmit() implementation is synchronous (polling-based),
    // waiting for SERCOM TXC (Transmit Complete) flag before returning.
    // Therefore, by the time waitComplete() is called, the transaction
    // is already complete.
    //
    // This timeout logic is provided for API consistency and future-proofing
    // in case async DMA implementation is added later.

    fl::u32 start_time = fl::millis();

    // Poll SERCOM status to verify transmission actually completed
    // Check TXC (Transmit Complete) flag in INTFLAG register
    while (mSercom && !mSercom->SPI.INTFLAG.bit.TXC) {
        if ((fl::millis() - start_time) >= timeout_ms) {
            FL_WARN("SPIDualSAMD51: waitComplete timeout");
            return false;  // Timeout
        }
    }

    mTransactionActive = false;

    // Auto-release DMA buffer
    mBufferAcquired = false;
    mDMABuffer.reset();

    return true;
}

bool SPIDualSAMD51::isBusy() const {
    if (!mInitialized) {
        return false;
    }
    return mTransactionActive;
}

bool SPIDualSAMD51::isInitialized() const {
    return mInitialized;
}

int SPIDualSAMD51::getBusId() const {
    return mBusId;
}

const char* SPIDualSAMD51::getName() const {
    return mName;
}

void SPIDualSAMD51::cleanup() {
    if (mInitialized) {
        // Wait for any pending transmission
        if (mTransactionActive) {
            waitComplete();
        }

        // Release DMA buffer
        mDMABuffer.reset();
        mBufferAcquired = false;

        // Release DMA resources (legacy, for future DMA implementation)
        // Note: DMA is not currently implemented (mDmaChannel is always -1)
        // These steps are placeholders for when async DMA is added:
        // 1. Disable DMA channel via DMAC registers
        // 2. Free DMA descriptor memory if dynamically allocated
        // 3. Release channel ID back to allocator
        if (mDmaChannel != -1) {
            // Future DMA cleanup would go here
            mDmaChannel = -1;
            mDmaDescriptor = nullptr;
        }

        if (mSercom != nullptr) {
            // Disable SERCOM
            mSercom->SPI.CTRLA.bit.ENABLE = 0;
            while (mSercom->SPI.SYNCBUSY.bit.ENABLE);

            // Reset SERCOM
            mSercom->SPI.CTRLA.bit.SWRST = 1;
            while (mSercom->SPI.CTRLA.bit.SWRST || mSercom->SPI.SYNCBUSY.bit.SWRST);

            // Note: We don't disable peripheral clocks as other code may use them
            mSercom = nullptr;
        }

        mInitialized = false;
    }
}

// ============================================================================
// Factory Implementation
// ============================================================================

/// SAMD51 factory override - returns empty (Dual-SPI not yet supported)
///
/// IMPORTANT: SAMD51 Dual-SPI is NOT YET IMPLEMENTED!
/// The current implementation only supports single-lane SPI transmission.
/// True dual-lane requires one of these approaches:
///   - Two SERCOM instances with synchronized DMA and shared GCLK
///   - Hybrid SERCOM + GPIO bit-banging with precise timing
///   - Hardware-synchronized dual-SERCOM with event system
///
/// Until true dual-lane is implemented, SAMD51 does not register any SpiHw2 instances.
/// This allows the bus manager to correctly fall back to single-lane SPI.
/// (No instances registered via SpiHw2::registerInstance())

}  // namespace fl

#endif  // __SAMD51G19A__ || __SAMD51J19A__ || ...
