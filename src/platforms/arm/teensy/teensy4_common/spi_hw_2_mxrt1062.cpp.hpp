/// @file spi_hw_2_mxrt1062.cpp
/// @brief Teensy 4.x (IMXRT1062) implementation of 2-lane (Dual) SPI
///
/// This file provides the SpiHw2MXRT1062 class and factory for Teensy 4.x platforms.
/// All class definition and implementation is contained in this single file.
///
/// The IMXRT1062's LPSPI peripheral supports dual-mode transfers by configuring
/// the WIDTH field in the transmit command register (TCR).

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#include "platforms/shared/spi_hw_2.h"
#include "fl/warn.h"
#include "fl/log.h"
#include "fl/numeric_limits.h"
#include <SPI.h>
#include <imxrt.h>
#include <cstring> // ok include
#include "platforms/shared/spi_bus_manager.h"  // For DMABuffer, TransmitMode, SPIError

namespace fl {

// Forward declarations for LPSPI port access
// The Teensy core defines IMXRT_LPSPI4_S, IMXRT_LPSPI3_S, IMXRT_LPSPI1_S
// These correspond to SPI (index 0), SPI1 (index 1), SPI2 (index 2)

// ============================================================================
// SpiHw2MXRT1062 Class Definition
// ============================================================================

/// Teensy 4.x hardware for 2-lane (Dual) SPI transmission
/// Implements SpiHw2 interface for LPSPI peripheral
class SpiHw2MXRT1062 : public SpiHw2 {
public:
    explicit SpiHw2MXRT1062(int bus_id = -1, const char* name = "Unknown");
    ~SpiHw2MXRT1062();

    bool begin(const SpiHw2::Config& config) override;
    void end() override;
    DMABuffer acquireDMABuffer(size_t bytes_per_lane) override;
    bool transmit(TransmitMode mode = TransmitMode::ASYNC) override;
    bool waitComplete(uint32_t timeout_ms = fl::numeric_limits<uint32_t>::max()) override;
    bool isBusy() const override;
    bool isInitialized() const override;
    int getBusId() const override;
    const char* getName() const override;

private:
    void cleanup();
    IMXRT_LPSPI_t* getPort();

    int mBusId;
    const char* mName;
    SPIClass* mSPI;
    bool mTransactionActive;
    bool mInitialized;
    uint32_t mClockSpeed;

    // Pin configuration
    int8_t mClockPin;
    int8_t mData0Pin;
    int8_t mData1Pin;

    // DMA buffer management
    DMABuffer mDMABuffer;            // DMA buffer (managed internally by DMABuffer)
    size_t mCurrentTotalSize;        // Current transmission size (bytes_per_lane * num_lanes)
    bool mBufferAcquired;

