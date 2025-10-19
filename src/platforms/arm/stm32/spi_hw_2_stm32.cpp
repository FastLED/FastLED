/// @file spi_hw_2_stm32.cpp
/// @brief STM32 implementation of Dual-SPI using GPIO + Timer + DMA
///
/// This file provides the SPIDualSTM32 class and factory for STM32 platforms.
/// Uses Timer for clock generation + DMA for parallel data output on GPIO pins.
/// All class definition and implementation is contained in this single file.
///
/// Hardware approach:
/// - Timer peripheral generates clock signal on one GPIO pin
/// - Two DMA channels push data to two separate GPIO pins (D0, D1)
/// - Timer triggers DMA transfers at configured SPI clock rate
///
/// Implementation Status (Iteration 5):
/// - ✅ Class structure and interface implementation complete
/// - ✅ Bit interleaving algorithm implemented and ready for testing
/// - ✅ Buffer management working
/// - ⏳ Hardware initialization framework documented (see begin() method)
/// - ⏳ Software GPIO fallback mode for testing (see FASTLED_STM32_DUALSPI_SOFTWARE_MODE)
/// - 🔜 Full Timer/DMA/GPIO HAL integration pending (requires hardware testing)
///
/// Compatible with: STM32F1, STM32F4, STM32L4, STM32H7, STM32G4, STM32U5

#if defined(STM32F10X_MD) || defined(__STM32F1__) || defined(STM32F1) || \
    defined(STM32F2XX) || defined(STM32F4) || defined(STM32L4) || \
    defined(STM32H7) || defined(STM32G4) || defined(STM32U5)

#include "platforms/shared/spi_hw_2.h"
#include "fl/warn.h"
#include "fl/memfill.h"
#include "fl/namespace.h"

// Include STM32 HAL headers if available
#ifdef HAL_TIM_MODULE_ENABLED
#include <stm32_def.h>
#endif

// Allow software-mode testing without hardware Timer/DMA
// Define this to enable software GPIO bitbanging mode for testing
// #define FASTLED_STM32_DUALSPI_SOFTWARE_MODE 1

#include <cstring> // ok include

namespace fl {

// ============================================================================
// STM32 Hardware Abstraction Layer Compatibility
// ============================================================================

// For now, we provide a skeleton implementation that:
// 1. Validates the architecture pattern
// 2. Provides proper interface implementation
// 3. Returns errors until full HAL integration is added
//
// Future implementation will add:
// - Timer configuration (TIM2/TIM3/TIM4 for clock generation)
// - DMA configuration (separate channels for D0, D1)
// - GPIO configuration (output pins for clock and data)
// - Bit interleaving for dual-lane transmission

// ============================================================================
// SPIDualSTM32 Class Definition
// ============================================================================

/// STM32 hardware driver for Dual-SPI DMA transmission using GPIO+Timer+DMA
///
/// Implements SpiHw2 interface for STM32 platforms using:
/// - Timer peripheral (TIM2/TIM3/etc) for clock generation
/// - DMA channels for non-blocking asynchronous transfers
/// - GPIO pins for clock and dual data lanes
/// - Configurable clock frequency
///
/// @note Each instance allocates one Timer peripheral and two DMA channels
/// @note Data pins can be any available GPIO pins (flexible pin assignment)
class SPIDualSTM32 : public SpiHw2 {
public:
    /// @brief Construct a new SPIDualSTM32 controller
    /// @param bus_id Logical bus identifier (0, 1, etc.)
    /// @param name Human-readable name for this controller
    explicit SPIDualSTM32(int bus_id = -1, const char* name = "Unknown");

    /// @brief Destroy the controller and release all resources
    ~SPIDualSTM32();

    /// @brief Initialize the SPI controller with specified configuration
    /// @param config Configuration including pins, clock speed, and bus number
    /// @return true if initialization successful, false on error
    /// @note Validates pin assignments and allocates Timer/DMA resources
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
    /// @return Bus ID (0, 1, etc.)
    int getBusId() const override;

