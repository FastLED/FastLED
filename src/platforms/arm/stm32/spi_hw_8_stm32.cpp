/// @file spi_hw_8_stm32.cpp
/// @brief STM32 implementation of Octal-SPI using GPIO + Timer + DMA
///
/// This file provides the SpiHw8STM32 class and factory for STM32 platforms.
/// Uses Timer for clock generation, GPIO for data outputs, and DMA for parallel transmission.
/// All class definition and implementation is contained in this single file.
///
/// IMPLEMENTATION STATUS:
/// ✅ Class structure complete (inherits from SpiHw8)
/// ✅ All interface methods implemented (begin, end, transmitAsync, waitComplete, etc.)
/// ✅ 8-lane bit interleaving algorithm implemented
/// ✅ DMA buffer management complete
/// ✅ Factory pattern with 2 controller instances
/// ⏳ Hardware initialization pending (Timer/DMA/GPIO HAL calls)
///
/// ARCHITECTURE PATTERN: GPIO + Timer + DMA
/// - Timer (TIM2/TIM3/TIM4): Generates SPI clock signal
/// - GPIO (8 pins): Data lanes D0-D7, driven by DMA
/// - DMA (8 channels): Each channel controls one data pin in parallel
///
/// PLATFORM SUPPORT:
/// - STM32F1 (Blue Pill, etc.) - 50 MHz GPIO, 2 DMA controllers
/// - STM32F4 (F401, F411, F429) - 100 MHz GPIO, 2 DMA controllers
/// - STM32L4 (L432, L476) - 80 MHz GPIO, 2 DMA controllers
/// - STM32H7 (H743, H750) - 100 MHz GPIO, 2 MDMA controllers
/// - STM32G4 (G431, G474) - 170 MHz GPIO, 2 DMA controllers
/// - STM32U5 (U575, U585) - 160 MHz GPIO, 4 GPDMA controllers
///
/// DMA CHANNEL REQUIREMENTS:
/// Each octal-SPI controller requires 8 DMA channels (one per data lane).
/// Most STM32 variants have 16 total DMA channels (2 controllers × 8 channels),
/// so theoretically 2 octal-SPI buses can coexist if no other DMA users exist.
/// In practice, 1 octal-SPI bus + 1 dual/quad-SPI bus is more realistic.

#if defined(STM32F10X_MD) || defined(__STM32F1__) || defined(STM32F1) || \
    defined(STM32F2XX) || defined(STM32F4) || defined(STM32L4) || \
    defined(STM32H7) || defined(STM32G4) || defined(STM32U5)

#include "platforms/shared/spi_hw_8.h"
#include "fl/warn.h"
#include <cstdlib>
#include <cstring>

// TODO: Include STM32 HAL headers when implementing hardware initialization
// #ifdef HAL_TIM_MODULE_ENABLED
// #include "stm32XXxx_hal_tim.h"
// #include "stm32XXxx_hal_dma.h"
// #include "stm32XXxx_hal_gpio.h"
// #endif

namespace fl {

// ============================================================================
// SPIOctalSTM32 Class Definition
// ============================================================================

/// STM32 hardware driver for Octal-SPI DMA transmission using GPIO + Timer + DMA
///
/// Implements SpiHw8 interface for STM32 platforms using:
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

    /// @brief Start non-blocking transmission of data buffer
    /// @param buffer Data to transmit (interleaved across 8 lanes internally)
    /// @return true if transfer started successfully, false on error
    /// @note Waits for previous transaction to complete if still active
    /// @note Returns immediately - use waitComplete() to block until done
    /// @note Each byte is split into 1 bit per lane (8 source bytes → 8 bits per lane)
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

    /// @brief Interleave source data across 8 DMA buffers (one per lane)
    /// @param buffer Source data buffer
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
    void interleaveBits(fl::span<const uint8_t> buffer);

    int mBusId;  ///< Logical bus identifier
    const char* mName;  ///< Controller name

    // Hardware resources (to be initialized)
    void* mTimerHandle;  ///< TIM_HandleTypeDef* (Timer for clock generation)
    void* mDMAHandles[8];  ///< DMA_HandleTypeDef* (8 DMA channels, one per lane)