    SpiHw2MXRT1062(const SpiHw2MXRT1062&) = delete;
    SpiHw2MXRT1062& operator=(const SpiHw2MXRT1062&) = delete;
};

// ============================================================================
// SpiHw2MXRT1062 Implementation
// ============================================================================

SpiHw2MXRT1062::SpiHw2MXRT1062(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mSPI(nullptr)
    , mTransactionActive(false)
    , mInitialized(false)
    , mClockSpeed(20000000)  // Default 20 MHz
    , mClockPin(-1)
    , mData0Pin(-1)
    , mData1Pin(-1)
    , mDMABuffer()
    , mCurrentTotalSize(0)
    , mBufferAcquired(false)
{
}

SpiHw2MXRT1062::~SpiHw2MXRT1062() {
    cleanup();
}

IMXRT_LPSPI_t* SpiHw2MXRT1062::getPort() {
    // Map bus_id to LPSPI port
    // SPI  (index 0) -> LPSPI4
    // SPI1 (index 1) -> LPSPI3
    // SPI2 (index 2) -> LPSPI1
    switch (mBusId) {
        case 0:
            return &IMXRT_LPSPI4_S;
        case 1:
            return &IMXRT_LPSPI3_S;
        case 2:
            return &IMXRT_LPSPI1_S;
        default:
            return nullptr;
    }
}

bool SpiHw2MXRT1062::begin(const SpiHw2::Config& config) {
    if (mInitialized) {
        return true;  // Already initialized
    }

    // Validate bus_num against mBusId if driver has pre-assigned ID
    if (mBusId != -1 && config.bus_num != static_cast<uint8_t>(mBusId)) {
        FL_WARN("SpiHw2MXRT1062: Bus mismatch - expected " << mBusId << ", got " << static_cast<int>(config.bus_num));
        return false;
    }

    // Select SPI object based on bus_num
    uint8_t bus_num = (mBusId != -1) ? static_cast<uint8_t>(mBusId) : config.bus_num;
    switch (bus_num) {
        case 0:
            mSPI = &SPI;
            mBusId = 0;
            break;
        case 1:
            mSPI = &SPI1;
            mBusId = 1;
            break;
        case 2:
            mSPI = &SPI2;
            mBusId = 2;
            break;
        default:
            FL_WARN("SpiHw2MXRT1062: Invalid bus number " << static_cast<int>(bus_num));
            return false;
    }

    // Validate that both data pins are specified
    if (config.data0_pin < 0 || config.data1_pin < 0) {
        FL_WARN("SpiHw2MXRT1062: Dual-SPI requires both data0 and data1 pins");
        return false;
    }

    // Store configuration
    mClockSpeed = config.clock_speed_hz;
    mClockPin = config.clock_pin;
    mData0Pin = config.data0_pin;
    mData1Pin = config.data1_pin;

    // Validate pins match hardware capabilities
    // Each LPSPI bus has fixed pin assignments on Teensy 4.x
    struct ValidPinSet {
        uint8_t bus_id;
        int8_t sck, mosi, miso;
    };
    constexpr ValidPinSet valid_pins[] = {
        {0, 13, 11, 12},  // SPI  (LPSPI4)
        {1, 27, 26, 1},   // SPI1 (LPSPI3)
        {2, 45, 43, 42}   // SPI2 (LPSPI1)
    };

    bool pins_valid = false;
    for (const auto& vp : valid_pins) {
        if (vp.bus_id == static_cast<uint8_t>(mBusId) &&
            vp.sck == mClockPin &&
            vp.mosi == mData0Pin &&
            vp.miso == mData1Pin) {
            pins_valid = true;
            break;
        }
    }

    if (!pins_valid) {
        FL_WARN("SpiHw2MXRT1062: Invalid pin combination for bus " << mBusId);
        FL_WARN("  Expected: SCK=" << (int)valid_pins[mBusId].sck
                   << " D0=" << (int)valid_pins[mBusId].mosi
                   << " D1=" << (int)valid_pins[mBusId].miso);
        FL_WARN("  Got: SCK=" << (int)mClockPin
                   << " D0=" << (int)mData0Pin
                   << " D1=" << (int)mData1Pin);
        return false;
    }

    // Configure custom pins BEFORE calling begin()
    // The Teensy SPI library requires setMOSI/setSCK/setMISO to be called before begin()
    // to use alternate pins. Without these calls, pins remain at default (11,13 for SPI0).
    if (mClockPin >= 0) {
        mSPI->setSCK(mClockPin);
    }
    if (mData0Pin >= 0) {
        mSPI->setMOSI(mData0Pin);
    }
    // Note: data1_pin is for dual-mode output, not MISO (input)
    // Setting MISO for potential bidirectional use
    if (mData1Pin >= 0) {
        mSPI->setMISO(mData1Pin);
    }

    // Initialize the SPI peripheral
    // Note: The Teensy SPI library doesn't expose low-level LPSPI configuration
    // We'll use the standard begin() and configure dual mode per-transaction
    mSPI->begin();

    // Configure CFGR1 for dual-mode operation
    // OUTCFG bit (26) must be set to enable pin tristating for multi-bit SPI
    // This allows the LPSPI hardware to control SDI (MISO) as an output in dual-mode
    IMXRT_LPSPI_t* port = getPort();
    if (port) {
        uint32_t cfgr1 = port->CFGR1;

        // Set OUTCFG (bit 26) to enable data output tristating for multi-bit mode
        cfgr1 |= LPSPI_CFGR1_OUTCFG;

        // Keep PINCFG at default (0 = kLPSPI_SdiInSdoOut)
        // In dual-mode, the hardware automatically handles pin direction
        // based on TCR.WIDTH setting

        port->CFGR1 = cfgr1;

        FL_LOG_SPI("SpiHw2MXRT1062: Configured CFGR1=" << cfgr1
                   << " (OUTCFG enabled for dual-mode)");
    }

    FL_LOG_SPI("SpiHw2MXRT1062: Initialized on bus " << mBusId
            << " clock=" << mClockSpeed << "Hz"
            << " pins: CLK=" << (int)mClockPin
            << " D0=" << (int)mData0Pin
            << " D1=" << (int)mData1Pin);

    mInitialized = true;
    mTransactionActive = false;

    return true;
}

void SpiHw2MXRT1062::end() {
    cleanup();
}

DMABuffer SpiHw2MXRT1062::acquireDMABuffer(size_t bytes_per_lane) {
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

    // Validate size against Teensy practical limit (256KB for embedded systems)
    constexpr size_t MAX_SIZE = 256 * 1024;
    if (total_size > MAX_SIZE) {
        return DMABuffer(SPIError::BUFFER_TOO_LARGE);
    }

    // Allocate new DMABuffer (manages memory internally via fl::vector)
    mDMABuffer = DMABuffer(total_size);
    if (!mDMABuffer.ok()) {
        return DMABuffer(SPIError::ALLOCATION_FAILED);
    }

    mBufferAcquired = true;
    mCurrentTotalSize = total_size;

    // Return the buffer (DMABuffer is copyable via shared_ptr)
    return mDMABuffer;
}

bool SpiHw2MXRT1062::transmit(TransmitMode mode) {
    if (!mInitialized || !mSPI || !mBufferAcquired) {
        return false;
    }

    // Mode handling (Teensy uses synchronous/blocking via LPSPI)
    (void)mode;

    if (mCurrentTotalSize == 0) {
        return true;  // Nothing to transmit
    }

    FL_LOG_SPI("SpiHw2MXRT1062: Transmitting " << mCurrentTotalSize << " bytes via LPSPI bus " << mBusId);

    // Begin SPI transaction with configured clock speed
    mSPI->beginTransaction(SPISettings(mClockSpeed, MSBFIRST, SPI_MODE0));

    // Note: True dual-mode transmission requires direct LPSPI register access
    // The Teensy SPI library doesn't support this natively.
    //
    // For a complete implementation, we would:
    // 1. Get the LPSPI port via getPort()
    // 2. Set TCR.WIDTH = 0b01 for 2-bit transfers
    // 3. Write interleaved data to TDR register
    // 4. Wait for transmission complete
    //
    // As a fallback, we'll use standard SPI transmission
    // (This won't actually use dual mode, but allows the architecture to work)

    IMXRT_LPSPI_t* port = getPort();
    if (!port) {
        mSPI->endTransaction();
        return false;
    }

    // Save current TCR
    uint32_t old_tcr = port->TCR;

    // Configure for dual-mode (2-bit width)
    // TCR.WIDTH field is bits 17:16
    // 0b00 = 1-bit (standard SPI)
    // 0b01 = 2-bit (dual SPI)
    // 0b10 = 4-bit (quad SPI)
    uint32_t new_tcr = (old_tcr & ~(0x3 << 16)) | (0x1 << 16);  // Set WIDTH to 0b01
    port->TCR = new_tcr;

    // Transmit data using internal DMA buffer
    // In dual mode, each byte in the buffer will be transmitted as nibbles
    // split across the two data lines
    fl::span<uint8_t> buffer_span = mDMABuffer.data();
    for (size_t i = 0; i < mCurrentTotalSize; ++i) {
        // Wait for transmit FIFO to have space
        while (!(port->SR & LPSPI_SR_TDF)) ;

        port->TDR = buffer_span[i];
    }

    // Wait for transmission to complete
    while (port->SR & LPSPI_SR_MBF) ;  // Module Busy Flag

    // Restore original TCR
    port->TCR = old_tcr;

    mSPI->endTransaction();

    // Note: Transaction is complete synchronously, so we can auto-release immediately
    mTransactionActive = false;
    mBufferAcquired = false;
    mCurrentTotalSize = 0;

    return true;
}

bool SpiHw2MXRT1062::waitComplete(uint32_t timeout_ms) {
    if (!mTransactionActive) {
        return true;  // Nothing to wait for
    }

    // For synchronous implementation, transmission is already complete
    mTransactionActive = false;

    // AUTO-RELEASE DMA buffer
    mBufferAcquired = false;
    mCurrentTotalSize = 0;

    return true;
}

bool SpiHw2MXRT1062::isBusy() const {
    return mTransactionActive;
}

bool SpiHw2MXRT1062::isInitialized() const {
    return mInitialized;
}

int SpiHw2MXRT1062::getBusId() const {
    return mBusId;
}

const char* SpiHw2MXRT1062::getName() const {
    return mName;
}

void SpiHw2MXRT1062::cleanup() {
    if (mInitialized && mSPI) {
        // Wait for any pending transmission
        if (mTransactionActive) {
            waitComplete();
        }

        // Reset DMA buffer (shared_ptr will handle deallocation)
        mDMABuffer.reset();
        mCurrentTotalSize = 0;
        mBufferAcquired = false;

        mSPI->end();
        mSPI = nullptr;
        mInitialized = false;
    }
}

// ============================================================================
// Static Registration - New Polymorphic Pattern
// ============================================================================

namespace platform {

/// @brief Initialize Teensy 4.x SpiHw2 instances
///
/// This function is called lazily by SpiHw2::getAll() on first access.
/// It replaces the old FL_INIT-based static initialization.
void initSpiHw2Instances() {
    FL_LOG_SPI("SpiHw2MXRT1062::Registrar - Teensy 4.x hardware SPI registration active");

    // Teensy 4.x has 3 LPSPI peripherals available
    // SPI (bus 0), SPI1 (bus 1), SPI2 (bus 2)
    static auto controller0 = fl::make_shared<SpiHw2MXRT1062>(0, "SPI");
    static auto controller1 = fl::make_shared<SpiHw2MXRT1062>(1, "SPI1");
    static auto controller2 = fl::make_shared<SpiHw2MXRT1062>(2, "SPI2");

    SpiHw2::registerInstance(controller0);
    SpiHw2::registerInstance(controller1);
    SpiHw2::registerInstance(controller2);
}

}  // namespace platform

}  // namespace fl

#endif  // FL_IS_TEENSY_4X
