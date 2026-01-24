/// @file spi_hw_2_samd21.cpp
/// @brief SAMD21 implementation of Dual-SPI using SERCOM + TCC/DMA
///
/// This file provides the SPIDualSAMD21 class and factory for SAMD21 platforms.
/// Uses SERCOM peripherals with TCC timer and DMA for dual-lane SPI output.
/// All class definition and implementation is contained in this single file.
///
/// Note: SAMD21 lacks native multi-lane SPI support, so this implementation
/// uses SERCOM for single-lane SPI combined with GPIO bit-banging or TCC
/// timer-based parallel output for the second lane.

#if defined(__SAMD21G18A__) || defined(__SAMD21J18A__) || \
    defined(__SAMD21E17A__) || defined(__SAMD21E18A__)

#include "platforms/shared/spi_hw_2.h"
#include "fl/warn.h"
#include "fl/stl/time.h"
#include "fl/numeric_limits.h"
#include <Arduino.h>  // ok include
#include <wiring_private.h>
#include "platforms/shared/spi_bus_manager.h"  // For DMABuffer, TransmitMode, SPIError

namespace fl {

// ============================================================================
// SPIDualSAMD21 Class Definition
// ============================================================================

/// SAMD21 hardware driver for Dual-SPI DMA transmission using SERCOM + TCC
///
/// Implements SpiHw2 interface for SAMD21 platforms using:
/// - SERCOM peripherals for primary SPI lane
/// - TCC timer with compare outputs for synchronized parallel lane
/// - DMA for non-blocking asynchronous transfers
/// - Configurable clock frequency up to 12 MHz
///
/// @note SAMD21 has no native multi-lane SPI, requires creative workarounds
/// @note Performance limited by 48 MHz CPU clock and GPIO toggle speed
class SPIDualSAMD21 : public SpiHw2 {
public:
    /// @brief Construct a new SPIDualSAMD21 controller
    /// @param bus_id Logical bus identifier (0-5 for SERCOM0-5)
    /// @param name Human-readable name for this controller
    explicit SPIDualSAMD21(int bus_id = -1, const char* name = "Unknown");

    /// @brief Destroy the controller and release all resources
    ~SPIDualSAMD21();

    /// @brief Initialize the SPI controller with specified configuration
    /// @param config Configuration including pins, clock speed, and bus number
    /// @return true if initialization successful, false on error
    /// @note Validates pin assignments and allocates SERCOM/TCC/DMA resources
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
    /// @return Bus ID (0-5 for SERCOM0-5)
    int getBusId() const override;

    /// @brief Get the human-readable name for this controller
    /// @return Controller name string
    const char* getName() const override;

private:
    /// @brief Release all allocated resources (SERCOM, TCC, DMA, buffers)
    void cleanup();

    int mBusId;  ///< Logical bus identifier (SERCOM number)
    const char* mName;

    // SERCOM resources
    Sercom* mSercom;

    // TCC resources for parallel output
    // Note: TCC (Timer/Counter for Control) can generate synchronized PWM outputs
    // We can use this for clock-synchronized parallel data output

    // DMA resources
    // Note: SAMD21 has 12 DMA channels
    // For now, we'll use a simplified polling implementation

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

