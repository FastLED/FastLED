/// @file spi_hw_4_stm32.cpp
/// @brief STM32 implementation of Quad-SPI using GPIO + Timer + DMA
///
/// This file provides the SPIQuadSTM32 class and factory for STM32 platforms.
/// Uses Timer for clock generation + DMA for parallel data output on 4 GPIO pins.
/// All class definition and implementation is contained in this single file.
///
/// Hardware approach:
/// - Timer peripheral generates clock signal on one GPIO pin
/// - Four DMA channels push data to four separate GPIO pins (D0, D1, D2, D3)
/// - Timer triggers DMA transfers at configured SPI clock rate
///
///
/// Compatible with: STM32F1, STM32F2, STM32F4, STM32F7, STM32L4, STM32H7, STM32G4, STM32U5

// Include platform detection header for FL_IS_STM32
#include "platforms/arm/stm32/is_stm32.h"

// Platform guard using standardized FL_IS_STM32 macro
#if defined(FL_IS_STM32)

// Platform capability macros
#include "platforms/arm/stm32/stm32_capabilities.h"

#ifdef FL_STM32_HAS_SPI_HW_4

// ============================================================================
// Hardware SPI Support Guard
// ============================================================================
// Only compile this hardware implementation if FL_STM32_HAS_SPI_HW_4 is defined.
// If undefined (e.g., on STM32F1 with channel-based DMA), the weak binding
// will cause FastLED to fall back to software bitbang implementation.
//
// FL_STM32_HAS_SPI_HW_4 is defined in stm32_capabilities.h for platforms with:
// - Stream-based DMA (F2/F4/F7/H7/L4)
//
// Not defined for:
// - F1 (channel-based DMA - not yet implemented)
// - G4 (channel-based DMA with DMAMUX - not yet implemented)
// - U5 (GPDMA architecture - not yet implemented)

#include "fl/stl/stdint.h"
#include "fl/numeric_limits.h"
#include <Arduino.h>  // Ensure STM32 HAL is initialized

#include "platforms/shared/spi_hw_4.h"
#include "fl/warn.h"
#include "fl/dbg.h"
#include "fl/stl/cstring.h"

#include <cstring> // ok include
#include "platforms/shared/spi_bus_manager.h"  // For DMABuffer, TransmitMode, SPIError
#include "platforms/arm/stm32/stm32_gpio_timer_helpers.h"  // Centralized GPIO/Timer/DMA helpers

namespace fl {

///
/// Implements SpiHw4 interface for STM32 platforms using:
/// - Timer peripheral (TIM2/TIM3/etc) for clock generation
/// - DMA channels for non-blocking asynchronous transfers
/// - GPIO pins for clock and quad data lanes
/// - Configurable clock frequency
///
/// @note Each instance allocates one Timer peripheral and four DMA channels
/// @note Data pins can be any available GPIO pins (flexible pin assignment)
class SPIQuadSTM32 : public SpiHw4 {
public:
    /// @brief Construct a new SPIQuadSTM32 controller
    /// @param bus_id Logical bus identifier (0, 1, etc.)
    /// @param name Human-readable name for this controller
    explicit SPIQuadSTM32(int bus_id = -1, const char* name = "Unknown");

    /// @brief Destroy the controller and release all resources
    ~SPIQuadSTM32();

    /// @brief Initialize the SPI controller with specified configuration
    /// @param config Configuration including pins, clock speed, and bus number
    /// @return true if initialization successful, false on error
    /// @note Validates pin assignments and allocates Timer/DMA resources
    bool begin(const SpiHw4::Config& config) override;

    /// @brief Deinitialize the controller and release resources
    void end() override;

    /// @brief Acquire DMA buffer for direct user writes (zero-copy)
    /// @param bytes_per_lane Number of bytes per lane (total = bytes_per_lane * 4)
    /// @return DMABuffer containing buffer span or error
    /// @note Auto-waits if previous transmission still active
    DMABuffer acquireDMABuffer(size_t bytes_per_lane) override;

    /// @brief Start non-blocking transmission of previously acquired DMA buffer
    /// @return true if transfer started successfully, false on error
    /// @note Must call acquireDMABuffer() first
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
    /// @return Bus ID (0, 1, etc.)
    int getBusId() const override;

