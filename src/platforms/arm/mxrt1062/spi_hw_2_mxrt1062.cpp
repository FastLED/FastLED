/// @file spi_hw_2_mxrt1062.cpp
/// @brief Teensy 4.x (IMXRT1062) implementation of 2-lane (Dual) SPI
///
/// This file provides the SpiHw2MXRT1062 class and factory for Teensy 4.x platforms.
/// All class definition and implementation is contained in this single file.
///
/// The IMXRT1062's LPSPI peripheral supports dual-mode transfers by configuring
/// the WIDTH field in the transmit command register (TCR).

#if defined(__IMXRT1062__) && defined(ARM_HARDWARE_SPI)

#include "platforms/shared/spi_hw_2.h"
#include "fl/namespace.h"
#include "fl/warn.h"
#include <SPI.h>
#include <imxrt.h>
#include <cstring>

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
    bool transmitAsync(fl::span<const uint8_t> buffer) override;
    bool waitComplete(uint32_t timeout_ms = UINT32_MAX) override;
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

    // Initialize the SPI peripheral
    // Note: The Teensy SPI library doesn't expose low-level LPSPI configuration
    // We'll use the standard begin() and configure dual mode per-transaction
    mSPI->begin();

    // Note: For true dual-mode operation, we would need to:
    // 1. Configure MOSI and MISO pins for bidirectional use
    // 2. Set the WIDTH field in TCR register to 0b01 (2-bit transfer)
    // 3. Handle the pin remapping for dual-mode
    //
    // The Teensy core SPI library doesn't expose these low-level controls,
    // so a complete implementation would need direct register access.
    // For now, we mark it as initialized and will handle dual-mode encoding
    // in the transmit function.

    FL_WARN("SpiHw2MXRT1062: Initialized on bus " << mBusId
            << " (Note: Teensy SPI library has limited dual-mode support)");

    mInitialized = true;
    mTransactionActive = false;

    return true;
}

void SpiHw2MXRT1062::end() {
    cleanup();
}

bool SpiHw2MXRT1062::transmitAsync(fl::span<const uint8_t> buffer) {
    if (!mInitialized || !mSPI) {
        return false;
    }

    // Wait for previous transaction if still active
    if (mTransactionActive) {
        waitComplete();
    }

    if (buffer.empty()) {
        return true;  // Nothing to transmit
    }

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

    // Transmit data
    // In dual mode, each byte in the buffer will be transmitted as nibbles
    // split across the two data lines
    for (size_t i = 0; i < buffer.size(); ++i) {
        // Wait for transmit FIFO to have space
        while (!(port->SR & LPSPI_SR_TDF)) ;

        port->TDR = buffer[i];
    }

    // Wait for transmission to complete
    while (port->SR & LPSPI_SR_MBF) ;  // Module Busy Flag

    // Restore original TCR
    port->TCR = old_tcr;

    mSPI->endTransaction();
    mTransactionActive = false;

    return true;
}

bool SpiHw2MXRT1062::waitComplete(uint32_t timeout_ms) {
    if (!mTransactionActive) {
        return true;  // Nothing to wait for
    }

    // For synchronous implementation, transmission is already complete
    mTransactionActive = false;
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

        mSPI->end();
        mSPI = nullptr;
        mInitialized = false;
    }
}

// ============================================================================
// Factory Implementation
// ============================================================================

/// Teensy 4.x factory override - returns available 2-lane SPI bus instances
/// Strong definition overrides weak default
fl::vector<SpiHw2*> SpiHw2::createInstances() {
    fl::vector<SpiHw2*> controllers;

    // Teensy 4.x has 3 LPSPI peripherals available
    // SPI (bus 0), SPI1 (bus 1), SPI2 (bus 2)
    static SpiHw2MXRT1062 controller0(0, "SPI");
    static SpiHw2MXRT1062 controller1(1, "SPI1");
    static SpiHw2MXRT1062 controller2(2, "SPI2");

    controllers.push_back(&controller0);
    controllers.push_back(&controller1);
    controllers.push_back(&controller2);

    return controllers;
}

}  // namespace fl

#endif  // __IMXRT1062__ && ARM_HARDWARE_SPI