    SPIDualSAMD21(const SPIDualSAMD21&) = delete;
    SPIDualSAMD21& operator=(const SPIDualSAMD21&) = delete;
};

// ============================================================================
// SPIDualSAMD21 Implementation
// ============================================================================

SPIDualSAMD21::SPIDualSAMD21(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mSercom(nullptr)
    , mTransactionActive(false)
    , mInitialized(false)
    , mClockPin(0)
    , mData0Pin(0)
    , mData1Pin(0)
    , mDMABuffer()
    , mBufferAcquired(false) {
}

SPIDualSAMD21::~SPIDualSAMD21() {
    cleanup();
}

bool SPIDualSAMD21::begin(const SpiHw2::Config& config) {
    if (mInitialized) {
        return true;  // Already initialized
    }

    // Validate bus_num against mBusId if driver has pre-assigned ID
    if (mBusId != -1 && config.bus_num != static_cast<uint8_t>(mBusId)) {
        FL_WARN("SPIDualSAMD21: Bus ID mismatch");
        return false;
    }

    // Validate pin assignments
    if (config.clock_pin < 0 || config.data0_pin < 0 || config.data1_pin < 0) {
        FL_WARN("SPIDualSAMD21: Invalid pin configuration");
        return false;
    }

    // Store pin configuration
    mClockPin = config.clock_pin;
    mData0Pin = config.data0_pin;
    mData1Pin = config.data1_pin;

    // SAMD21 has 6 SERCOM peripherals (0-5)
    // Map bus_num to SERCOM instance
    int sercom_num = (mBusId != -1) ? mBusId : config.bus_num;
    if (sercom_num < 0 || sercom_num > 5) {
        FL_WARN("SPIDualSAMD21: Invalid SERCOM number");
        return false;
    }

    // Get SERCOM instance
    mSercom = nullptr;
    switch (sercom_num) {
        case 0: mSercom = SERCOM0; break;
        case 1: mSercom = SERCOM1; break;
        case 2: mSercom = SERCOM2; break;
        case 3: mSercom = SERCOM3; break;
        case 4: mSercom = SERCOM4; break;
        case 5: mSercom = SERCOM5; break;
        default:
            FL_WARN("SPIDualSAMD21: Invalid SERCOM");
            return false;
    }

    if (mSercom == nullptr) {
        FL_WARN("SPIDualSAMD21: SERCOM not available");
        return false;
    }

    // Enable SERCOM clock via PM and GCLK
    // PM (Power Manager) enables the peripheral bus clock
    // GCLK provides the core clock source
    switch (sercom_num) {
        case 0:
            PM->APBCMASK.bit.SERCOM0_ = 1;
            GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_SERCOM0_CORE |
                                GCLK_CLKCTRL_GEN_GCLK0 |
                                GCLK_CLKCTRL_CLKEN;
            break;
        case 1:
            PM->APBCMASK.bit.SERCOM1_ = 1;
            GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_SERCOM1_CORE |
                                GCLK_CLKCTRL_GEN_GCLK0 |
                                GCLK_CLKCTRL_CLKEN;
            break;
        case 2:
            PM->APBCMASK.bit.SERCOM2_ = 1;
            GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_SERCOM2_CORE |
                                GCLK_CLKCTRL_GEN_GCLK0 |
                                GCLK_CLKCTRL_CLKEN;
            break;
        case 3:
            PM->APBCMASK.bit.SERCOM3_ = 1;
            GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_SERCOM3_CORE |
                                GCLK_CLKCTRL_GEN_GCLK0 |
                                GCLK_CLKCTRL_CLKEN;
            break;
        case 4:
            PM->APBCMASK.bit.SERCOM4_ = 1;
            GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_SERCOM4_CORE |
                                GCLK_CLKCTRL_GEN_GCLK0 |
                                GCLK_CLKCTRL_CLKEN;
            break;
        case 5:
            PM->APBCMASK.bit.SERCOM5_ = 1;
            GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_SERCOM5_CORE |
                                GCLK_CLKCTRL_GEN_GCLK0 |
                                GCLK_CLKCTRL_CLKEN;
            break;
    }

    // Wait for clock synchronization
    while (GCLK->STATUS.bit.SYNCBUSY);

    // Reset SERCOM to ensure clean state
    mSercom->SPI.CTRLA.bit.SWRST = 1;
    while (mSercom->SPI.CTRLA.bit.SWRST || mSercom->SPI.SYNCBUSY.bit.SWRST);

    // Configure pin multiplexing
    // Note: User must provide pins compatible with SERCOM PAD assignments
    // SAMD21 restrictions: SCK must be on PAD 1, MOSI on PAD 0 or 3
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
    uint32_t f_cpu = F_CPU;  // Typically 48 MHz for SAMD21
    uint32_t target_freq = config.clock_speed_hz;
    if (target_freq == 0) {
        target_freq = 8000000;  // Default 8 MHz (lower than SAMD51 due to slower CPU)
    }

    // SAMD21 max safe SPI clock is ~24 MHz (F_CPU/2)
    if (target_freq > 24000000) {
        target_freq = 24000000;
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

    FL_WARN("SPIDualSAMD21: Initialized on SERCOM" << sercom_num
            << " at " << (f_cpu / (2 * (baud_div + 1)) / 1000000.0) << " MHz"
            << " (polling mode, single-lane - true dual-lane TBD)");

    // Note: This is a single-lane implementation like SAMD51
    // True dual-lane requires:
    // - Approach 1: SERCOM + TCC Timer with event system synchronization
    // - Approach 2: Dual SERCOM with shared GCLK and DMA
    // - Approach 3: Hybrid SERCOM + GPIO bit-banging
    //
    // Hardware testing required before implementing more complex approaches

    mInitialized = true;
    return true;
}

void SPIDualSAMD21::end() {
    cleanup();
}

DMABuffer SPIDualSAMD21::acquireDMABuffer(size_t bytes_per_lane) {
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

bool SPIDualSAMD21::transmit(TransmitMode mode) {
    if (!mInitialized || !mBufferAcquired) {
        return false;
    }

    // Mode is a hint - platform may block
    (void)mode;

    if (!mDMABuffer.ok() || mDMABuffer.size() == 0) {
        return true;  // Nothing to transmit
    }

    // For single-lane SPI, we use SERCOM to send data on the primary lane (data0_pin)
    // Note: True dual-lane requires more complex implementation (TCC+SERCOM or dual-SERCOM)
    // This provides single-lane SPI as a starting point, matching SAMD51 approach

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

bool SPIDualSAMD21::waitComplete(uint32_t timeout_ms) {
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
            FL_WARN("SPIDualSAMD21: waitComplete timeout");
            return false;  // Timeout
        }
    }

    mTransactionActive = false;

    // Auto-release DMA buffer
    mBufferAcquired = false;
    mDMABuffer.reset();

    return true;
}

bool SPIDualSAMD21::isBusy() const {
    if (!mInitialized) {
        return false;
    }
    return mTransactionActive;
}

bool SPIDualSAMD21::isInitialized() const {
    return mInitialized;
}

int SPIDualSAMD21::getBusId() const {
    return mBusId;
}

const char* SPIDualSAMD21::getName() const {
    return mName;
}

void SPIDualSAMD21::cleanup() {
    if (mInitialized) {
        // Wait for any pending transmission
        if (mTransactionActive) {
            waitComplete();
        }

        // Release DMA buffer
        mDMABuffer.reset();
        mBufferAcquired = false;

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

/// IMPORTANT: SAMD21 Dual-SPI is NOT YET IMPLEMENTED!
/// The current implementation only supports single-lane SPI transmission.
/// True dual-lane requires one of these approaches:
///   - SERCOM + TCC Timer with event system synchronization
///   - Dual SERCOM with shared GCLK and DMA
///   - Hybrid SERCOM + GPIO bit-banging
///
/// Until true dual-lane is implemented, SAMD21 does not register any SpiHw2 instances.
/// This allows the bus manager to correctly fall back to single-lane SPI.
/// (No instances registered via SpiHw2::registerInstance())

}  // namespace fl

#endif  // __SAMD21G18A__ || __SAMD21J18A__ || ...
