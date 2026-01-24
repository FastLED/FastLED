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
///
/// Compatible with: STM32F1, STM32F2, STM32F4, STM32F7, STM32L4, STM32H7, STM32G4, STM32U5

// Include platform detection header for FL_IS_STM32
#include "platforms/arm/stm32/is_stm32.h"

// Platform guard using standardized FL_IS_STM32 macro
#if defined(FL_IS_STM32)

// Platform capability macros
#include "platforms/arm/stm32/stm32_capabilities.h"

#ifdef FL_STM32_HAS_SPI_HW_2

// ============================================================================
// Hardware SPI Support Guard
// ============================================================================
// Only compile this hardware implementation if FL_STM32_HAS_SPI_HW_2 is defined.
// If undefined (e.g., on STM32F1 with channel-based DMA), the weak binding
// will cause FastLED to fall back to software bitbang implementation.
//
// FL_STM32_HAS_SPI_HW_2 is defined in stm32_capabilities.h for platforms with:
// - Stream-based DMA (F2/F4/F7/H7/L4)
//
// Not defined for:
// - F1 (channel-based DMA - not yet implemented)
// - G4 (channel-based DMA with DMAMUX - not yet implemented)
// - U5 (GPDMA architecture - not yet implemented)

#include "fl/stl/stdint.h"
#include "fl/numeric_limits.h"
#include <Arduino.h>  // Ensure STM32 HAL is initialized

#include "platforms/shared/spi_hw_2.h"
#include "fl/warn.h"
#include "fl/dbg.h"
#include "fl/stl/cstring.h"

// Allow software-mode testing without hardware Timer/DMA
// Define this to enable software GPIO bitbanging mode for testing
// #define FASTLED_STM32_DUALSPI_SOFTWARE_MODE 1

#include <cstring> // ok include
#include "platforms/shared/spi_bus_manager.h"  // For DMABuffer, TransmitMode, SPIError
#include "platforms/arm/stm32/stm32_gpio_timer_helpers.h"  // Centralized GPIO/Timer/DMA helpers

namespace fl {

// Use centralized STM32 helper functions
using namespace fl::stm32;

// ============================================================================
// SPIDualSTM32 Class Implementation
// ============================================================================

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

    /// @brief Acquire DMA buffer for direct user writes (zero-copy)
    /// @param bytes_per_lane Number of bytes per lane (total = bytes_per_lane * 2)
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

    // Hardware resources
    TIM_TypeDef* mTimer;  ///< Timer peripheral for clock generation
#ifdef HAL_TIM_MODULE_ENABLED
    TIM_HandleTypeDef mTimerHandle;  ///< HAL handle for timer (persistent storage)
#endif
#ifdef FASTLED_STM32_HAS_DMA_STREAMS
    DMA_Stream_TypeDef* mDMAStream0;  ///< DMA stream for lane 0
    DMA_Stream_TypeDef* mDMAStream1;  ///< DMA stream for lane 1
#ifdef HAL_DMA_MODULE_ENABLED
    DMA_HandleTypeDef mDMAHandle0;  ///< HAL handle for DMA stream 0 (persistent storage)
    DMA_HandleTypeDef mDMAHandle1;  ///< HAL handle for DMA stream 1 (persistent storage)
#endif
#endif

    // DMA buffer management (zero-copy API)
    fl::span<uint8_t> mDMABuffer;    ///< Allocated DMA buffer (interleaved format for dual-lane)
    size_t mMaxBytesPerLane;         ///< Max bytes per lane we've allocated for
    size_t mCurrentTotalSize;        ///< Current transmission size (bytes_per_lane * 2)
    bool mBufferAcquired;            ///< True if buffer acquired and ready for transmit

    // Legacy buffers (will be derived from mDMABuffer for lane splitting)
    void* mDMABuffer0;      ///< Buffer for lane 0 (derived from mDMABuffer)
    void* mDMABuffer1;      ///< Buffer for lane 1 (derived from mDMABuffer)
    size_t mDMABufferSize;  ///< Size of each DMA buffer (derived)

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
    , mTimer(nullptr)
#ifdef FASTLED_STM32_HAS_DMA_STREAMS
    , mDMAStream0(nullptr)
    , mDMAStream1(nullptr)
#endif
    , mDMABuffer()
    , mMaxBytesPerLane(0)
    , mCurrentTotalSize(0)
    , mBufferAcquired(false)
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

