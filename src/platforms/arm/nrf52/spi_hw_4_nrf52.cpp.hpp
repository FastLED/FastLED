/// @file spi_hw_4_nrf52.cpp
/// @brief NRF52840 implementation of Quad-SPI
///
/// This file provides the SPIQuadNRF52 class implementation for Nordic nRF52840/52833 platforms.
/// All implementation is contained in this single file.

// Only compile on nRF52840/52833 (requires SPIM3 for quad-lane operation)
#if defined(NRF52840) || defined(NRF52833)

#include "spi_hw_4_nrf52.h"
#include "fl/stl/cstring.h"
#include "fl/numeric_limits.h"
#include "platforms/shared/spi_bus_manager.h"  // For DMABuffer, TransmitMode, SPIError

namespace fl {

// ============================================================================
// SPIQuadNRF52 Implementation
// ============================================================================

SPIQuadNRF52::SPIQuadNRF52(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mSPIM0(NRF_SPIM0)
    , mSPIM1(NRF_SPIM1)
    , mSPIM2(NRF_SPIM2)
    , mSPIM3(NRF_SPIM3)
    , mTimer(NRF_TIMER1)  // Use TIMER1 (TIMER0 reserved for dual-SPI)
    , mDMABuffer()
    , mMaxBytesPerLane(0)
    , mCurrentTotalSize(0)
    , mBufferAcquired(false)
    , mLane0Buffer(nullptr)
    , mLane1Buffer(nullptr)
    , mLane2Buffer(nullptr)
    , mLane3Buffer(nullptr)
    , mBufferSize(0)
    , mTransactionActive(false)
    , mInitialized(false)
    , mClockPin(0)
    , mData0Pin(0)
    , mData1Pin(0)
    , mData2Pin(0)
    , mData3Pin(0)
    , mClockSpeed(0)
    , mPPIChannel4(4)
    , mPPIChannel5(5)
    , mPPIChannel6(6)
    , mPPIChannel7(7) {
}

SPIQuadNRF52::~SPIQuadNRF52() {
    cleanup();
}

bool SPIQuadNRF52::begin(const SpiHw4::Config& config) {
    if (mInitialized) {
        return true;  // Already initialized
    }

    // Validate bus_num against mBusId if driver has pre-assigned ID
    if (mBusId != -1 && config.bus_num != static_cast<uint8_t>(mBusId)) {
        FL_WARN("SPIQuadNRF52: Bus ID mismatch");
        return false;
    }

    // Validate pin assignments
    if (config.clock_pin < 0 || config.data0_pin < 0 || config.data1_pin < 0 ||
        config.data2_pin < 0 || config.data3_pin < 0) {
        FL_WARN("SPIQuadNRF52: Invalid pin configuration");
        return false;
    }

    // Store configuration
    mClockPin = config.clock_pin;
    mData0Pin = config.data0_pin;
    mData1Pin = config.data1_pin;
    mData2Pin = config.data2_pin;
    mData3Pin = config.data3_pin;
    mClockSpeed = config.clock_speed_hz;

    // Configure GPIO pins as outputs
    nrf_gpio_cfg_output(mClockPin);
    nrf_gpio_cfg_output(mData0Pin);
    nrf_gpio_cfg_output(mData1Pin);
    nrf_gpio_cfg_output(mData2Pin);
    nrf_gpio_cfg_output(mData3Pin);

    // Set initial pin states (low)
    nrf_gpio_pin_clear(mClockPin);
    nrf_gpio_pin_clear(mData0Pin);
    nrf_gpio_pin_clear(mData1Pin);
    nrf_gpio_pin_clear(mData2Pin);
    nrf_gpio_pin_clear(mData3Pin);

    // Set frequency based on clock speed
    // Map requested frequency to nearest Nordic frequency constant
    nrf_spim_frequency_t freq;
    if (config.clock_speed_hz >= 8000000) {
        freq = NRF_SPIM_FREQ_8M;
    } else if (config.clock_speed_hz >= 4000000) {
        freq = NRF_SPIM_FREQ_4M;
    } else if (config.clock_speed_hz >= 2000000) {
        freq = NRF_SPIM_FREQ_2M;
    } else if (config.clock_speed_hz >= 1000000) {
        freq = NRF_SPIM_FREQ_1M;
    } else if (config.clock_speed_hz >= 500000) {
        freq = NRF_SPIM_FREQ_500K;
    } else if (config.clock_speed_hz >= 250000) {
        freq = NRF_SPIM_FREQ_250K;
    } else {
        freq = NRF_SPIM_FREQ_125K;
    }

    // Configure SPIM0 (lane 0)
    nrf_spim_pins_set(mSPIM0, mClockPin, mData0Pin, NRF_SPIM_PIN_NOT_CONNECTED);
    nrf_spim_configure(mSPIM0, NRF_SPIM_MODE_0, NRF_SPIM_BIT_ORDER_MSB_FIRST);
    nrf_spim_frequency_set(mSPIM0, freq);
    nrf_spim_event_clear(mSPIM0, NRF_SPIM_EVENT_END);
    nrf_spim_event_clear(mSPIM0, NRF_SPIM_EVENT_STARTED);
    nrf_spim_enable(mSPIM0);

    // Configure SPIM1 (lane 1)
    nrf_spim_pins_set(mSPIM1, mClockPin, mData1Pin, NRF_SPIM_PIN_NOT_CONNECTED);
    nrf_spim_configure(mSPIM1, NRF_SPIM_MODE_0, NRF_SPIM_BIT_ORDER_MSB_FIRST);
    nrf_spim_frequency_set(mSPIM1, freq);
    nrf_spim_event_clear(mSPIM1, NRF_SPIM_EVENT_END);
    nrf_spim_event_clear(mSPIM1, NRF_SPIM_EVENT_STARTED);
    nrf_spim_enable(mSPIM1);

    // Configure SPIM2 (lane 2)
    nrf_spim_pins_set(mSPIM2, mClockPin, mData2Pin, NRF_SPIM_PIN_NOT_CONNECTED);
    nrf_spim_configure(mSPIM2, NRF_SPIM_MODE_0, NRF_SPIM_BIT_ORDER_MSB_FIRST);
    nrf_spim_frequency_set(mSPIM2, freq);
    nrf_spim_event_clear(mSPIM2, NRF_SPIM_EVENT_END);
    nrf_spim_event_clear(mSPIM2, NRF_SPIM_EVENT_STARTED);
    nrf_spim_enable(mSPIM2);

    // Configure SPIM3 (lane 3)
    nrf_spim_pins_set(mSPIM3, mClockPin, mData3Pin, NRF_SPIM_PIN_NOT_CONNECTED);
    nrf_spim_configure(mSPIM3, NRF_SPIM_MODE_0, NRF_SPIM_BIT_ORDER_MSB_FIRST);
    nrf_spim_frequency_set(mSPIM3, freq);
    nrf_spim_event_clear(mSPIM3, NRF_SPIM_EVENT_END);
    nrf_spim_event_clear(mSPIM3, NRF_SPIM_EVENT_STARTED);
    nrf_spim_enable(mSPIM3);

    // Configure hardware synchronization: TIMER + PPI
    configureTimer(mClockSpeed);
    configurePPI();

    mInitialized = true;
    mTransactionActive = false;

    return true;
}

void SPIQuadNRF52::end() {
    cleanup();
}

DMABuffer SPIQuadNRF52::acquireDMABuffer(size_t bytes_per_lane) {
    if (!mInitialized) {
        return SPIError::NOT_INITIALIZED;
    }

    // Auto-wait if previous transmission still active
    if (mTransactionActive) {
        if (!waitComplete()) {
            return SPIError::BUSY;
        }
    }

    // For quad-lane SPI: total size = bytes_per_lane × 4 (interleaved)
    constexpr size_t num_lanes = 4;
    const size_t total_size = bytes_per_lane * num_lanes;

    // Validate size against platform max (256KB for embedded systems)
    constexpr size_t MAX_SIZE = 256 * 1024;
    if (total_size > MAX_SIZE) {
        return SPIError::BUFFER_TOO_LARGE;
    }

    // Reallocate buffer only if we need more capacity
    if (bytes_per_lane > mMaxBytesPerLane) {
        if (!mDMABuffer.empty()) {
            free(mDMABuffer.data());
            mDMABuffer = fl::span<uint8_t>();
        }

        // Allocate DMA-capable memory (regular malloc for NRF52)
        uint8_t* ptr = static_cast<uint8_t*>(malloc(total_size));
        if (!ptr) {
            return SPIError::ALLOCATION_FAILED;
        }

        mDMABuffer = fl::span<uint8_t>(ptr, total_size);
        mMaxBytesPerLane = bytes_per_lane;
    }

    mBufferAcquired = true;
    mCurrentTotalSize = total_size;

    // Return span of current size (not max allocated size)
    return fl::span<uint8_t>(mDMABuffer.data(), total_size);
}

bool SPIQuadNRF52::allocateDMABuffers(size_t required_size) {
    if (mBufferSize >= required_size) {
        return true;  // Buffers are already large enough
    }

    // Free old buffers if they exist
    if (mLane0Buffer != nullptr) {
        free(mLane0Buffer);
        mLane0Buffer = nullptr;
    }
    if (mLane1Buffer != nullptr) {
        free(mLane1Buffer);
        mLane1Buffer = nullptr;
    }
    if (mLane2Buffer != nullptr) {
        free(mLane2Buffer);
        mLane2Buffer = nullptr;
    }
    if (mLane3Buffer != nullptr) {
        free(mLane3Buffer);
        mLane3Buffer = nullptr;
    }
    mBufferSize = 0;

    // Allocate new buffers in RAM (required for EasyDMA)
    mLane0Buffer = (uint8_t*)malloc(required_size);
    if (mLane0Buffer == nullptr) {
        FL_WARN("SPIQuadNRF52: Failed to allocate lane 0 DMA buffer");
        return false;
    }

    mLane1Buffer = (uint8_t*)malloc(required_size);
    if (mLane1Buffer == nullptr) {
        FL_WARN("SPIQuadNRF52: Failed to allocate lane 1 DMA buffer");
        free(mLane0Buffer);
        mLane0Buffer = nullptr;
        return false;
    }

    mLane2Buffer = (uint8_t*)malloc(required_size);
    if (mLane2Buffer == nullptr) {
        FL_WARN("SPIQuadNRF52: Failed to allocate lane 2 DMA buffer");
        free(mLane0Buffer);
        free(mLane1Buffer);
        mLane0Buffer = nullptr;
        mLane1Buffer = nullptr;
        return false;
    }

    mLane3Buffer = (uint8_t*)malloc(required_size);
    if (mLane3Buffer == nullptr) {
        FL_WARN("SPIQuadNRF52: Failed to allocate lane 3 DMA buffer");
        free(mLane0Buffer);
        free(mLane1Buffer);
        free(mLane2Buffer);
        mLane0Buffer = nullptr;
        mLane1Buffer = nullptr;
        mLane2Buffer = nullptr;
        return false;
    }

    mBufferSize = required_size;
    return true;
}

bool SPIQuadNRF52::transmit(TransmitMode mode) {
    if (!mInitialized || !mBufferAcquired) {
        return false;
    }

    // Mode is a hint - platform may block
    (void)mode;

    if (mCurrentTotalSize == 0) {
        return true;  // Nothing to transmit
    }

    // Calculate bytes per lane from total size
    constexpr size_t num_lanes = 4;
    size_t bytes_per_lane = mCurrentTotalSize / num_lanes;

    if (bytes_per_lane == 0) {
        return true;  // Empty buffer
    }

    // Allocate lane buffers for hardware (SPIM requires separate buffers per peripheral)
    if (!allocateDMABuffers(bytes_per_lane)) {
        return false;
    }

    // De-interleave mDMABuffer into lane-specific buffers
    // mDMABuffer contains interleaved data: quarters for each lane
    fl::memcpy(mLane0Buffer, mDMABuffer.data(), bytes_per_lane);
    fl::memcpy(mLane1Buffer, mDMABuffer.data() + bytes_per_lane, bytes_per_lane);
    fl::memcpy(mLane2Buffer, mDMABuffer.data() + (bytes_per_lane * 2), bytes_per_lane);
    fl::memcpy(mLane3Buffer, mDMABuffer.data() + (bytes_per_lane * 3), bytes_per_lane);

    // Configure SPIM0 transmission (lane 0)
    nrf_spim_tx_buffer_set(mSPIM0, mLane0Buffer, bytes_per_lane);
    nrf_spim_rx_buffer_set(mSPIM0, nullptr, 0);  // TX only

    // Configure SPIM1 transmission (lane 1)
    nrf_spim_tx_buffer_set(mSPIM1, mLane1Buffer, bytes_per_lane);
    nrf_spim_rx_buffer_set(mSPIM1, nullptr, 0);  // TX only

    // Configure SPIM2 transmission (lane 2)
    nrf_spim_tx_buffer_set(mSPIM2, mLane2Buffer, bytes_per_lane);
    nrf_spim_rx_buffer_set(mSPIM2, nullptr, 0);  // TX only

    // Configure SPIM3 transmission (lane 3)
    nrf_spim_tx_buffer_set(mSPIM3, mLane3Buffer, bytes_per_lane);
    nrf_spim_rx_buffer_set(mSPIM3, nullptr, 0);  // TX only

    // Start synchronized transmission via TIMER + PPI
    // This triggers all four SPIM START tasks simultaneously
    startTransmission();

    mTransactionActive = true;
    return true;
}

bool SPIQuadNRF52::waitComplete(uint32_t timeout_ms) {
    if (!mTransactionActive) {
        return true;  // Nothing to wait for
    }

    // Use a simple timeout mechanism based on loop iterations
    // For more accurate timing, could use TIMER peripheral or system tick counter
    uint32_t timeout_iterations = timeout_ms * 1000;  // Rough approximation
    uint32_t iterations = 0;

    // Wait for all SPIM peripherals to complete
    while ((!nrf_spim_event_check(mSPIM0, NRF_SPIM_EVENT_END) ||
            !nrf_spim_event_check(mSPIM1, NRF_SPIM_EVENT_END) ||
            !nrf_spim_event_check(mSPIM2, NRF_SPIM_EVENT_END) ||
            !nrf_spim_event_check(mSPIM3, NRF_SPIM_EVENT_END)) &&
           (timeout_ms == fl::numeric_limits<uint32_t>::max() || iterations < timeout_iterations)) {
        iterations++;
    }

    // Check if we timed out
    bool timed_out = (timeout_ms != fl::numeric_limits<uint32_t>::max()) && (iterations >= timeout_iterations);
    if (timed_out) {
        FL_WARN("SPIQuadNRF52: Transaction timeout");
        // Clear state even on timeout
        mTransactionActive = false;
        return false;
    }

    // Clear events
    nrf_spim_event_clear(mSPIM0, NRF_SPIM_EVENT_END);
    nrf_spim_event_clear(mSPIM0, NRF_SPIM_EVENT_STARTED);
    nrf_spim_event_clear(mSPIM1, NRF_SPIM_EVENT_END);
    nrf_spim_event_clear(mSPIM1, NRF_SPIM_EVENT_STARTED);
    nrf_spim_event_clear(mSPIM2, NRF_SPIM_EVENT_END);
    nrf_spim_event_clear(mSPIM2, NRF_SPIM_EVENT_STARTED);
    nrf_spim_event_clear(mSPIM3, NRF_SPIM_EVENT_END);
    nrf_spim_event_clear(mSPIM3, NRF_SPIM_EVENT_STARTED);

    mTransactionActive = false;

    // AUTO-RELEASE DMA buffer
    mBufferAcquired = false;
    mCurrentTotalSize = 0;

    return true;
}

bool SPIQuadNRF52::isBusy() const {
    if (!mInitialized) {
        return false;
    }

    // Check if any SPIM peripheral is busy
    bool spim0_busy = !nrf_spim_event_check(mSPIM0, NRF_SPIM_EVENT_END);
    bool spim1_busy = !nrf_spim_event_check(mSPIM1, NRF_SPIM_EVENT_END);
    bool spim2_busy = !nrf_spim_event_check(mSPIM2, NRF_SPIM_EVENT_END);
    bool spim3_busy = !nrf_spim_event_check(mSPIM3, NRF_SPIM_EVENT_END);

    return mTransactionActive || spim0_busy || spim1_busy || spim2_busy || spim3_busy;
}

bool SPIQuadNRF52::isInitialized() const {
    return mInitialized;
}

int SPIQuadNRF52::getBusId() const {
    return mBusId;
}

const char* SPIQuadNRF52::getName() const {
    return mName;
}

void SPIQuadNRF52::cleanup() {
    if (mInitialized) {
        // Wait for any pending transmission
        if (mTransactionActive) {
            waitComplete();
        }

        // Disable SPIM peripherals
        nrf_spim_disable(mSPIM0);
        nrf_spim_disable(mSPIM1);
        nrf_spim_disable(mSPIM2);
        nrf_spim_disable(mSPIM3);

        // Stop TIMER
        nrf_timer_task_trigger(mTimer, NRF_TIMER_TASK_STOP);
        nrf_timer_task_trigger(mTimer, NRF_TIMER_TASK_CLEAR);

        // Disable PPI channels
        NRF_PPI->CHENCLR = (1UL << mPPIChannel4) | (1UL << mPPIChannel5) |
                           (1UL << mPPIChannel6) | (1UL << mPPIChannel7);

        // Free DMA buffer
        if (!mDMABuffer.empty()) {
            free(mDMABuffer.data());
            mDMABuffer = fl::span<uint8_t>();
            mMaxBytesPerLane = 0;
            mCurrentTotalSize = 0;
            mBufferAcquired = false;
        }

        // Free lane buffers
        if (mLane0Buffer != nullptr) {
            free(mLane0Buffer);
            mLane0Buffer = nullptr;
        }
        if (mLane1Buffer != nullptr) {
            free(mLane1Buffer);
            mLane1Buffer = nullptr;
        }
        if (mLane2Buffer != nullptr) {
            free(mLane2Buffer);
            mLane2Buffer = nullptr;
        }
        if (mLane3Buffer != nullptr) {
            free(mLane3Buffer);
            mLane3Buffer = nullptr;
        }
        mBufferSize = 0;

        mInitialized = false;
    }
}

void SPIQuadNRF52::configureTimer(uint32_t clock_speed_hz) {
    // Configure TIMER1 to generate compare events for synchronization
    // TIMER will not generate the actual clock signal, but will trigger
    // SPIM START tasks via PPI for synchronized transmission

    // Stop timer if running
    nrf_timer_task_trigger(mTimer, NRF_TIMER_TASK_STOP);
    nrf_timer_task_trigger(mTimer, NRF_TIMER_TASK_CLEAR);

    // Configure timer mode
    nrf_timer_mode_set(mTimer, NRF_TIMER_MODE_TIMER);
    nrf_timer_bit_width_set(mTimer, NRF_TIMER_BIT_WIDTH_32);

    // Use 16 MHz prescaler 0 (maximum resolution)
    nrf_timer_frequency_set(mTimer, NRF_TIMER_FREQ_16MHz);

    // Set compare value to trigger at start of transmission
    // For quad-SPI, we just need a single trigger event
    // The SPIM peripherals will handle their own clock generation
    nrf_timer_cc_set(mTimer, NRF_TIMER_CC_CHANNEL0, 1);

    // Enable compare event
    nrf_timer_event_clear(mTimer, NRF_TIMER_EVENT_COMPARE0);

    // Configure timer in one-shot mode (stops after compare)
    nrf_timer_shorts_set(mTimer, NRF_TIMER_SHORT_COMPARE0_STOP_MASK);
}

void SPIQuadNRF52::configurePPI() {
    // Configure PPI channels to synchronize SPIM START tasks
    // All PPI channels connect TIMER1 COMPARE[0] event to SPIM START tasks

    // Get event and task addresses
    uint32_t timer_compare_event = (uint32_t)&mTimer->EVENTS_COMPARE[0];
    uint32_t spim0_start_task = (uint32_t)&mSPIM0->TASKS_START;
    uint32_t spim1_start_task = (uint32_t)&mSPIM1->TASKS_START;
    uint32_t spim2_start_task = (uint32_t)&mSPIM2->TASKS_START;
    uint32_t spim3_start_task = (uint32_t)&mSPIM3->TASKS_START;

    // Configure PPI channel 4: TIMER1.COMPARE[0] -> SPIM0.START
    NRF_PPI->CH[mPPIChannel4].EEP = timer_compare_event;
    NRF_PPI->CH[mPPIChannel4].TEP = spim0_start_task;

    // Configure PPI channel 5: TIMER1.COMPARE[0] -> SPIM1.START
    NRF_PPI->CH[mPPIChannel5].EEP = timer_compare_event;
    NRF_PPI->CH[mPPIChannel5].TEP = spim1_start_task;

    // Configure PPI channel 6: TIMER1.COMPARE[0] -> SPIM2.START
    NRF_PPI->CH[mPPIChannel6].EEP = timer_compare_event;
    NRF_PPI->CH[mPPIChannel6].TEP = spim2_start_task;

    // Configure PPI channel 7: TIMER1.COMPARE[0] -> SPIM3.START
    NRF_PPI->CH[mPPIChannel7].EEP = timer_compare_event;
    NRF_PPI->CH[mPPIChannel7].TEP = spim3_start_task;

    // Enable PPI channels 4, 5, 6, and 7
    NRF_PPI->CHENSET = (1UL << mPPIChannel4) | (1UL << mPPIChannel5) |
                       (1UL << mPPIChannel6) | (1UL << mPPIChannel7);
}

void SPIQuadNRF52::startTransmission() {
    // Start TIMER1 to trigger the COMPARE[0] event
    // This will cause PPI to start all four SPIM peripherals simultaneously

    // Clear the compare event before starting
    nrf_timer_event_clear(mTimer, NRF_TIMER_EVENT_COMPARE0);

    // Clear SPIM events
    nrf_spim_event_clear(mSPIM0, NRF_SPIM_EVENT_END);
    nrf_spim_event_clear(mSPIM0, NRF_SPIM_EVENT_STARTED);
    nrf_spim_event_clear(mSPIM1, NRF_SPIM_EVENT_END);
    nrf_spim_event_clear(mSPIM1, NRF_SPIM_EVENT_STARTED);
    nrf_spim_event_clear(mSPIM2, NRF_SPIM_EVENT_END);
    nrf_spim_event_clear(mSPIM2, NRF_SPIM_EVENT_STARTED);
    nrf_spim_event_clear(mSPIM3, NRF_SPIM_EVENT_END);
    nrf_spim_event_clear(mSPIM3, NRF_SPIM_EVENT_STARTED);

    // Start the timer - this will trigger COMPARE[0] event at tick 1
    // which via PPI will simultaneously start all four SPIM peripherals
    nrf_timer_task_trigger(mTimer, NRF_TIMER_TASK_START);
}

// ============================================================================
// Static Registration - New Polymorphic Pattern
// ============================================================================

namespace platform {

/// @brief Initialize nRF52 SpiHw4 instances
///
/// This function is called lazily by SpiHw4::getAll() on first access.
/// It replaces the old FL_INIT-based static initialization.
void initSpiHw4Instances() {
    // NRF52840/52833 has all four SPIM peripherals (SPIM0-3) available for quad-SPI
    // Create one logical quad-SPI controller (using SPIM0+SPIM1+SPIM2+SPIM3)
    static auto controller0 = fl::make_shared<SPIQuadNRF52>(0, "SPIM0+1+2+3");
    SpiHw4::registerInstance(controller0);
}

}  // namespace platform

}  // namespace fl

#endif  // NRF52840 || NRF52833
