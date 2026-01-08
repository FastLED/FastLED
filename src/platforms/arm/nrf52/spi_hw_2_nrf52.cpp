/// @file spi_hw_2_nrf52.cpp
/// @brief NRF52 implementation of Dual-SPI
///
/// This file provides the SPIDualNRF52 class implementation for Nordic nRF52 platforms.
/// All implementation is contained in this single file.

#if defined(NRF52) || defined(NRF52832) || defined(NRF52840) || defined(NRF52833)

#include "spi_hw_2_nrf52.h"
#include "fl/stl/cstring.h"
#include "fl/numeric_limits.h"
#include "platforms/shared/spi_bus_manager.h"  // For DMABuffer, TransmitMode, SPIError

namespace fl {

// ============================================================================
// SPIDualNRF52 Implementation
// ============================================================================

SPIDualNRF52::SPIDualNRF52(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mSPIM0(NRF_SPIM0)
    , mSPIM1(NRF_SPIM1)
    , mTimer(NRF_TIMER0)
    , mDMABuffer()
    , mMaxBytesPerLane(0)
    , mCurrentTotalSize(0)
    , mBufferAcquired(false)
    , mLane0Buffer(nullptr)
    , mLane1Buffer(nullptr)
    , mBufferSize(0)
    , mTransactionActive(false)
    , mInitialized(false)
    , mClockPin(0)
    , mData0Pin(0)
    , mData1Pin(0)
    , mClockSpeed(0)
    , mPPIChannel0(0)
    , mPPIChannel1(1)
    , mPPIChannel2(2) {
}

SPIDualNRF52::~SPIDualNRF52() {
    cleanup();
}

bool SPIDualNRF52::begin(const SpiHw2::Config& config) {
    if (mInitialized) {
        return true;  // Already initialized
    }

    // Validate bus_num against mBusId if driver has pre-assigned ID
    if (mBusId != -1 && config.bus_num != static_cast<uint8_t>(mBusId)) {
        FL_WARN("SPIDualNRF52: Bus ID mismatch");
        return false;
    }

    // Validate pin assignments
    if (config.clock_pin < 0 || config.data0_pin < 0 || config.data1_pin < 0) {
        FL_WARN("SPIDualNRF52: Invalid pin configuration");
        return false;
    }

    // Store configuration
    mClockPin = config.clock_pin;
    mData0Pin = config.data0_pin;
    mData1Pin = config.data1_pin;
    mClockSpeed = config.clock_speed_hz;

    // Configure GPIO pins as outputs
    nrf_gpio_cfg_output(mClockPin);
    nrf_gpio_cfg_output(mData0Pin);
    nrf_gpio_cfg_output(mData1Pin);

    // Set initial pin states (low)
    nrf_gpio_pin_clear(mClockPin);
    nrf_gpio_pin_clear(mData0Pin);
    nrf_gpio_pin_clear(mData1Pin);

    // Configure SPIM0 (lane 0)
    nrf_spim_pins_set(mSPIM0, mClockPin, mData0Pin, NRF_SPIM_PIN_NOT_CONNECTED);
    nrf_spim_configure(mSPIM0, NRF_SPIM_MODE_0, NRF_SPIM_BIT_ORDER_MSB_FIRST);

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

    nrf_spim_frequency_set(mSPIM0, freq);

    // Clear events
    nrf_spim_event_clear(mSPIM0, NRF_SPIM_EVENT_END);
    nrf_spim_event_clear(mSPIM0, NRF_SPIM_EVENT_STARTED);

    // Enable SPIM0
    nrf_spim_enable(mSPIM0);

    // Configure SPIM1 (lane 1) similarly
    // Note: For true dual-SPI, SPIM1 should use a separate data pin but share timing
    // For now, we configure it independently; synchronization via PPI will be added
    nrf_spim_pins_set(mSPIM1, mClockPin, mData1Pin, NRF_SPIM_PIN_NOT_CONNECTED);
    nrf_spim_configure(mSPIM1, NRF_SPIM_MODE_0, NRF_SPIM_BIT_ORDER_MSB_FIRST);
    nrf_spim_frequency_set(mSPIM1, freq);

    // Clear events
    nrf_spim_event_clear(mSPIM1, NRF_SPIM_EVENT_END);
    nrf_spim_event_clear(mSPIM1, NRF_SPIM_EVENT_STARTED);

    // Enable SPIM1
    nrf_spim_enable(mSPIM1);

    // Configure hardware synchronization: TIMER + PPI
    configureTimer(mClockSpeed);
    configurePPI();
    configureGPIOTE();  // Currently a no-op, but reserved for future use

    mInitialized = true;
    mTransactionActive = false;

    return true;
}

void SPIDualNRF52::end() {
    cleanup();
}

DMABuffer SPIDualNRF52::acquireDMABuffer(size_t bytes_per_lane) {
    if (!mInitialized) {
        return SPIError::NOT_INITIALIZED;
    }

    // Auto-wait if previous transmission still active
    if (mTransactionActive) {
        if (!waitComplete()) {
            return SPIError::BUSY;
        }
    }

    // For dual-lane SPI: total size = bytes_per_lane × 2 (interleaved)
    constexpr size_t num_lanes = 2;
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

bool SPIDualNRF52::allocateDMABuffers(size_t required_size) {
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
    mBufferSize = 0;

    // Allocate new buffers in RAM (required for EasyDMA)
    mLane0Buffer = (uint8_t*)malloc(required_size);
    if (mLane0Buffer == nullptr) {
        FL_WARN("SPIDualNRF52: Failed to allocate lane 0 DMA buffer");
        return false;
    }

    mLane1Buffer = (uint8_t*)malloc(required_size);
    if (mLane1Buffer == nullptr) {
        FL_WARN("SPIDualNRF52: Failed to allocate lane 1 DMA buffer");
        free(mLane0Buffer);
        mLane0Buffer = nullptr;
        return false;
    }

    mBufferSize = required_size;
    return true;
}

bool SPIDualNRF52::transmit(TransmitMode mode) {
    if (!mInitialized || !mBufferAcquired) {
        return false;
    }

    // Mode is a hint - platform may block
    (void)mode;

    if (mCurrentTotalSize == 0) {
        return true;  // Nothing to transmit
    }

    // Calculate bytes per lane from total size
    constexpr size_t num_lanes = 2;
    size_t bytes_per_lane = mCurrentTotalSize / num_lanes;

    if (bytes_per_lane == 0) {
        return true;  // Empty buffer
    }

    // Allocate lane buffers for hardware (SPIM requires separate buffers per peripheral)
    if (!allocateDMABuffers(bytes_per_lane)) {
        return false;
    }

    // De-interleave mDMABuffer into lane-specific buffers
    // mDMABuffer contains interleaved data: first half -> lane 0, second half -> lane 1
    fl::memcpy(mLane0Buffer, mDMABuffer.data(), bytes_per_lane);
    fl::memcpy(mLane1Buffer, mDMABuffer.data() + bytes_per_lane, bytes_per_lane);

    // Configure SPIM0 transmission (lane 0)
    nrf_spim_tx_buffer_set(mSPIM0, mLane0Buffer, bytes_per_lane);
    nrf_spim_rx_buffer_set(mSPIM0, nullptr, 0);  // TX only

    // Configure SPIM1 transmission (lane 1)
    nrf_spim_tx_buffer_set(mSPIM1, mLane1Buffer, bytes_per_lane);
    nrf_spim_rx_buffer_set(mSPIM1, nullptr, 0);  // TX only

    // Start synchronized transmission via TIMER + PPI
    // This triggers both SPIM0 and SPIM1 START tasks simultaneously
    startTransmission();

    mTransactionActive = true;
    return true;
}

bool SPIDualNRF52::waitComplete(uint32_t timeout_ms) {
    if (!mTransactionActive) {
        return true;  // Nothing to wait for
    }

    // Use a simple timeout mechanism based on loop iterations
    // For more accurate timing, could use TIMER peripheral or system tick counter
    uint32_t timeout_iterations = timeout_ms * 1000;  // Rough approximation
    uint32_t iterations = 0;

    // Wait for both SPIM peripherals to complete
    while ((!nrf_spim_event_check(mSPIM0, NRF_SPIM_EVENT_END) ||
            !nrf_spim_event_check(mSPIM1, NRF_SPIM_EVENT_END)) &&
           (timeout_ms == fl::numeric_limits<uint32_t>::max() || iterations < timeout_iterations)) {
        iterations++;
    }

    // Check if we timed out
    bool timed_out = (timeout_ms != fl::numeric_limits<uint32_t>::max()) && (iterations >= timeout_iterations);
    if (timed_out) {
        FL_WARN("SPIDualNRF52: Transaction timeout");
        // Clear state even on timeout
        mTransactionActive = false;
        return false;
    }

    // Clear events
    nrf_spim_event_clear(mSPIM0, NRF_SPIM_EVENT_END);
    nrf_spim_event_clear(mSPIM0, NRF_SPIM_EVENT_STARTED);
    nrf_spim_event_clear(mSPIM1, NRF_SPIM_EVENT_END);
    nrf_spim_event_clear(mSPIM1, NRF_SPIM_EVENT_STARTED);

    mTransactionActive = false;

    // AUTO-RELEASE DMA buffer
    mBufferAcquired = false;
    mCurrentTotalSize = 0;

    return true;
}

bool SPIDualNRF52::isBusy() const {
    if (!mInitialized) {
        return false;
    }

    // Check if either SPIM peripheral is busy
    bool spim0_busy = !nrf_spim_event_check(mSPIM0, NRF_SPIM_EVENT_END);
    bool spim1_busy = !nrf_spim_event_check(mSPIM1, NRF_SPIM_EVENT_END);

    return mTransactionActive || spim0_busy || spim1_busy;
}

bool SPIDualNRF52::isInitialized() const {
    return mInitialized;
}

int SPIDualNRF52::getBusId() const {
    return mBusId;
}

const char* SPIDualNRF52::getName() const {
    return mName;
}

void SPIDualNRF52::cleanup() {
    if (mInitialized) {
        // Wait for any pending transmission
        if (mTransactionActive) {
            waitComplete();
        }

        // Disable SPIM peripherals
        nrf_spim_disable(mSPIM0);
        nrf_spim_disable(mSPIM1);

        // Stop TIMER
        nrf_timer_task_trigger(mTimer, NRF_TIMER_TASK_STOP);
        nrf_timer_task_trigger(mTimer, NRF_TIMER_TASK_CLEAR);

        // Disable PPI channels
        NRF_PPI->CHENCLR = (1UL << mPPIChannel0) | (1UL << mPPIChannel1) | (1UL << mPPIChannel2);

        // Release GPIOTE resources (currently none allocated)

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
        mBufferSize = 0;

        mInitialized = false;
    }
}

void SPIDualNRF52::configureTimer(uint32_t clock_speed_hz) {
    // Configure TIMER0 to generate compare events for synchronization
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
    // For dual-SPI, we just need a single trigger event
    // The SPIM peripherals will handle their own clock generation
    nrf_timer_cc_set(mTimer, NRF_TIMER_CC_CHANNEL0, 1);

    // Enable compare event
    nrf_timer_event_clear(mTimer, NRF_TIMER_EVENT_COMPARE0);

    // Configure timer in one-shot mode (stops after compare)
    nrf_timer_shorts_set(mTimer, NRF_TIMER_SHORT_COMPARE0_STOP_MASK);
}

void SPIDualNRF52::configurePPI() {
    // Configure PPI channels to synchronize SPIM START tasks
    // All PPI channels connect TIMER0 COMPARE[0] event to SPIM START tasks

    // Get event and task addresses
    uint32_t timer_compare_event = (uint32_t)&mTimer->EVENTS_COMPARE[0];
    uint32_t spim0_start_task = (uint32_t)&mSPIM0->TASKS_START;
    uint32_t spim1_start_task = (uint32_t)&mSPIM1->TASKS_START;

    // Configure PPI channel 1: TIMER0.COMPARE[0] -> SPIM0.START
    NRF_PPI->CH[mPPIChannel1].EEP = timer_compare_event;
    NRF_PPI->CH[mPPIChannel1].TEP = spim0_start_task;

    // Configure PPI channel 2: TIMER0.COMPARE[0] -> SPIM1.START
    NRF_PPI->CH[mPPIChannel2].EEP = timer_compare_event;
    NRF_PPI->CH[mPPIChannel2].TEP = spim1_start_task;

    // Enable PPI channels 1 and 2
    NRF_PPI->CHENSET = (1UL << mPPIChannel1) | (1UL << mPPIChannel2);

    // Note: PPI channel 0 is reserved for GPIOTE clock toggle (not implemented yet)
}

void SPIDualNRF52::configureGPIOTE() {
    // Note: For dual-SPI on nRF52, GPIOTE clock toggle is not strictly necessary
    // because each SPIM peripheral generates its own clock signal on the configured
    // clock pin. Since both SPIM0 and SPIM1 are configured with the same clock pin,
    // they will both drive it (though this should be carefully considered in hardware).
    //
    // In a true dual-SPI setup, you would typically:
    // - Use separate data pins (mData0Pin, mData1Pin) ✓ We do this
    // - Share a common clock pin ✓ Both SPIM peripherals configured with mClockPin
    // - Synchronize START via PPI ✓ Implemented above
    //
    // GPIOTE could be used if we needed to generate a custom clock signal,
    // but since SPIM peripherals handle clock generation internally, we skip this.
    //
    // If future implementation requires external clock control:
    // - Configure GPIOTE channel 0 in TASK mode
    // - Set pin to mClockPin
    // - Configure toggle operation
    // - Connect TIMER COMPARE event to GPIOTE TOGGLE task via PPI channel 0
}

void SPIDualNRF52::startTransmission() {
    // Start TIMER0 to trigger the COMPARE[0] event
    // This will cause PPI to start both SPIM peripherals simultaneously

    // Clear the compare event before starting
    nrf_timer_event_clear(mTimer, NRF_TIMER_EVENT_COMPARE0);

    // Clear SPIM events
    nrf_spim_event_clear(mSPIM0, NRF_SPIM_EVENT_END);
    nrf_spim_event_clear(mSPIM0, NRF_SPIM_EVENT_STARTED);
    nrf_spim_event_clear(mSPIM1, NRF_SPIM_EVENT_END);
    nrf_spim_event_clear(mSPIM1, NRF_SPIM_EVENT_STARTED);

    // Start the timer - this will trigger COMPARE[0] event at tick 1
    // which via PPI will simultaneously start both SPIM peripherals
    nrf_timer_task_trigger(mTimer, NRF_TIMER_TASK_START);
}

// ============================================================================
// Static Registration - New Polymorphic Pattern
// ============================================================================

namespace platform {

/// @brief Initialize nRF52 SpiHw2 instances
///
/// This function is called lazily by SpiHw2::getAll() on first access.
/// It replaces the old FL_INIT-based static initialization.
void initSpiHw2Instances() {
    // NRF52 has at least SPIM0 and SPIM1 available for dual-SPI
    // Create one logical dual-SPI controller (using SPIM0+SPIM1)
    static auto controller0 = fl::make_shared<SPIDualNRF52>(0, "SPIM0+1");
    SpiHw2::registerInstance(controller0);
}

}  // namespace platform

}  // namespace fl

#endif  // NRF52 variants