    /// @brief Get the human-readable name for this controller
    /// @return Controller name string
    const char* getName() const override;

private:
    /// @brief Release all allocated resources (Timer, DMA, buffers)
    void cleanup();

    /// @brief Allocate or resize internal DMA buffers for all 4 lanes
    /// @param required_size Size needed in bytes per lane
    /// @return true if buffers allocated successfully
    bool allocateDMABuffer(size_t required_size);

    /// @brief Interleave buffer data for quad-lane transmission
    /// @param src Source buffer (linear byte array)
    /// @param src_len Source buffer length in bytes
    /// @param dst0 Destination buffer for lane 0 (bits [7,3] from each byte)
    /// @param dst1 Destination buffer for lane 1 (bits [6,2] from each byte)
    /// @param dst2 Destination buffer for lane 2 (bits [5,1] from each byte)
    /// @param dst3 Destination buffer for lane 3 (bits [4,0] from each byte)
    /// @param dst_len Destination buffer length per lane
    void interleaveBits(const uint8_t* src, size_t src_len,
                        uint8_t* dst0, uint8_t* dst1, uint8_t* dst2, uint8_t* dst3,
                        size_t dst_len);

    int mBusId;  ///< Logical bus identifier
    const char* mName;
    
    // Hardware resources
    TIM_TypeDef* mTimer;  ///< Timer peripheral for clock generation

    // Hardware resources (future implementation)
    // TIM_HandleTypeDef* mTimerHandle;  // Timer for clock generation
    // DMA_HandleTypeDef* mDMAChannel0;  // DMA for D0
    // DMA_HandleTypeDef* mDMAChannel1;  // DMA for D1
    // DMA_HandleTypeDef* mDMAChannel2;  // DMA for D2
    // DMA_HandleTypeDef* mDMAChannel3;  // DMA for D3

    // DMA buffer management (zero-copy API)
    fl::span<uint8_t> mDMABuffer;    ///< Allocated DMA buffer (interleaved format for quad-lane)
    size_t mMaxBytesPerLane;         ///< Max bytes per lane we've allocated for
    size_t mCurrentTotalSize;        ///< Current transmission size (bytes_per_lane * 4)
    bool mBufferAcquired;            ///< True if buffer acquired and ready for transmit

    // Legacy buffers (will be derived from mDMABuffer for lane splitting)
    void* mDMABuffer0;      ///< Buffer for lane 0 (derived from mDMABuffer)
    void* mDMABuffer1;      ///< Buffer for lane 1 (derived from mDMABuffer)
    void* mDMABuffer2;      ///< Buffer for lane 2 (derived from mDMABuffer)
    void* mDMABuffer3;      ///< Buffer for lane 3 (derived from mDMABuffer)
    size_t mDMABufferSize;  ///< Size of each DMA buffer (derived)

    // State
    bool mTransactionActive;
    bool mInitialized;

    // Configuration
    uint8_t mClockPin;
    uint8_t mData0Pin;
    uint8_t mData1Pin;
    uint8_t mData2Pin;
    uint8_t mData3Pin;
    uint32_t mClockSpeedHz;

