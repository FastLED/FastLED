/// @file spi_hw_8_stm32.cpp
/// @brief STM32 implementation of Octal-SPI using GPIO + Timer + DMA
///
/// This file provides the SpiHw8STM32 class and factory for STM32 platforms.
/// Uses Timer for clock generation, GPIO for data outputs, and DMA for parallel transmission.
/// All class definition and implementation is contained in this single file.
///
/// ARCHITECTURE PATTERN: GPIO + Timer + DMA
/// - Timer (TIM2/TIM3/TIM4): Generates SPI clock signal
/// - GPIO (8 pins): Data lanes D0-D7, driven by DMA
/// - DMA (8 channels): Each channel controls one data pin in parallel
///
/// PLATFORM SUPPORT:
/// - STM32F2/F4/F7/H7/L4: Stream-based DMA (hardware accelerated)
/// - STM32F1/G4/U5: Falls back to software bitbang (channel-based DMA not yet implemented)
///
/// DMA CHANNEL REQUIREMENTS:
/// Each octal-SPI controller requires 8 DMA channels (one per data lane).
/// Most STM32 variants have 16 total DMA channels (2 controllers × 8 channels),
/// so theoretically 2 octal-SPI buses can coexist if no other DMA users exist.
/// In practice, 1 octal-SPI bus + 1 dual/quad-SPI bus is more realistic.

// Include platform detection header for FL_IS_STM32
#include "platforms/arm/stm32/is_stm32.h"

// Platform guard using standardized FL_IS_STM32 macro
#if defined(FL_IS_STM32)

// Platform capability macros
#include "platforms/arm/stm32/stm32_capabilities.h"

#ifdef FL_STM32_HAS_SPI_HW_8

// ============================================================================
// Hardware SPI Support Guard
// ============================================================================
// Only compile this hardware implementation if FL_STM32_HAS_SPI_HW_8 is defined.
// If undefined (e.g., on STM32F1 with channel-based DMA), the weak binding
// will cause FastLED to fall back to software bitbang implementation.
//
// FL_STM32_HAS_SPI_HW_8 is defined in stm32_capabilities.h for platforms with:
// - Stream-based DMA (F2/F4/F7/H7/L4)
//
// Not defined for:
// - F1 (channel-based DMA - not yet implemented)
// - G4 (channel-based DMA with DMAMUX - not yet implemented)
// - U5 (GPDMA architecture - not yet implemented)

#include "fl/stl/stdint.h"
#include "fl/numeric_limits.h"
#include <Arduino.h>  // Ensure STM32 HAL is initialized

#include "platforms/shared/spi_hw_8.h"
#include "fl/warn.h"
#include "fl/dbg.h"
#include "fl/stl/cstring.h"
#include <cstring> // ok include
#include "platforms/shared/spi_bus_manager.h"  // For DMABuffer, TransmitMode, SPIError
#include "platforms/arm/stm32/stm32_gpio_timer_helpers.h"  // Centralized GPIO/Timer/DMA helpers

namespace fl {
/// - Timer (PWM mode) for synchronized clock generation
/// - 8 GPIO outputs for data lanes (D0-D7)
/// - 8 DMA channels for parallel, non-blocking data transmission
/// - Configurable clock frequency (typically 1-10 MHz for LED strips)
/// - Full 8-bit parallel output (one byte per clock cycle)
///
/// @note Each instance allocates 1 Timer peripheral and 8 DMA channels
/// @note Data pins can be any GPIO pins (need not be consecutive)
/// @note Highest throughput mode - outputs full bytes in parallel
/// @note DMA resource intensive - most practical for 1 octal-SPI bus per system
class SPIOctalSTM32 : public SpiHw8 {
public:
    /// @brief Construct a new SPIOctalSTM32 controller
    /// @param bus_id Logical bus identifier (0 or 1)
    /// @param name Human-readable name for this controller
    explicit SPIOctalSTM32(int bus_id = -1, const char* name = "Unknown");