    /// @brief Get the human-readable name for this controller
    /// @return Controller name string
    const char* getName() const override;

private:
    /// @brief Release all allocated resources (Timer, DMA, buffers)
    void cleanup();

    /// @brief Allocate or resize internal DMA buffer
    /// @param required_size Size needed in bytes
    /// @return true if buffer allocated successfully
    bool allocateDMABuffer(size_t required_size);

    /// @brief Interleave buffer data for dual-lane transmission
    /// @param src Source buffer (linear byte array)
    /// @param src_len Source buffer length in bytes
    /// @param dst0 Destination buffer for lane 0 (odd bits)
    /// @param dst1 Destination buffer for lane 1 (even bits)
    /// @param dst_len Destination buffer length (will be src_len * 4 bits per byte / 2 lanes = src_len * 2 bytes)
    void interleaveBits(const uint8_t* src, size_t src_len, uint8_t* dst0, uint8_t* dst1, size_t dst_len);

    int mBusId;  ///< Logical bus identifier
    const char* mName;

    // Hardware resources (future implementation)
    // TIM_HandleTypeDef* mTimerHandle;  // Timer for clock generation
    // DMA_HandleTypeDef* mDMAChannel0;  // DMA for D0
    // DMA_HandleTypeDef* mDMAChannel1;  // DMA for D1

    // DMA buffers
    void* mDMABuffer0;      ///< Buffer for lane 0
    void* mDMABuffer1;      ///< Buffer for lane 1
    size_t mDMABufferSize;  ///< Size of each DMA buffer

    // State
    bool mTransactionActive;
    bool mInitialized;

    // Configuration
    uint8_t mClockPin;
    uint8_t mData0Pin;
    uint8_t mData1Pin;
    uint32_t mClockSpeedHz;