    // Validate pins using GPIO helper functions
    if (!isValidPin(mClockPin)) {
        FL_WARN("SPIDualSTM32: Invalid clock pin " << static_cast<int>(mClockPin));
        return false;
    }
    if (!isValidPin(mData0Pin)) {
        FL_WARN("SPIDualSTM32: Invalid data0 pin " << static_cast<int>(mData0Pin));
        return false;
    }
    if (!isValidPin(mData1Pin)) {
        FL_WARN("SPIDualSTM32: Invalid data1 pin " << static_cast<int>(mData1Pin));
        return false;
    }

    // Configure GPIO pins
#ifdef HAL_GPIO_MODULE_ENABLED
    // Configure data pins as outputs
    if (!configurePinAsOutput(mData0Pin, GPIO_SPEED_FREQ_HIGH)) {
        FL_WARN("SPIDualSTM32: Failed to configure data0 pin");
        return false;
    }
    if (!configurePinAsOutput(mData1Pin, GPIO_SPEED_FREQ_HIGH)) {
        FL_WARN("SPIDualSTM32: Failed to configure data1 pin");
        return false;
    }

    FL_DBG("SPIDualSTM32: GPIO pins configured successfully");
    FL_DBG("  Clock pin: " << static_cast<int>(mClockPin));
    FL_DBG("  Data0 pin: " << static_cast<int>(mData0Pin));
    FL_DBG("  Data1 pin: " << static_cast<int>(mData1Pin));
#endif

    // Configure Timer for clock generation
#ifdef HAL_TIM_MODULE_ENABLED
    // Select timer based on bus_id
    mTimer = selectTimer(mBusId);
    if (mTimer == nullptr) {
        FL_WARN("SPIDualSTM32: Failed to select timer for bus " << mBusId);
        return false;
    }

    // Initialize timer for PWM clock generation
    if (!initTimerPWM(&mTimerHandle, mTimer, mClockSpeedHz)) {
        FL_WARN("SPIDualSTM32: Failed to initialize timer PWM");
        mTimer = nullptr;
        return false;
    }

    // Configure clock pin as timer alternate function
    if (!configurePinAsTimerAF(mClockPin, mTimer, FASTLED_GPIO_SPEED_MAX)) {
        FL_WARN("SPIDualSTM32: Failed to configure clock pin as timer AF");
        mTimer = nullptr;
        return false;
    }

    FL_DBG("SPIDualSTM32: Timer configured successfully");
    FL_DBG("  Timer: TIM" << ((mTimer == TIM2) ? "2" : (mTimer == TIM3) ? "3" : (mTimer == TIM4) ? "4"
#ifdef FASTLED_STM32_HAS_TIM5
         : (mTimer == TIM5) ? "5"
#endif
         : "?"));
    FL_DBG("  Clock speed: " << mClockSpeedHz << " Hz");
#endif

    // Configure DMA streams for data lanes
#if defined(HAL_DMA_MODULE_ENABLED) && defined(FASTLED_STM32_HAS_DMA_STREAMS)
    // Select DMA streams for both lanes
    mDMAStream0 = getDMAStream(mTimer, mBusId, 0);
    mDMAStream1 = getDMAStream(mTimer, mBusId, 1);

    if (mDMAStream0 == nullptr || mDMAStream1 == nullptr) {
        FL_WARN("SPIDualSTM32: Failed to select DMA streams for bus " << mBusId);
        mTimer = nullptr;
        mDMAStream0 = nullptr;
        mDMAStream1 = nullptr;
        return false;
    }

    // Get DMA channel number for timer update event
    uint32_t dma_channel = getDMAChannel(mTimer);
    if (dma_channel == 0xFF) {
        FL_WARN("SPIDualSTM32: Failed to get DMA channel for timer");
        mTimer = nullptr;
        mDMAStream0 = nullptr;
        mDMAStream1 = nullptr;
        return false;
    }

    // Enable DMA clocks
    enableDMAClock(getDMAController(mDMAStream0));
    enableDMAClock(getDMAController(mDMAStream1));

    FL_DBG("SPIDualSTM32: DMA streams selected successfully");
    FL_DBG("  Stream 0: " << (void*)mDMAStream0);
    FL_DBG("  Stream 1: " << (void*)mDMAStream1);
    FL_DBG("  DMA channel: " << dma_channel);

    // Note: DMA stream configuration (addresses, sizes) happens in transmit()
    // because we need to know the buffer addresses and sizes at that time.
    // Here we just allocate the streams and verify they're available.
#else
    FL_WARN("SPIDualSTM32: DMA not supported on this platform");
    mTimer = nullptr;
    return false;
#endif

