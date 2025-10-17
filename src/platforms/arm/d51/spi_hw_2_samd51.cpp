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
#include <Arduino.h>  // ok include
#include <wiring_private.h>

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

    /// @brief Start non-blocking transmission of data buffer
    /// @param buffer Data to transmit (reorganized into dual-lane format internally)
    /// @return true if transfer started successfully, false on error
    /// @note Waits for previous transaction to complete if still active
    /// @note Returns immediately - use waitComplete() to block until done
    bool transmitAsync(fl::span<const uint8_t> buffer) override;

    /// @brief Wait for current transmission to complete
    /// @param timeout_ms Maximum time to wait in milliseconds (UINT32_MAX = infinite)
    /// @return true if transmission completed, false on timeout
    bool waitComplete(uint32_t timeout_ms = UINT32_MAX) override;

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

    /// @brief Allocate or resize internal DMA buffer
    /// @param required_size Size needed in bytes
    /// @return true if buffer allocated successfully
    bool allocateDMABuffer(size_t required_size);

    int mBusId;  ///< Logical bus identifier (SERCOM number)
    const char* mName;

    // SERCOM resources
    Sercom* mSercom;

    // DMA resources
    int mDmaChannel;                       ///< Allocated DMA channel (-1 if not allocated)
    DmacDescriptor* mDmaDescriptor;        ///< Pointer to writeback descriptor
    uint8_t* mDmaBuffer;                   ///< DMA buffer for data
    size_t mDmaBufferSize;                 ///< Size of allocated DMA buffer
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
    , mDmaBuffer(nullptr)
    , mDmaBufferSize(0)
    , mDmaComplete(false)
    , mTransactionActive(false)
    , mInitialized(false)
    , mClockPin(0)
    , mData0Pin(0)
    , mData1Pin(0) {
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

bool SPIDualSAMD51::allocateDMABuffer(size_t required_size) {
    // TODO: Implement DMA buffer allocation
    return false;
}

bool SPIDualSAMD51::transmitAsync(fl::span<const uint8_t> buffer) {
    if (!mInitialized) {
        return false;
    }

    // Wait for previous transaction if still active
    if (mTransactionActive) {
        waitComplete();
    }

    if (buffer.empty()) {
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

    mTransactionActive = true;

    // Transmit each byte via SERCOM SPI DATA register
    for (size_t i = 0; i < buffer.size(); ++i) {
        // Wait for Data Register Empty flag
        while (!mSercom->SPI.INTFLAG.bit.DRE);

        // Write byte to DATA register
        mSercom->SPI.DATA.reg = buffer[i];
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

    // TODO: Implement completion waiting
    // Poll DMA status or use interrupts

    mTransactionActive = false;
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

        // Release DMA resources
        if (mDmaChannel != -1) {
            // TODO: Disable DMA channel
            // TODO: Free DMA descriptor
            mDmaChannel = -1;
            mDmaDescriptor = nullptr;
        }

        // Free DMA buffer
        if (mDmaBuffer != nullptr) {
            delete[] mDmaBuffer;
            mDmaBuffer = nullptr;
            mDmaBufferSize = 0;
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

/// SAMD51 factory override - returns available SPI bus instances
/// Strong definition overrides weak default
fl::vector<SpiHw2*> SpiHw2::createInstances() {
    fl::vector<SpiHw2*> controllers;

    // SAMD51 has 8 SERCOM peripherals
    // Create instances for commonly used SERCOM units
    // Note: Some may be reserved for standard SPI/I2C/UART in Arduino

    static SPIDualSAMD51 controller0(0, "SERCOM0");
    controllers.push_back(&controller0);

    static SPIDualSAMD51 controller1(1, "SERCOM1");
    controllers.push_back(&controller1);

    static SPIDualSAMD51 controller2(2, "SERCOM2");
    controllers.push_back(&controller2);

    static SPIDualSAMD51 controller3(3, "SERCOM3");
    controllers.push_back(&controller3);

    return controllers;
}

}  // namespace fl

#endif  // __SAMD51G19A__ || __SAMD51J19A__ || ...