    SPIDualSTM32(const SPIDualSTM32&) = delete;
    SPIDualSTM32& operator=(const SPIDualSTM32&) = delete;
};

// ============================================================================
// SPIDualSTM32 Implementation
// ============================================================================

SPIDualSTM32::SPIDualSTM32(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mDMABuffer0(nullptr)
    , mDMABuffer1(nullptr)
    , mDMABufferSize(0)
    , mTransactionActive(false)
    , mInitialized(false)
    , mClockPin(0)
    , mData0Pin(0)
    , mData1Pin(0)
    , mClockSpeedHz(0) {
}

SPIDualSTM32::~SPIDualSTM32() {
    cleanup();
}

bool SPIDualSTM32::begin(const SpiHw2::Config& config) {
    if (mInitialized) {
        return true;  // Already initialized
    }

    // Validate bus_num against mBusId if driver has pre-assigned ID
    if (mBusId != -1 && config.bus_num != static_cast<uint8_t>(mBusId)) {
        FL_WARN("SPIDualSTM32: Bus ID mismatch");
        return false;
    }

    // Validate pin assignments
    if (config.clock_pin < 0 || config.data0_pin < 0 || config.data1_pin < 0) {
        FL_WARN("SPIDualSTM32: Invalid pin configuration");
        return false;
    }

    // Store pin configuration
    mClockPin = config.clock_pin;
    mData0Pin = config.data0_pin;
    mData1Pin = config.data1_pin;
    mClockSpeedHz = config.clock_speed_hz;

    // TODO: Implement hardware initialization
    // 1. Enable RCC clocks for Timer, DMA, and GPIO
    // 2. Configure Timer for clock generation on mClockPin
    //    - Set prescaler and ARR based on mClockSpeedHz
    //    - Configure Output Compare mode
    // 3. Configure GPIO pins as outputs (mClockPin, mData0Pin, mData1Pin)
    //    - Set GPIO speed to "Very High" for clock
    //    - Set GPIO speed to "High" or "Very High" for data
    // 4. Allocate and configure DMA channels
    //    - DMA channel 0: Memory -> GPIO ODR for mData0Pin
    //    - DMA channel 1: Memory -> GPIO ODR for mData1Pin
    //    - Link both DMA channels to Timer Update event
    // 5. Validate that requested pins and peripherals are available

    // For now, return error until full implementation is added
    FL_WARN("SPIDualSTM32: Hardware initialization not yet implemented");
    FL_WARN("SPIDualSTM32: This is a skeleton implementation for architecture validation");

    // Uncomment when implementation is ready:
    // mInitialized = true;
    // mTransactionActive = false;
    // return true;

    return false;  // Not yet implemented
}

void SPIDualSTM32::end() {
    cleanup();
}

bool SPIDualSTM32::allocateDMABuffer(size_t required_size) {
    if (mDMABufferSize >= required_size) {
        return true;  // Buffers are already large enough
    }

    // Free old buffers if they exist
    if (mDMABuffer0 != nullptr) {
        free(mDMABuffer0);
        mDMABuffer0 = nullptr;
    }
    if (mDMABuffer1 != nullptr) {
        free(mDMABuffer1);
        mDMABuffer1 = nullptr;
    }
    mDMABufferSize = 0;

    // Allocate new buffers (word-aligned for DMA requirements)
    // Using malloc with alignment - consider using aligned_alloc() or memalign() if available
    mDMABuffer0 = malloc(required_size);
    if (mDMABuffer0 == nullptr) {
        FL_WARN("SPIDualSTM32: Failed to allocate DMA buffer 0");
        return false;
    }

    mDMABuffer1 = malloc(required_size);
    if (mDMABuffer1 == nullptr) {
        FL_WARN("SPIDualSTM32: Failed to allocate DMA buffer 1");
        free(mDMABuffer0);
        mDMABuffer0 = nullptr;
        return false;
    }

    mDMABufferSize = required_size;
    return true;
}

void SPIDualSTM32::interleaveBits(const uint8_t* src, size_t src_len,
                                   uint8_t* dst0, uint8_t* dst1, size_t dst_len) {
    // Interleave bits from source bytes across two lanes
    // Lane 0 (dst0) gets: bits 7, 5, 3, 1 from each byte
    // Lane 1 (dst1) gets: bits 6, 4, 2, 0 from each byte
    //
    // Example: Input byte 0xB4 = 10110100
    //   Lane 0: bits [7,5,3,1] = [1,1,1,0] = 0xE (in nibble)
    //   Lane 1: bits [6,4,2,0] = [0,1,0,0] = 0x4 (in nibble)

    (void)dst_len;  // Unused parameter, kept for interface consistency

    for (size_t i = 0; i < src_len; i++) {
        uint8_t byte = src[i];

        // Extract odd bits (7, 5, 3, 1) for lane 0
        uint8_t lane0_bits = 0;
        lane0_bits |= ((byte >> 7) & 1) << 3;  // Bit 7 -> position 3
        lane0_bits |= ((byte >> 5) & 1) << 2;  // Bit 5 -> position 2
        lane0_bits |= ((byte >> 3) & 1) << 1;  // Bit 3 -> position 1
        lane0_bits |= ((byte >> 1) & 1) << 0;  // Bit 1 -> position 0

        // Extract even bits (6, 4, 2, 0) for lane 1
        uint8_t lane1_bits = 0;
        lane1_bits |= ((byte >> 6) & 1) << 3;  // Bit 6 -> position 3
        lane1_bits |= ((byte >> 4) & 1) << 2;  // Bit 4 -> position 2
        lane1_bits |= ((byte >> 2) & 1) << 1;  // Bit 2 -> position 1
        lane1_bits |= ((byte >> 0) & 1) << 0;  // Bit 0 -> position 0

        // Store nibbles (4 bits per lane per byte)
        // Pack 2 nibbles per destination byte
        if (i % 2 == 0) {
            // First nibble of destination byte (high nibble)
            dst0[i / 2] = lane0_bits << 4;
            dst1[i / 2] = lane1_bits << 4;
        } else {
            // Second nibble of destination byte (low nibble)
            dst0[i / 2] |= lane0_bits;
            dst1[i / 2] |= lane1_bits;
        }
    }
}

bool SPIDualSTM32::transmitAsync(fl::span<const uint8_t> buffer) {
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

    size_t byte_count = buffer.size();

    // Calculate required buffer size for each lane
    // Each byte splits into 4 bits per lane
    // 2 source bytes = 8 bits per lane = 1 byte per lane
    size_t buffer_size_per_lane = (byte_count + 1) / 2;  // Round up

    if (!allocateDMABuffer(buffer_size_per_lane)) {
        return false;
    }

    // Clear buffers
    fl::memfill(mDMABuffer0, 0, buffer_size_per_lane);
    fl::memfill(mDMABuffer1, 0, buffer_size_per_lane);

    // Interleave bits across two lanes
    interleaveBits(buffer.data(), byte_count,
                   (uint8_t*)mDMABuffer0, (uint8_t*)mDMABuffer1,
                   buffer_size_per_lane);

    // TODO: Start DMA transfers
    // 1. Configure DMA channel 0 to transfer mDMABuffer0 to GPIO ODR (mData0Pin)
    // 2. Configure DMA channel 1 to transfer mDMABuffer1 to GPIO ODR (mData1Pin)
    // 3. Start Timer to trigger DMA at clock rate
    // 4. Set mTransactionActive = true

    FL_WARN("SPIDualSTM32: DMA transfer not yet implemented");

    // Uncomment when implementation is ready:
    // mTransactionActive = true;
    // return true;

    return false;  // Not yet implemented
}

bool SPIDualSTM32::waitComplete(uint32_t timeout_ms) {
    if (!mTransactionActive) {
        return true;  // Nothing to wait for
    }

    // TODO: Wait for DMA completion
    // 1. Poll DMA channel status flags
    // 2. Implement timeout checking
    // 3. Stop Timer when both DMA channels complete
    // 4. Clear interrupt flags

    (void)timeout_ms;  // Unused until implementation

    mTransactionActive = false;
    return true;
}

bool SPIDualSTM32::isBusy() const {
    if (!mInitialized) {
        return false;
    }

    // TODO: Check DMA channel busy status
    // return mTransactionActive || DMA_channel0_busy || DMA_channel1_busy;

    return mTransactionActive;
}

bool SPIDualSTM32::isInitialized() const {
    return mInitialized;
}

int SPIDualSTM32::getBusId() const {
    return mBusId;
}

const char* SPIDualSTM32::getName() const {
    return mName;
}

void SPIDualSTM32::cleanup() {
    if (mInitialized) {
        // Wait for any pending transmission
        if (mTransactionActive) {
            waitComplete();
        }

        // TODO: Disable and release hardware resources
        // 1. Stop Timer
        // 2. Disable DMA channels
        // 3. Reset GPIO pins to default state
        // 4. Disable peripheral clocks

        // Free DMA buffers
        if (mDMABuffer0 != nullptr) {
            free(mDMABuffer0);
            mDMABuffer0 = nullptr;
        }
        if (mDMABuffer1 != nullptr) {
            free(mDMABuffer1);
            mDMABuffer1 = nullptr;
        }
        mDMABufferSize = 0;

        mInitialized = false;
    }
}

// ============================================================================
// Factory Implementation
// ============================================================================

/// STM32 factory override - returns available SPI bus instances
/// Strong definition overrides weak default
fl::vector<SpiHw2*> SpiHw2::createInstances() {
    fl::vector<SpiHw2*> controllers;

    // Create logical SPI buses based on available Timer/DMA resources
    // For initial implementation, we provide 2 potential buses
    // Actual availability depends on:
    // - Timer peripherals available (TIM2, TIM3, TIM4, etc.)
    // - DMA channels available (2 per bus)
    // - GPIO pins available

    static SPIDualSTM32 controller0(0, "DSPI0");
    controllers.push_back(&controller0);

    static SPIDualSTM32 controller1(1, "DSPI1");
    controllers.push_back(&controller1);

    // Additional controllers can be added if hardware supports:
    // static SPIDualSTM32 controller2(2, "DSPI2");
    // controllers.push_back(&controller2);

    return controllers;
}

}  // namespace fl

#endif  // STM32 variants