    // Hardware initialization complete
    mInitialized = true;
    mTransactionActive = false;

    FL_DBG("SPIDualSTM32: Hardware initialization complete");
    return true;
}

void SPIDualSTM32::end() {
    cleanup();
}

DMABuffer SPIDualSTM32::acquireDMABuffer(size_t bytes_per_lane) {
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

bool SPIDualSTM32::transmit(TransmitMode mode) {
    if (!mInitialized || !mBufferAcquired) {
        return false;
    }

    // Mode is a hint - platform may block
    (void)mode;

    if (mCurrentTotalSize == 0) {
        return true;  // Nothing to transmit
    }

    // Calculate buffer size for each lane from interleaved buffer
    // Each byte splits into 4 bits per lane
    // 2 source bytes = 8 bits per lane = 1 byte per lane
    size_t buffer_size_per_lane = (mCurrentTotalSize + 1) / 2;  // Round up

    if (!allocateDMABuffer(buffer_size_per_lane)) {
        return false;
    }

    // Clear buffers
    fl::memset(mDMABuffer0, 0, buffer_size_per_lane);
    fl::memset(mDMABuffer1, 0, buffer_size_per_lane);

    // Interleave bits from mDMABuffer across two lanes
    interleaveBits(mDMABuffer.data(), mCurrentTotalSize,
                   (uint8_t*)mDMABuffer0, (uint8_t*)mDMABuffer1,
                   buffer_size_per_lane);

    // Start DMA transfers and timer
#if defined(HAL_DMA_MODULE_ENABLED) && defined(FASTLED_STM32_HAS_DMA_STREAMS)
    // Get GPIO ODR register addresses for DMA destination
    GPIO_TypeDef* port0 = getGPIOPort(mData0Pin);
    GPIO_TypeDef* port1 = getGPIOPort(mData1Pin);

    if (port0 == nullptr || port1 == nullptr) {
        FL_WARN("SPIDualSTM32: Failed to get GPIO ports for data pins");
        return false;
    }

    // Get DMA channel number for timer update event
    uint32_t dma_channel = getDMAChannel(mTimer);
    if (dma_channel == 0xFF) {
        FL_WARN("SPIDualSTM32: Failed to get DMA channel for timer");
        return false;
    }

    // Clear any pending DMA flags before starting
    clearDMAFlags(mDMAStream0);
    clearDMAFlags(mDMAStream1);

    // Configure and start DMA stream 0 (lane 0)
    volatile void* odr0 = (volatile void*)&port0->ODR;
    if (!initDMA(mDMAStream0, mDMABuffer0, odr0, buffer_size_per_lane, dma_channel)) {
        FL_WARN("SPIDualSTM32: Failed to initialize DMA stream 0");
        return false;
    }

    // Configure and start DMA stream 1 (lane 1)
    volatile void* odr1 = (volatile void*)&port1->ODR;
    if (!initDMA(mDMAStream1, mDMABuffer1, odr1, buffer_size_per_lane, dma_channel)) {
        FL_WARN("SPIDualSTM32: Failed to initialize DMA stream 1");
        stopDMA(mDMAStream0);  // Stop stream 0 since we're aborting
        return false;
    }

    // Enable DMA requests from timer update event
    TIM_HandleTypeDef htim = {};
    htim.Instance = mTimer;
    __HAL_TIM_ENABLE_DMA(&htim, TIM_DMA_UPDATE);

    // Start timer PWM to trigger DMA transfers
    if (!startTimer(&mTimerHandle)) {
        FL_WARN("SPIDualSTM32: Failed to start timer");
        stopDMA(mDMAStream0);
        stopDMA(mDMAStream1);
        __HAL_TIM_DISABLE_DMA(&htim, TIM_DMA_UPDATE);
        return false;
    }

    FL_DBG("SPIDualSTM32: DMA transmission started");
    FL_DBG("  Buffer size per lane: " << buffer_size_per_lane << " bytes");
    FL_DBG("  Total bytes: " << mCurrentTotalSize);

    mTransactionActive = true;
    return true;
#else
    FL_WARN("SPIDualSTM32: DMA not supported on this platform");
    return false;
#endif
}

bool SPIDualSTM32::waitComplete(uint32_t timeout_ms) {
    if (!mTransactionActive) {
        return true;  // Nothing to wait for
    }

    // Wait for DMA completion with timeout
#if defined(HAL_DMA_MODULE_ENABLED) && defined(FASTLED_STM32_HAS_DMA_STREAMS)
    // Record start time for timeout checking
    uint32_t start_ms = millis();
    bool timeout_enabled = (timeout_ms != fl::numeric_limits<uint32_t>::max());

    // Poll DMA completion flags for both streams
    while (true) {
        bool stream0_complete = isDMAComplete(mDMAStream0);
        bool stream1_complete = isDMAComplete(mDMAStream1);

        // Check if both streams completed
        if (stream0_complete && stream1_complete) {
            FL_DBG("SPIDualSTM32: DMA transfer complete");
            break;
        }

        // Check timeout
        if (timeout_enabled) {
            uint32_t elapsed_ms = millis() - start_ms;
            if (elapsed_ms >= timeout_ms) {
                FL_WARN("SPIDualSTM32: DMA transfer timeout after " << elapsed_ms << " ms");

                // Emergency stop: disable DMA and timer
                stopDMA(mDMAStream0);
                stopDMA(mDMAStream1);
                stopTimer(&mTimerHandle);

                // Disable timer DMA requests
                TIM_HandleTypeDef htim = {};
                htim.Instance = mTimer;
                __HAL_TIM_DISABLE_DMA(&htim, TIM_DMA_UPDATE);

                mTransactionActive = false;
                mBufferAcquired = false;
                mCurrentTotalSize = 0;
                return false;
            }
        }

        // Small delay to avoid busy-waiting too aggressively
        // This allows other tasks to run if RTOS is present
        delayMicroseconds(10);
    }

    // Stop timer (DMA transfers are complete)
    stopTimer(&mTimerHandle);

    // Disable timer DMA requests
    TIM_HandleTypeDef htim = {};
    htim.Instance = mTimer;
    __HAL_TIM_DISABLE_DMA(&htim, TIM_DMA_UPDATE);

    // Clear DMA flags
    clearDMAFlags(mDMAStream0);
    clearDMAFlags(mDMAStream1);

    FL_DBG("SPIDualSTM32: Timer and DMA stopped successfully");

    mTransactionActive = false;

    // AUTO-RELEASE DMA buffer
    mBufferAcquired = false;
    mCurrentTotalSize = 0;

    return true;
#else
    (void)timeout_ms;
    mTransactionActive = false;
    mBufferAcquired = false;
    mCurrentTotalSize = 0;
    return true;
#endif
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

        //  Stop hardware resources in reverse order of initialization
        // 1. Stop DMA streams
        // 2. Stop Timer
        // 3. Reset GPIO pins (optional - not critical)

        // Stop DMA streams if initialized
#ifdef FASTLED_STM32_HAS_DMA_STREAMS
        if (mDMAStream0 != nullptr) {
            stopDMA(mDMAStream0);
            mDMAStream0 = nullptr;
        }
        if (mDMAStream1 != nullptr) {
            stopDMA(mDMAStream1);
            mDMAStream1 = nullptr;
        }
#endif

        // Stop timer if initialized
        if (mTimer != nullptr) {
            stopTimer(&mTimerHandle);
            mTimer = nullptr;
        }

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
        mDMABufferSize = 0;

        mInitialized = false;
    }
}

// ============================================================================
// Static Registration - New Polymorphic Pattern
// ============================================================================

namespace platform {

/// @brief Initialize STM32 SpiHw2 instances
///
/// This function is called lazily by SpiHw2::getAll() on first access.
/// It replaces the old FL_INIT-based static initialization.
void initSpiHw2Instances() {
    // Create logical SPI buses based on available Timer/DMA resources
    // For initial implementation, we provide 2 potential buses
    // Actual availability depends on:
    // - Timer peripherals available (TIM2, TIM3, TIM4, etc.)
    // - DMA channels available (2 per bus)
    // - GPIO pins available

    static auto controller0 = fl::make_shared<SPIDualSTM32>(0, "DSPI0");
    static auto controller1 = fl::make_shared<SPIDualSTM32>(1, "DSPI1");

    SpiHw2::registerInstance(controller0);
    SpiHw2::registerInstance(controller1);

    // Additional controllers can be added if hardware supports:
    // static auto controller2 = fl::make_shared<SPIDualSTM32>(2, "DSPI2");
    // SpiHw2::registerInstance(controller2);
}

}  // namespace platform

}  // namespace fl

#endif  // FL_STM32_HAS_SPI_HW_2

#endif  // FL_IS_STM32