    SPIQuadSTM32(const SPIQuadSTM32&) = delete;
    SPIQuadSTM32& operator=(const SPIQuadSTM32&) = delete;
};

// ============================================================================
// SPIQuadSTM32 Implementation
// ============================================================================

using namespace fl::stm32;  // For GPIO/Timer/DMA helper functions

SPIQuadSTM32::SPIQuadSTM32(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mDMABuffer()
    , mMaxBytesPerLane(0)
    , mCurrentTotalSize(0)
    , mBufferAcquired(false)
    , mDMABuffer0(nullptr)
    , mDMABuffer1(nullptr)
    , mDMABuffer2(nullptr)
    , mDMABuffer3(nullptr)
    , mDMABufferSize(0)
    , mTransactionActive(false)
    , mInitialized(false)
    , mClockPin(0)
    , mData0Pin(0)
    , mData1Pin(0)
    , mData2Pin(0)
    , mData3Pin(0)
    , mClockSpeedHz(0) {
}

SPIQuadSTM32::~SPIQuadSTM32() {
    cleanup();
}

bool SPIQuadSTM32::begin(const SpiHw4::Config& config) {
    if (mInitialized) {
        return true;  // Already initialized
    }

    // Validate bus_num against mBusId if driver has pre-assigned ID
    if (mBusId != -1 && config.bus_num != static_cast<uint8_t>(mBusId)) {
        FL_WARN("SPIQuadSTM32: Bus ID mismatch");
        return false;
    }

    // Validate pin assignments - require clock and all 4 data pins for full quad mode
    if (config.clock_pin < 0 || config.data0_pin < 0 || config.data1_pin < 0 ||
        config.data2_pin < 0 || config.data3_pin < 0) {
        FL_WARN("SPIQuadSTM32: Invalid pin configuration (clock and D0-D3 required)");
        return false;
    }

    // Store pin configuration
    mClockPin = config.clock_pin;
    mData0Pin = config.data0_pin;
    mData1Pin = config.data1_pin;
    mData2Pin = config.data2_pin;
    mData3Pin = config.data3_pin;
    mClockSpeedHz = config.clock_speed_hz;

    // Validate all pins using GPIO helper functions
    if (!isValidPin(mClockPin)) {
        FL_WARN("SPIQuadSTM32: Invalid clock pin " << static_cast<int>(mClockPin));
        return false;
    }
    if (!isValidPin(mData0Pin)) {
        FL_WARN("SPIQuadSTM32: Invalid data0 pin " << static_cast<int>(mData0Pin));
        return false;
    }
    if (!isValidPin(mData1Pin)) {
        FL_WARN("SPIQuadSTM32: Invalid data1 pin " << static_cast<int>(mData1Pin));
        return false;
    }
    if (!isValidPin(mData2Pin)) {
        FL_WARN("SPIQuadSTM32: Invalid data2 pin " << static_cast<int>(mData2Pin));
        return false;
    }
    if (!isValidPin(mData3Pin)) {
        FL_WARN("SPIQuadSTM32: Invalid data3 pin " << static_cast<int>(mData3Pin));
        return false;
    }

    // Configure GPIO pins
#ifdef HAL_GPIO_MODULE_ENABLED
    // Configure all 4 data pins as outputs
    if (!configurePinAsOutput(mData0Pin, GPIO_SPEED_FREQ_HIGH)) {
        FL_WARN("SPIQuadSTM32: Failed to configure data0 pin");
        return false;
    }
    if (!configurePinAsOutput(mData1Pin, GPIO_SPEED_FREQ_HIGH)) {
        FL_WARN("SPIQuadSTM32: Failed to configure data1 pin");
        return false;
    }
    if (!configurePinAsOutput(mData2Pin, GPIO_SPEED_FREQ_HIGH)) {
        FL_WARN("SPIQuadSTM32: Failed to configure data2 pin");
        return false;
    }
    if (!configurePinAsOutput(mData3Pin, GPIO_SPEED_FREQ_HIGH)) {
        FL_WARN("SPIQuadSTM32: Failed to configure data3 pin");
        return false;
    }

    FL_DBG("SPIQuadSTM32: GPIO pins configured successfully");
    FL_DBG("  Clock pin: " << static_cast<int>(mClockPin));
    FL_DBG("  Data pins: " << static_cast<int>(mData0Pin) << ", "
                           << static_cast<int>(mData1Pin) << ", "
                           << static_cast<int>(mData2Pin) << ", "
                           << static_cast<int>(mData3Pin));
#endif

    // TODO: Implement remaining hardware initialization
    // 1. Configure Timer for clock generation on mClockPin
    //    - Enable Timer clock (e.g., __HAL_RCC_TIM2_CLK_ENABLE())
    //    - Set prescaler and ARR based on mClockSpeedHz
    //    - Configure Output Compare/PWM mode
    //    - Configure mClockPin as Timer alternate function
    // 2. Configure 4 DMA channels
    //    - Enable DMA clock (e.g., __HAL_RCC_DMA1_CLK_ENABLE())
    //    - DMA channels 0-3: Memory -> GPIO ODR for mData0-3 pins
    //    - Link all DMA channels to Timer Update event
    // 3. Start Timer

    // For now, return error until Timer/DMA implementation is added
    FL_WARN("SPIQuadSTM32: Timer/DMA initialization not yet implemented");
    FL_WARN("SPIQuadSTM32: GPIO configuration complete - hardware integration not complete");

    // Uncomment when Timer/DMA implementation is ready:
    // mInitialized = true;
    // mTransactionActive = false;
    // return true;

    return false;  // Timer/DMA not yet implemented
}

void SPIQuadSTM32::end() {
    cleanup();
}

DMABuffer SPIQuadSTM32::acquireDMABuffer(size_t bytes_per_lane) {
    if (!mInitialized) {
        return DMABuffer(SPIError::NOT_INITIALIZED);
    }

    // Auto-wait if previous transmission still active
    if (mTransactionActive) {
        if (!waitComplete()) {
            return DMABuffer(SPIError::BUSY);
        }
    }

    // For quad-lane SPI: total size = bytes_per_lane × 4 (interleaved)
    constexpr size_t num_lanes = 4;
    const size_t total_size = bytes_per_lane * num_lanes;

    // STM32 doesn't have a specific max size limit like ESP32, but validate reasonable size
    // Use 256KB as a practical limit for embedded systems
    constexpr size_t MAX_SIZE = 256 * 1024;
    if (total_size > MAX_SIZE) {
        return DMABuffer(SPIError::BUFFER_TOO_LARGE);
    }

    // Reallocate buffer only if we need more capacity
    if (bytes_per_lane > mMaxBytesPerLane) {
        if (!mDMABuffer.empty()) {
            free(mDMABuffer.data());
            mDMABuffer = fl::span<uint8_t>();
        }

        // Allocate DMA-capable memory (regular malloc for STM32)
        uint8_t* ptr = static_cast<uint8_t*>(malloc(total_size));
        if (!ptr) {
            return DMABuffer(SPIError::ALLOCATION_FAILED);
        }

        mDMABuffer = fl::span<uint8_t>(ptr, total_size);
        mMaxBytesPerLane = bytes_per_lane;
    }

    mBufferAcquired = true;
    mCurrentTotalSize = total_size;

    // Return DMABuffer constructed from size
    return DMABuffer(total_size);
}

bool SPIQuadSTM32::allocateDMABuffer(size_t required_size) {
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
    if (mDMABuffer2 != nullptr) {
        free(mDMABuffer2);
        mDMABuffer2 = nullptr;
    }
    if (mDMABuffer3 != nullptr) {
        free(mDMABuffer3);
        mDMABuffer3 = nullptr;
    }
    mDMABufferSize = 0;

    // Allocate new buffers (word-aligned for DMA requirements)
    mDMABuffer0 = malloc(required_size);
    if (mDMABuffer0 == nullptr) {
        FL_WARN("SPIQuadSTM32: Failed to allocate DMA buffer 0");
        return false;
    }

    mDMABuffer1 = malloc(required_size);
    if (mDMABuffer1 == nullptr) {
        FL_WARN("SPIQuadSTM32: Failed to allocate DMA buffer 1");
        free(mDMABuffer0);
        mDMABuffer0 = nullptr;
        return false;
    }

    mDMABuffer2 = malloc(required_size);
    if (mDMABuffer2 == nullptr) {
        FL_WARN("SPIQuadSTM32: Failed to allocate DMA buffer 2");
        free(mDMABuffer0);
        free(mDMABuffer1);
        mDMABuffer0 = nullptr;
        mDMABuffer1 = nullptr;
        return false;
    }

    mDMABuffer3 = malloc(required_size);
    if (mDMABuffer3 == nullptr) {
        FL_WARN("SPIQuadSTM32: Failed to allocate DMA buffer 3");
        free(mDMABuffer0);
        free(mDMABuffer1);
        free(mDMABuffer2);
        mDMABuffer0 = nullptr;
        mDMABuffer1 = nullptr;
        mDMABuffer2 = nullptr;
        return false;
    }

    mDMABufferSize = required_size;
    return true;
}

void SPIQuadSTM32::interleaveBits(const uint8_t* src, size_t src_len,
                                   uint8_t* dst0, uint8_t* dst1, uint8_t* dst2, uint8_t* dst3,
                                   size_t dst_len) {
    // Interleave bits from source bytes across four lanes
    // Lane 0 (dst0) gets: bits 7, 3 from each byte
    // Lane 1 (dst1) gets: bits 6, 2 from each byte
    // Lane 2 (dst2) gets: bits 5, 1 from each byte
    // Lane 3 (dst3) gets: bits 4, 0 from each byte
    //
    // Example: Input byte 0xB4 = 10110100
    //   Lane 0: bits [7,3] = [1,1] = 0x3 (in 2-bit group)
    //   Lane 1: bits [6,2] = [0,1] = 0x1
    //   Lane 2: bits [5,1] = [1,0] = 0x2
    //   Lane 3: bits [4,0] = [0,0] = 0x0
    //
    // For quad-lane, each source byte produces 2 bits per lane
    // 4 source bytes = 8 bits per lane = 1 byte per lane

    (void)dst_len;  // Unused parameter, kept for interface consistency

    for (size_t i = 0; i < src_len; i++) {
        uint8_t byte = src[i];

        // Extract bits for each lane (2 bits per lane per source byte)
        uint8_t lane0_bits = 0;
        lane0_bits |= ((byte >> 7) & 1) << 1;  // Bit 7 -> position 1
        lane0_bits |= ((byte >> 3) & 1) << 0;  // Bit 3 -> position 0

        uint8_t lane1_bits = 0;
        lane1_bits |= ((byte >> 6) & 1) << 1;  // Bit 6 -> position 1
        lane1_bits |= ((byte >> 2) & 1) << 0;  // Bit 2 -> position 0

        uint8_t lane2_bits = 0;
        lane2_bits |= ((byte >> 5) & 1) << 1;  // Bit 5 -> position 1
        lane2_bits |= ((byte >> 1) & 1) << 0;  // Bit 1 -> position 0

        uint8_t lane3_bits = 0;
        lane3_bits |= ((byte >> 4) & 1) << 1;  // Bit 4 -> position 1
        lane3_bits |= ((byte >> 0) & 1) << 0;  // Bit 0 -> position 0

        // Store 2-bit groups (pack 4 groups per destination byte)
        // Each destination byte holds data from 4 source bytes
        size_t dst_byte_idx = i / 4;
        size_t bit_offset = (3 - (i % 4)) * 2;  // 6, 4, 2, 0

        if (i % 4 == 0) {
            // First byte of a new group - initialize
            dst0[dst_byte_idx] = lane0_bits << bit_offset;
            dst1[dst_byte_idx] = lane1_bits << bit_offset;
            dst2[dst_byte_idx] = lane2_bits << bit_offset;
            dst3[dst_byte_idx] = lane3_bits << bit_offset;
        } else {
            // Add to existing byte
            dst0[dst_byte_idx] |= lane0_bits << bit_offset;
            dst1[dst_byte_idx] |= lane1_bits << bit_offset;
            dst2[dst_byte_idx] |= lane2_bits << bit_offset;
            dst3[dst_byte_idx] |= lane3_bits << bit_offset;
        }
    }
}

bool SPIQuadSTM32::transmit(TransmitMode mode) {
    if (!mInitialized || !mBufferAcquired) {
        return false;
    }

    // Mode is a hint - platform may block
    (void)mode;

    if (mCurrentTotalSize == 0) {
        return true;  // Nothing to transmit
    }

    // Calculate required buffer size for each lane from interleaved buffer
    // Each byte splits into 2 bits per lane (8 bits / 4 lanes)
    // 4 source bytes = 8 bits per lane = 1 byte per lane
    size_t buffer_size_per_lane = (mCurrentTotalSize + 3) / 4;  // Round up

    if (!allocateDMABuffer(buffer_size_per_lane)) {
        return false;
    }

    // Clear buffers
    fl::memset(mDMABuffer0, 0, buffer_size_per_lane);
    fl::memset(mDMABuffer1, 0, buffer_size_per_lane);
    fl::memset(mDMABuffer2, 0, buffer_size_per_lane);
    fl::memset(mDMABuffer3, 0, buffer_size_per_lane);

    // Interleave bits from mDMABuffer across four lanes
    interleaveBits(mDMABuffer.data(), mCurrentTotalSize,
                   (uint8_t*)mDMABuffer0, (uint8_t*)mDMABuffer1,
                   (uint8_t*)mDMABuffer2, (uint8_t*)mDMABuffer3,
                   buffer_size_per_lane);

    // TODO: Start DMA transfers
    // 1. Configure DMA channel 0 to transfer mDMABuffer0 to GPIO ODR (mData0Pin)
    // 2. Configure DMA channel 1 to transfer mDMABuffer1 to GPIO ODR (mData1Pin)
    // 3. Configure DMA channel 2 to transfer mDMABuffer2 to GPIO ODR (mData2Pin)
    // 4. Configure DMA channel 3 to transfer mDMABuffer3 to GPIO ODR (mData3Pin)
    // 5. Start Timer to trigger all 4 DMA channels at clock rate
    // 6. Set mTransactionActive = true

    FL_WARN("SPIQuadSTM32: DMA transfer not yet implemented");

    // Uncomment when implementation is ready:
    // mTransactionActive = true;
    // return true;

    return false;  // Not yet implemented
}

bool SPIQuadSTM32::waitComplete(uint32_t timeout_ms) {
    if (!mTransactionActive) {
        return true;  // Nothing to wait for
    }

    // TODO: Wait for DMA completion
    // 1. Poll DMA channel status flags for all 4 channels
    // 2. Implement timeout checking
    // 3. Stop Timer when all 4 DMA channels complete
    // 4. Clear interrupt flags

    (void)timeout_ms;  // Unused until implementation

    mTransactionActive = false;

    // AUTO-RELEASE DMA buffer
    mBufferAcquired = false;
    mCurrentTotalSize = 0;

    return true;
}

bool SPIQuadSTM32::isBusy() const {
    if (!mInitialized) {
        return false;
    }

    // TODO: Check DMA channel busy status for all 4 channels
    // return mTransactionActive ||
    //        DMA_channel0_busy || DMA_channel1_busy ||
    //        DMA_channel2_busy || DMA_channel3_busy;

    return mTransactionActive;
}

bool SPIQuadSTM32::isInitialized() const {
    return mInitialized;
}

int SPIQuadSTM32::getBusId() const {
    return mBusId;
}

const char* SPIQuadSTM32::getName() const {
    return mName;
}

void SPIQuadSTM32::cleanup() {
    if (mInitialized) {
        // Wait for any pending transmission
        if (mTransactionActive) {
            waitComplete();
        }

        // TODO: Disable and release hardware resources
        // 1. Stop Timer
        // 2. Disable all 4 DMA channels
        // 3. Reset GPIO pins to default state
        // 4. Disable peripheral clocks

        // Free main DMA buffer
        if (!mDMABuffer.empty()) {
            free(mDMABuffer.data());
            mDMABuffer = fl::span<uint8_t>();
            mMaxBytesPerLane = 0;
            mCurrentTotalSize = 0;
            mBufferAcquired = false;
        }

        // Free legacy DMA buffers
        if (mDMABuffer0 != nullptr) {
            free(mDMABuffer0);
            mDMABuffer0 = nullptr;
        }
        if (mDMABuffer1 != nullptr) {
            free(mDMABuffer1);
            mDMABuffer1 = nullptr;
        }
        if (mDMABuffer2 != nullptr) {
            free(mDMABuffer2);
            mDMABuffer2 = nullptr;
        }
        if (mDMABuffer3 != nullptr) {
            free(mDMABuffer3);
            mDMABuffer3 = nullptr;
        }
        mDMABufferSize = 0;

        mInitialized = false;
    }
}

// ============================================================================
// Static Registration - New Polymorphic Pattern
// ============================================================================

namespace platform {

/// @brief Initialize STM32 SpiHw4 instances
///
/// This function is called lazily by SpiHw4::getAll() on first access.
/// It replaces the old FL_INIT-based static initialization.
void initSpiHw4Instances() {
    // Create logical SPI buses based on available Timer/DMA resources
    // For initial implementation, we provide 2 potential buses
    // Actual availability depends on:
    // - Timer peripherals available (TIM2, TIM3, TIM4, etc.)
    // - DMA channels available (4 per bus)
    // - GPIO pins available

    static auto controller0 = fl::make_shared<SPIQuadSTM32>(0, "QSPI0");
    static auto controller1 = fl::make_shared<SPIQuadSTM32>(1, "QSPI1");

    SpiHw4::registerInstance(controller0);
    SpiHw4::registerInstance(controller1);

    // Additional controllers can be added if hardware supports:
    // Note: STM32F4 has 2 DMA controllers with 8 channels each = 16 total
    // Each quad bus needs 4 channels, so theoretically 4 buses possible
    // static auto controller2 = fl::make_shared<SPIQuadSTM32>(2, "QSPI2");
    // SpiHw4::registerInstance(controller2);
}

}  // namespace platform

}  // namespace fl

#endif  // FL_STM32_HAS_SPI_HW_4

#endif  // FL_IS_STM32