    /// @brief Destroy the controller and release all resources
    ~SPIOctalSTM32();

    /// @brief Initialize the SPI controller with specified configuration
    /// @param config Configuration including all 8 data pins, clock pin, and speed
    /// @return true if initialization successful, false on error
    /// @note Validates all 8 data pins are assigned
    /// @note Allocates Timer/DMA/GPIO resources
    bool begin(const SpiHw8::Config& config) override;

    /// @brief Deinitialize the controller and release resources
    void end() override;

    /// @brief Acquire DMA buffer for direct user writes (zero-copy)
    /// @param bytes_per_lane Number of bytes per lane (total = bytes_per_lane * 8)
    /// @return DMABuffer containing buffer span or error
    /// @note Auto-waits if previous transmission still active
    DMABuffer acquireDMABuffer(size_t bytes_per_lane) override;

    /// @brief Start non-blocking transmission of previously acquired DMA buffer
    /// @return true if transfer started successfully, false on error
    /// @note Must call acquireDMABuffer() first
    /// @note Returns immediately - use waitComplete() to block until done
    /// @note Each byte is split into 1 bit per lane (8 source bytes → 8 bits per lane)
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
    /// @return Bus ID (0 or 1)
    int getBusId() const override;

    /// @brief Get the human-readable name for this controller
    /// @return Controller name string
    const char* getName() const override;

private:
    /// @brief Release all allocated resources (Timer, DMA, GPIO, buffers)
    void cleanup();

    /// @brief Allocate or resize internal DMA buffers for all 8 lanes
    /// @param required_size Size needed per lane in bytes
    /// @return true if all buffers allocated successfully
    bool allocateDMABuffer(size_t required_size);

    /// @brief Interleave source data from mDMABuffer across 8 DMA buffers (one per lane)
    ///
    /// Each source byte contributes 1 bit to each of 8 lanes:
    /// - Lane 0 receives bit 7 from each byte
    /// - Lane 1 receives bit 6 from each byte
    /// - Lane 2 receives bit 5 from each byte
    /// - Lane 3 receives bit 4 from each byte
    /// - Lane 4 receives bit 3 from each byte
    /// - Lane 5 receives bit 2 from each byte
    /// - Lane 6 receives bit 1 from each byte
    /// - Lane 7 receives bit 0 from each byte
    ///
    /// Example: 8 source bytes [0xB4, 0x7E, 0x2A, 0x5C, 0x91, 0xD3, 0x68, 0xF0]
    /// → Lane 0: 0b10011101 (bits [7,7,7,7,7,7,7,7])
    /// → Lane 1: 0b01110100 (bits [6,6,6,6,6,6,6,6])
    /// → ...
    /// → Lane 7: 0b00010100 (bits [0,0,0,0,0,0,0,0])
    ///
    /// Packing: 8 source bytes → 1 byte per lane
    void interleaveBits();

    int mBusId;  ///< Logical bus identifier
    const char* mName;  ///< Controller name

    // Hardware resources (to be initialized)
    void* mTimerHandle;  ///< TIM_HandleTypeDef* (Timer for clock generation)
    void* mDMAHandles[8];  ///< DMA_HandleTypeDef* (8 DMA channels, one per lane)

    // DMA buffer management (zero-copy API)
    fl::span<uint8_t> mDMABuffer;    ///< Allocated DMA buffer (interleaved format for octal-lane)
    size_t mMaxBytesPerLane;         ///< Max bytes per lane we've allocated for
    size_t mCurrentTotalSize;        ///< Current transmission size (bytes_per_lane * 8)
    bool mBufferAcquired;            ///< True if buffer acquired and ready for transmit

    // Legacy DMA buffers (one per data lane, derived from mDMABuffer for lane splitting)
    uint8_t* mDMABuffers[8];  ///< 8 separate DMA buffers (derived)
    size_t mDMABufferSize;  ///< Size of each DMA buffer (all same size, derived)