    // DMA buffers (one per data lane)
    uint8_t* mDMABuffers[8];  ///< 8 separate DMA buffers
    size_t mDMABufferSize;  ///< Size of each DMA buffer (all same size)

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

SPIOctalSTM32::SPIOctalSTM32(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mTimerHandle(nullptr)
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

    // TODO: Hardware initialization
    // This is where STM32 HAL calls would be added for actual hardware setup.
    // The implementation would follow this pattern:
    //
    // 1. Enable peripheral clocks:
    //    __HAL_RCC_TIM2_CLK_ENABLE();  // or TIM3/TIM4
    //    __HAL_RCC_DMA1_CLK_ENABLE();  // and DMA2 if needed
    //    __HAL_RCC_GPIOA_CLK_ENABLE(); // for all GPIO ports used
    //
    // 2. Configure Timer for clock generation:
    //    TIM_HandleTypeDef* htim = new TIM_HandleTypeDef();
    //    htim->Instance = TIM2;  // or TIM3/TIM4
    //    htim->Init.Period = (Timer_Clock / clock_speed_hz) - 1;
    //    htim->Init.Prescaler = 0;
    //    htim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    //    htim->Init.CounterMode = TIM_COUNTERMODE_UP;
    //    HAL_TIM_PWM_Init(htim);
    //    // Configure PWM channel for clock pin
    //    TIM_OC_InitTypeDef sConfigOC = {};
    //    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    //    sConfigOC.Pulse = htim->Init.Period / 2;  // 50% duty cycle
    //    HAL_TIM_PWM_ConfigChannel(htim, &sConfigOC, TIM_CHANNEL_1);
    //    mTimerHandle = htim;
    //
    // 3. Configure GPIO pins:
    //    GPIO_InitTypeDef GPIO_InitStruct = {};
    //    // Clock pin as Timer PWM output (Alternate Function)
    //    GPIO_InitStruct.Pin = GPIO_PIN_x;
    //    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    //    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    //    GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
    //    HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
    //    // Data pins as GPIO outputs (controlled by DMA)
    //    for (int i = 0; i < 8; ++i) {
    //        GPIO_InitStruct.Pin = mDataPins[i];
    //        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    //        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    //        HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
    //    }
    //
    // 4. Configure 8 DMA channels (one per data lane):
    //    for (int i = 0; i < 8; ++i) {
    //        DMA_HandleTypeDef* hdma = new DMA_HandleTypeDef();
    //        hdma->Instance = DMA1_Channel1 + i;  // or appropriate channel
    //        hdma->Init.Direction = DMA_MEMORY_TO_PERIPH;
    //        hdma->Init.PeriphInc = DMA_PINC_DISABLE;
    //        hdma->Init.MemInc = DMA_MINC_ENABLE;
    //        hdma->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    //        hdma->Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    //        hdma->Init.Mode = DMA_NORMAL;
    //        hdma->Init.Priority = DMA_PRIORITY_HIGH;
    //        HAL_DMA_Init(hdma);
    //        // Link DMA to Timer Update event
    //        __HAL_LINKDMA(mTimerHandle, hdma2, hdma);
    //        mDMAHandles[i] = hdma;
    //    }
    //
    // 5. Start Timer (clock generation):
    //    HAL_TIM_PWM_Start(mTimerHandle, TIM_CHANNEL_1);

    FL_WARN("SPIOctalSTM32: Hardware initialization not yet implemented");
    FL_WARN("SPIOctalSTM32: Skeleton complete, awaiting STM32 HAL integration");

    // For now, mark as not initialized until HAL code is added
    // mInitialized = true;
    return false;
}

void SPIOctalSTM32::end() {
    cleanup();
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

void SPIOctalSTM32::interleaveBits(fl::span<const uint8_t> buffer) {
    size_t byte_count = buffer.size();

    // Each source byte contributes 1 bit to each of the 8 lanes
    // 8 source bytes pack into 1 byte per lane
    size_t bytes_per_lane = (byte_count + 7) / 8;  // Ceiling division

    // Clear all DMA buffers
    for (int lane = 0; lane < 8; ++lane) {
        memset(mDMABuffers[lane], 0, mDMABufferSize);
    }

    // Interleave bits across lanes
    for (size_t src_idx = 0; src_idx < byte_count; ++src_idx) {
        uint8_t src_byte = buffer[src_idx];

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

bool SPIOctalSTM32::transmitAsync(fl::span<const uint8_t> buffer) {
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

    // Calculate buffer size needed per lane
    // 8 source bytes → 1 byte per lane
    size_t byte_count = buffer.size();
    size_t bytes_per_lane = (byte_count + 7) / 8;  // Ceiling division

    // Allocate DMA buffers if needed
    if (!allocateDMABuffer(bytes_per_lane)) {
        return false;
    }

    // Interleave source data across 8 lanes
    interleaveBits(buffer);

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

        // Free DMA buffers
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
// Factory Implementation
// ============================================================================

/// STM32 factory override - returns available SPI bus instances
/// Strong definition overrides weak default
fl::vector<SpiHw8*> SpiHw8::createInstances() {
    fl::vector<SpiHw8*> controllers;

    // Create 2 logical octal-SPI buses
    // Note: In practice, STM32 DMA resources may limit this to 1 bus
    // (8 DMA channels per bus × 2 buses = 16 channels = all available on most STM32)
    static SPIOctalSTM32 controller0(0, "OSPI0");
    controllers.push_back(&controller0);

    static SPIOctalSTM32 controller1(1, "OSPI1");
    controllers.push_back(&controller1);

    // Note: If your application doesn't need 2 octal buses, consider:
    // - 1 octal (8 channels) + 1 quad (4 channels) + 1 dual (2 channels) = 14 channels
    // - Or any other combination that fits within your DMA budget

    return controllers;
}

}  // namespace fl

#endif  // STM32 platform check