    // State
    bool mTransactionActive;  ///< True if DMA transfer in progress
    bool mInitialized;  ///< True if hardware initialized

    // Configuration
    uint8_t mClockPin;  ///< Clock output pin (Timer PWM)
    uint8_t mDataPins[8];  ///< All 8 data pins (D0-D7)
    uint32_t mClockSpeedHz;  ///< Configured clock frequency

    SPIOctalSTM32(const SPIOctalSTM32&) = delete;
    SPIOctalSTM32& operator=(const SPIOctalSTM32&) = delete;
};

// ============================================================================
// SPIOctalSTM32 Implementation
// ============================================================================

using namespace fl::stm32;  // For GPIO/Timer/DMA helper functions

SPIOctalSTM32::SPIOctalSTM32(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mTimerHandle(nullptr)
    , mDMABuffer()
    , mMaxBytesPerLane(0)
    , mCurrentTotalSize(0)
    , mBufferAcquired(false)
    , mDMABufferSize(0)
    , mTransactionActive(false)
    , mInitialized(false)
    , mClockPin(0)
    , mClockSpeedHz(0) {
    // Initialize all DMA handles and buffers to nullptr
    for (int i = 0; i < 8; ++i) {
        mDMAHandles[i] = nullptr;
        mDMABuffers[i] = nullptr;
        mDataPins[i] = 0;
    }
}

SPIOctalSTM32::~SPIOctalSTM32() {
    cleanup();
}

bool SPIOctalSTM32::begin(const SpiHw8::Config& config) {
    if (mInitialized) {
        return true;  // Already initialized
    }

    // Validate bus_num against mBusId if driver has pre-assigned ID
    if (mBusId != -1 && config.bus_num != static_cast<uint8_t>(mBusId)) {
        FL_WARN("SPIOctalSTM32: Bus ID mismatch");
        return false;
    }

    // Validate pin assignments - all 8 data pins and clock must be set
    if (config.clock_pin < 0 || config.data0_pin < 0 || config.data1_pin < 0 ||
        config.data2_pin < 0 || config.data3_pin < 0 || config.data4_pin < 0 ||
        config.data5_pin < 0 || config.data6_pin < 0 || config.data7_pin < 0) {
        FL_WARN("SPIOctalSTM32: Invalid pin configuration (all 8 data pins + clock required)");
        return false;
    }

    // Store pin configuration
    mClockPin = config.clock_pin;
    mDataPins[0] = config.data0_pin;
    mDataPins[1] = config.data1_pin;
    mDataPins[2] = config.data2_pin;
    mDataPins[3] = config.data3_pin;
    mDataPins[4] = config.data4_pin;
    mDataPins[5] = config.data5_pin;
    mDataPins[6] = config.data6_pin;
    mDataPins[7] = config.data7_pin;
    mClockSpeedHz = config.clock_speed_hz;

    // Validate all pins using GPIO helper functions
    if (!isValidPin(mClockPin)) {
        FL_WARN("SPIOctalSTM32: Invalid clock pin " << static_cast<int>(mClockPin));
        return false;
    }
    for (int i = 0; i < 8; ++i) {
        if (!isValidPin(mDataPins[i])) {
            FL_WARN("SPIOctalSTM32: Invalid data pin " << i << ": " << static_cast<int>(mDataPins[i]));
            return false;
        }
    }

    // Configure GPIO pins
#ifdef HAL_GPIO_MODULE_ENABLED
    // Configure all 8 data pins as outputs
    for (int i = 0; i < 8; ++i) {
        if (!configurePinAsOutput(mDataPins[i], GPIO_SPEED_FREQ_HIGH)) {
            FL_WARN("SPIOctalSTM32: Failed to configure data pin " << i);
            return false;
        }
    }

    FL_DBG("SPIOctalSTM32: GPIO pins configured successfully");
    FL_DBG("  Clock pin: " << static_cast<int>(mClockPin));
    FL_DBG("  Data pins: " << static_cast<int>(mDataPins[0]) << ", " << static_cast<int>(mDataPins[1]) << ", "
                           << static_cast<int>(mDataPins[2]) << ", " << static_cast<int>(mDataPins[3]) << ", "
                           << static_cast<int>(mDataPins[4]) << ", " << static_cast<int>(mDataPins[5]) << ", "
                           << static_cast<int>(mDataPins[6]) << ", " << static_cast<int>(mDataPins[7]));
#endif

    // TODO: Implement remaining hardware initialization
    // 1. Configure Timer for clock generation on mClockPin
    //    - Enable Timer clock (e.g., __HAL_RCC_TIM2_CLK_ENABLE())
    //    - Set prescaler and ARR based on mClockSpeedHz
    //    - Configure Output Compare/PWM mode
    //    - Configure mClockPin as Timer alternate function
    // 2. Configure 8 DMA channels
    //    - Enable DMA clock (e.g., __HAL_RCC_DMA1_CLK_ENABLE())
    //    - DMA channels 0-7: Memory -> GPIO ODR for mDataPins[0-7]
    //    - Link all DMA channels to Timer Update event
    // 3. Start Timer

    FL_WARN("SPIOctalSTM32: Timer/DMA initialization not yet implemented");
    FL_WARN("SPIOctalSTM32: GPIO configuration complete - hardware integration not complete");

    // Uncomment when Timer/DMA implementation is ready:
    // mInitialized = true;
    // mTransactionActive = false;
    // return true;

    return false;  // Timer/DMA not yet implemented
}

void SPIOctalSTM32::end() {
    cleanup();
}

DMABuffer SPIOctalSTM32::acquireDMABuffer(size_t bytes_per_lane) {
    if (!mInitialized) {
        return DMABuffer(SPIError::NOT_INITIALIZED);
    }

    // Auto-wait if previous transmission still active
    if (mTransactionActive) {
        if (!waitComplete()) {
            return DMABuffer(SPIError::BUSY);
        }
    }

    // For octal-lane SPI: total size = bytes_per_lane × 8 (interleaved)
    constexpr size_t num_lanes = 8;
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

bool SPIOctalSTM32::allocateDMABuffer(size_t required_size) {
    if (mDMABufferSize >= required_size) {
        return true;  // Buffers are already large enough
    }

    // Free old buffers if they exist
    for (int i = 0; i < 8; ++i) {
        if (mDMABuffers[i] != nullptr) {
            free(mDMABuffers[i]);
            mDMABuffers[i] = nullptr;
        }
    }
    mDMABufferSize = 0;

    // Allocate new buffers for all 8 lanes (word-aligned for DMA)
    for (int i = 0; i < 8; ++i) {
        mDMABuffers[i] = (uint8_t*)malloc(required_size);
        if (mDMABuffers[i] == nullptr) {
            FL_WARN("SPIOctalSTM32: Failed to allocate DMA buffer for lane " << i);
            // Free any buffers that were successfully allocated
            for (int j = 0; j < i; ++j) {
                free(mDMABuffers[j]);
                mDMABuffers[j] = nullptr;
            }
            return false;
        }
    }

    mDMABufferSize = required_size;
    return true;
}

void SPIOctalSTM32::interleaveBits() {
    size_t byte_count = mCurrentTotalSize;

    // Each source byte contributes 1 bit to each of the 8 lanes
    // 8 source bytes pack into 1 byte per lane
    size_t bytes_per_lane = (byte_count + 7) / 8;  // Ceiling division

    // Clear all DMA buffers
    for (int lane = 0; lane < 8; ++lane) {
        fl::memset(mDMABuffers[lane], 0, mDMABufferSize);
    }

    // Interleave bits from mDMABuffer across lanes
    for (size_t src_idx = 0; src_idx < byte_count; ++src_idx) {
        uint8_t src_byte = mDMABuffer[src_idx];

        // Which output byte index in each lane buffer
        size_t lane_byte_idx = src_idx / 8;
        // Which bit position within that byte (0-7)
        uint8_t bit_pos = 7 - (src_idx % 8);

        // Extract and distribute each bit to its corresponding lane
        // Lane 0 gets bit 7, Lane 1 gets bit 6, ..., Lane 7 gets bit 0
        for (int lane = 0; lane < 8; ++lane) {
            uint8_t bit = (src_byte >> (7 - lane)) & 0x01;
            mDMABuffers[lane][lane_byte_idx] |= (bit << bit_pos);
        }
    }
}

bool SPIOctalSTM32::transmit(TransmitMode mode) {
    if (!mInitialized || !mBufferAcquired) {
        return false;
    }

    // Mode is a hint - platform may block
    (void)mode;

    if (mCurrentTotalSize == 0) {
        return true;  // Nothing to transmit
    }

    // Calculate buffer size needed per lane from interleaved buffer
    // 8 source bytes → 1 byte per lane
    size_t bytes_per_lane = (mCurrentTotalSize + 7) / 8;  // Ceiling division

    // Allocate DMA buffers if needed
    if (!allocateDMABuffer(bytes_per_lane)) {
        return false;
    }

    // Interleave source data from mDMABuffer across 8 lanes
    interleaveBits();

    // TODO: Start DMA transfers on all 8 channels
    // This is where STM32 HAL DMA start calls would be added.
    // The implementation would follow this pattern:
    //
    // for (int i = 0; i < 8; ++i) {
    //     DMA_HandleTypeDef* hdma = (DMA_HandleTypeDef*)mDMAHandles[i];
    //     // Configure DMA transfer from buffer to GPIO ODR
    //     HAL_DMA_Start(hdma,
    //                   (uint32_t)mDMABuffers[i],  // Source: DMA buffer
    //                   (uint32_t)&GPIOx->ODR,     // Destination: GPIO output register
    //                   bytes_per_lane);           // Transfer count
    // }
    //
    // Note: DMA should be triggered by Timer Update event, so data is
    // output synchronously with the clock signal generated by the Timer.

    mTransactionActive = true;
    return true;
}

bool SPIOctalSTM32::waitComplete(uint32_t timeout_ms) {
    if (!mTransactionActive) {
        return true;  // Nothing to wait for
    }

    // TODO: Poll all 8 DMA channels for completion
    // This is where STM32 HAL DMA polling would be added.
    // The implementation would follow this pattern:
    //
    // if (timeout_ms == UINT32_MAX) {
    //     // Infinite timeout - just wait for all channels
    //     for (int i = 0; i < 8; ++i) {
    //         DMA_HandleTypeDef* hdma = (DMA_HandleTypeDef*)mDMAHandles[i];
    //         HAL_DMA_PollForTransfer(hdma, HAL_DMA_FULL_TRANSFER, HAL_MAX_DELAY);
    //     }
    // } else {
    //     // Timeout-based polling
    //     uint32_t start_tick = HAL_GetTick();
    //     bool all_complete = false;
    //     while (!all_complete && (HAL_GetTick() - start_tick) < timeout_ms) {
    //         all_complete = true;
    //         for (int i = 0; i < 8; ++i) {
    //             DMA_HandleTypeDef* hdma = (DMA_HandleTypeDef*)mDMAHandles[i];
    //             if (HAL_DMA_GetState(hdma) != HAL_DMA_STATE_READY) {
    //                 all_complete = false;
    //                 break;
    //             }
    //         }
    //     }
    //     if (!all_complete) {
    //         return false;  // Timeout
    //     }
    // }

    mTransactionActive = false;

    // AUTO-RELEASE DMA buffer
    mBufferAcquired = false;
    mCurrentTotalSize = 0;

    return true;
}

bool SPIOctalSTM32::isBusy() const {
    if (!mInitialized) {
        return false;
    }

    // TODO: Check DMA status on all 8 channels
    // This is where STM32 HAL DMA status check would be added.
    // The implementation would be:
    //
    // if (mTransactionActive) {
    //     for (int i = 0; i < 8; ++i) {
    //         DMA_HandleTypeDef* hdma = (DMA_HandleTypeDef*)mDMAHandles[i];
    //         if (HAL_DMA_GetState(hdma) != HAL_DMA_STATE_READY) {
    //             return true;  // At least one channel still busy
    //         }
    //     }
    //     return false;  // All channels idle
    // }

    return mTransactionActive;
}

bool SPIOctalSTM32::isInitialized() const {
    return mInitialized;
}

int SPIOctalSTM32::getBusId() const {
    return mBusId;
}

const char* SPIOctalSTM32::getName() const {
    return mName;
}

void SPIOctalSTM32::cleanup() {
    if (mInitialized) {
        // Wait for any pending transmission
        if (mTransactionActive) {
            waitComplete();
        }

        // TODO: Stop Timer and DMA channels
        // This is where STM32 HAL cleanup calls would be added:
        //
        // // Stop Timer PWM
        // if (mTimerHandle != nullptr) {
        //     TIM_HandleTypeDef* htim = (TIM_HandleTypeDef*)mTimerHandle;
        //     HAL_TIM_PWM_Stop(htim, TIM_CHANNEL_1);
        //     HAL_TIM_PWM_DeInit(htim);
        //     delete htim;
        //     mTimerHandle = nullptr;
        // }
        //
        // // Stop and deinitialize all DMA channels
        // for (int i = 0; i < 8; ++i) {
        //     if (mDMAHandles[i] != nullptr) {
        //         DMA_HandleTypeDef* hdma = (DMA_HandleTypeDef*)mDMAHandles[i];
        //         HAL_DMA_Abort(hdma);
        //         HAL_DMA_DeInit(hdma);
        //         delete hdma;
        //         mDMAHandles[i] = nullptr;
        //     }
        // }
        //
        // // Reset GPIO pins to input mode
        // for (int i = 0; i < 8; ++i) {
        //     GPIO_InitTypeDef GPIO_InitStruct = {};
        //     GPIO_InitStruct.Pin = mDataPins[i];
        //     GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        //     HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
        // }

        // Free main DMA buffer
        if (!mDMABuffer.empty()) {
            free(mDMABuffer.data());
            mDMABuffer = fl::span<uint8_t>();
            mMaxBytesPerLane = 0;
            mCurrentTotalSize = 0;
            mBufferAcquired = false;
        }

        // Free legacy DMA buffers
        for (int i = 0; i < 8; ++i) {
            if (mDMABuffers[i] != nullptr) {
                free(mDMABuffers[i]);
                mDMABuffers[i] = nullptr;
            }
        }
        mDMABufferSize = 0;

        mInitialized = false;
    }
}

// ============================================================================
// Static Registration - New Polymorphic Pattern
// ============================================================================

namespace platform {

/// @brief Initialize STM32 SpiHw8 instances
///
/// This function is called lazily by SpiHw8::getAll() on first access.
/// It replaces the old FL_INIT-based static initialization.
void initSpiHw8Instances() {
    // Create 2 logical octal-SPI buses
    // Note: In practice, STM32 DMA resources may limit this to 1 bus
    // (8 DMA channels per bus × 2 buses = 16 channels = all available on most STM32)
    static auto controller0 = fl::make_shared<SPIOctalSTM32>(0, "OSPI0");
    static auto controller1 = fl::make_shared<SPIOctalSTM32>(1, "OSPI1");

    SpiHw8::registerInstance(controller0);
    SpiHw8::registerInstance(controller1);

    // Note: If your application doesn't need 2 octal buses, consider:
    // - 1 octal (8 channels) + 1 quad (4 channels) + 1 dual (2 channels) = 14 channels
    // - Or any other combination that fits within your DMA budget
}

}  // namespace platform

}  // namespace fl

#endif  // FL_STM32_HAS_SPI_HW_8

#endif  // FL_IS_STM32
