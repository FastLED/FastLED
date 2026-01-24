/// @file spi_hw_4_rp.cpp
/// @brief RP2040/RP2350 implementation of Quad-SPI using PIO
///
/// This file provides the SPIQuadRP2040 class and factory for all Raspberry Pi Pico platforms.
/// Uses PIO (Programmable I/O) to implement true quad-lane SPI with DMA support.
/// All class definition and implementation is contained in this single file.

#if defined(PICO_RP2040) || defined(PICO_RP2350) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)

#include "platforms/shared/spi_hw_4.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pio_asm.h"
#include "fl/warn.h"
#include "fl/numeric_limits.h"
#include <cstring> // ok include
#include "fl/stl/cstring.h"
#include "fl/stl/time.h"
#include "platforms/shared/spi_bus_manager.h"  // For DMABuffer, TransmitMode, SPIError

namespace fl {

// ============================================================================
// PIO Program for Quad-SPI
// ============================================================================

/// PIO program for quad-lane SPI transmission
///
/// The program outputs synchronized data on 4 data pins (D0, D1, D2, D3) with a clock signal.
/// Data is fed from DMA into the PIO TX FIFO as 32-bit words.
///
/// Pin mapping:
/// - Base pin: D0 (data bit 0)
/// - Base+1:   D1 (data bit 1)
/// - Base+2:   D2 (data bit 2)
/// - Base+3:   D3 (data bit 3)
/// - Sideset:  SCK (clock)
///
/// Data format:
/// Each 32-bit word contains 8 bits to transmit, split across 4 lanes:
/// - Bits are output 4 at a time (one per lane) on each clock cycle
/// - 8 clock cycles per 32-bit word (8 bits × 4 lanes = 32 bits throughput)
///
/// Timing:
/// - Clock high on output
/// - Clock low on output
/// - Repeat for all 8 bits in the word

#define SPI_QUAD_PIO_SIDESET_COUNT 1  // Clock on sideset pin

static inline int add_spi_quad_pio_program(PIO pio) {
    // PIO program for quad-SPI:
    // Loop 8 times (output 8 bits × 4 lanes = 32 bits total per word)
    //   out pins, 4 side 1  ; Output 4 bits (D0, D1, D2, D3) with clock high
    //   nop         side 0  ; Clock low

    pio_instr spi_quad_pio_instr[] = {
        // Set loop counter Y = 7 (for 8 iterations)
        // This runs once at start via exec from C code, not part of looped program

        // wrap_target (address 0)
        // out pins, 4 side 1  ; Output 4 bits to pins D0,D1,D2,D3 with clock high
        (pio_instr)(PIO_INSTR_OUT | PIO_OUT_DST_PINS | PIO_OUT_CNT(4) | PIO_SIDESET(1, SPI_QUAD_PIO_SIDESET_COUNT)),
        // jmp y-- side 0      ; Decrement Y, loop if Y != 0, clock low
        (pio_instr)(PIO_INSTR_JMP | PIO_JMP_CND_Y_DEC | PIO_JMP_ADR(0) | PIO_SIDESET(0, SPI_QUAD_PIO_SIDESET_COUNT)),
        // set y, 7 side 0     ; Reset counter for next word, clock low
        (pio_instr)(PIO_INSTR_SET | PIO_SET_DST_Y | PIO_SET_DATA(7) | PIO_SIDESET(0, SPI_QUAD_PIO_SIDESET_COUNT)),
        // wrap (back to address 0)
    };

    struct pio_program spi_quad_pio_program = {
        .instructions = spi_quad_pio_instr,
        .length = sizeof(spi_quad_pio_instr) / sizeof(spi_quad_pio_instr[0]),
        .origin = -1,
    };

    if (!pio_can_add_program(pio, &spi_quad_pio_program))
        return -1;

    return (int)pio_add_program(pio, &spi_quad_pio_program);
}

static inline pio_sm_config spi_quad_pio_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + 0, offset + 2);  // Wrap from instruction 2 back to 0
    sm_config_set_sideset(&c, SPI_QUAD_PIO_SIDESET_COUNT, false, false);
    return c;
}

// ============================================================================
// SPIQuadRP2040 Class Definition
// ============================================================================

/// RP2040/RP2350 hardware driver for Quad-SPI DMA transmission using PIO
///
/// Implements SpiHw4 interface for Raspberry Pi Pico platforms using:
/// - PIO (Programmable I/O) for synchronized quad-lane output
/// - DMA for non-blocking asynchronous transfers
/// - Configurable clock frequency up to 25 MHz
/// - Auto-detection of active lanes (1/2/4-lane modes)
///
/// @note Each instance allocates one PIO state machine and one DMA channel
/// @note Data pins must be consecutive GPIO numbers (D0, D0+1, D0+2, D0+3)
class SPIQuadRP2040 : public SpiHw4 {
public:
    /// @brief Construct a new SPIQuadRP2040 controller
    /// @param bus_id Logical bus identifier (0 or 1)
    /// @param name Human-readable name for this controller
    explicit SPIQuadRP2040(int bus_id = -1, const char* name = "Unknown");

    /// @brief Destroy the controller and release all resources
    ~SPIQuadRP2040();

    /// @brief Initialize the SPI controller with specified configuration
    /// @param config Configuration including pins, clock speed, and bus number
    /// @return true if initialization successful, false on error
    /// @note Validates pin assignments and allocates PIO/DMA resources
    /// @note Auto-detects 1/2/4-lane mode based on active pins
    bool begin(const SpiHw4::Config& config) override;

    /// @brief Deinitialize the controller and release resources
    void end() override;

    /// @brief Acquire DMA buffer for zero-copy transmission
    /// @param bytes_per_lane Number of bytes per lane to allocate
    /// @return DMABuffer containing span to write into, or error
    /// @note Auto-waits if previous transmission still active
    DMABuffer acquireDMABuffer(size_t bytes_per_lane) override;

    /// @brief Start non-blocking transmission using acquired DMA buffer
    /// @param mode Transmission mode (async/sync hint - may be ignored)
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
    /// @return Bus ID (0 or 1)
    int getBusId() const override;

    /// @brief Get the human-readable name for this controller
    /// @return Controller name string
    const char* getName() const override;

private:
    /// @brief Release all allocated resources (PIO, DMA, buffers)
    void cleanup();

    int mBusId;  ///< Logical bus identifier
    const char* mName;  ///< Controller name

    // PIO resources
    PIO mPIO;
    int mStateMachine;
    int mPIOOffset;

    // DMA resources
    int mDMAChannel;

    // DMA buffer management
    fl::span<uint8_t> mDMABuffer;    // Allocated DMA buffer (interleaved format for quad-lane)
    size_t mMaxBytesPerLane;         // Max bytes per lane we've allocated for
    size_t mCurrentTotalSize;        // Current transmission size (bytes_per_lane * 4)
    bool mBufferAcquired;

    // State
    bool mTransactionActive;
    bool mInitialized;

    // Configuration
    uint8_t mClockPin;
    uint8_t mData0Pin;
    uint8_t mData1Pin;
    uint8_t mData2Pin;
    uint8_t mData3Pin;

    SPIQuadRP2040(const SPIQuadRP2040&) = delete;
    SPIQuadRP2040& operator=(const SPIQuadRP2040&) = delete;
};

// ============================================================================
// SPIQuadRP2040 Implementation
// ============================================================================

SPIQuadRP2040::SPIQuadRP2040(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mPIO(nullptr)
    , mStateMachine(-1)
    , mPIOOffset(-1)
    , mDMAChannel(-1)
    , mDMABuffer()
    , mMaxBytesPerLane(0)
    , mCurrentTotalSize(0)
    , mBufferAcquired(false)
    , mTransactionActive(false)
    , mInitialized(false)
    , mClockPin(0)
    , mData0Pin(0)
    , mData1Pin(0)
    , mData2Pin(0)
    , mData3Pin(0) {
}

SPIQuadRP2040::~SPIQuadRP2040() {
    cleanup();
}

bool SPIQuadRP2040::begin(const SpiHw4::Config& config) {
    if (mInitialized) {
        return true;  // Already initialized
    }

    // Validate bus_num against mBusId if driver has pre-assigned ID
    if (mBusId != -1 && config.bus_num != static_cast<uint8_t>(mBusId)) {
        FL_WARN("SPIQuadRP2040: Bus ID mismatch");
        return false;
    }

    // Validate pin assignments - at least clock and D0 must be set
    if (config.clock_pin < 0 || config.data0_pin < 0) {
        FL_WARN("SPIQuadRP2040: Invalid pin configuration (clock and D0 required)");
        return false;
    }

    // Store pin configuration
    mClockPin = config.clock_pin;
    mData0Pin = config.data0_pin;
    mData1Pin = config.data1_pin;
    mData2Pin = config.data2_pin;
    mData3Pin = config.data3_pin;

    // Find available PIO instance and state machine
#if defined(PICO_RP2040)
    const PIO pios[NUM_PIOS] = { pio0, pio1 };
#elif defined(PICO_RP2350) || defined(ARDUINO_ARCH_RP2350)
    const PIO pios[NUM_PIOS] = { pio0, pio1, pio2 };
#endif

    mPIO = nullptr;
    mStateMachine = -1;
    mPIOOffset = -1;

    for (unsigned int i = 0; i < NUM_PIOS; i++) {
        PIO pio = pios[i];
        int sm = pio_claim_unused_sm(pio, false);
        if (sm == -1) continue;

        int offset = add_spi_quad_pio_program(pio);
        if (offset == -1) {
            pio_sm_unclaim(pio, sm);
            continue;
        }

        mPIO = pio;
        mStateMachine = sm;
        mPIOOffset = offset;
        break;
    }

    if (mPIO == nullptr || mStateMachine == -1 || mPIOOffset == -1) {
        FL_WARN("SPIQuadRP2040: No available PIO resources");
        return false;
    }

    // Claim DMA channel
    mDMAChannel = dma_claim_unused_channel(false);
    if (mDMAChannel == -1) {
        FL_WARN("SPIQuadRP2040: No available DMA channel");
        pio_sm_unclaim(mPIO, mStateMachine);
        return false;
    }

    // Configure GPIO pins for PIO
    // Data pins must be consecutive for quad mode
    if (mData1Pin >= 0 && mData2Pin >= 0 && mData3Pin >= 0) {
        // Full quad mode - all 4 pins must be consecutive
        if (mData1Pin != mData0Pin + 1 || mData2Pin != mData0Pin + 2 || mData3Pin != mData0Pin + 3) {
            FL_WARN("SPIQuadRP2040: Data pins must be consecutive (D0, D0+1, D0+2, D0+3)");
            dma_channel_unclaim(mDMAChannel);
            pio_sm_unclaim(mPIO, mStateMachine);
            return false;
        }
    }

    // Initialize active data pins
    pio_gpio_init(mPIO, mData0Pin);
    pio_sm_set_consecutive_pindirs(mPIO, mStateMachine, mData0Pin, 1, true);  // D0 as output

    int active_pins = 1;
    if (mData1Pin >= 0) {
        pio_gpio_init(mPIO, mData1Pin);
        pio_sm_set_consecutive_pindirs(mPIO, mStateMachine, mData1Pin, 1, true);
        active_pins++;
    }
    if (mData2Pin >= 0) {
        pio_gpio_init(mPIO, mData2Pin);
        pio_sm_set_consecutive_pindirs(mPIO, mStateMachine, mData2Pin, 1, true);
        active_pins++;
    }
    if (mData3Pin >= 0) {
        pio_gpio_init(mPIO, mData3Pin);
        pio_sm_set_consecutive_pindirs(mPIO, mStateMachine, mData3Pin, 1, true);
        active_pins++;
    }

    // Initialize clock pin
    pio_gpio_init(mPIO, mClockPin);
    pio_sm_set_consecutive_pindirs(mPIO, mStateMachine, mClockPin, 1, true);  // CLK as output

    // Configure PIO state machine
    pio_sm_config c = spi_quad_pio_program_get_default_config(mPIOOffset);
    sm_config_set_out_pins(&c, mData0Pin, 4);  // 4 output pins (D0, D1, D2, D3)
    sm_config_set_sideset_pins(&c, mClockPin);  // Clock on sideset
    sm_config_set_out_shift(&c, false, true, 32);  // Shift left, autopull at 32 bits

    // Calculate clock divider for requested frequency
    // PIO clock runs at 2x SPI clock (high + low cycles)
    float div = clock_get_hz(clk_sys) / (2.0f * config.clock_speed_hz);
    sm_config_set_clkdiv(&c, div);

    // Initialize and start state machine
    pio_sm_init(mPIO, mStateMachine, mPIOOffset, &c);

    // Initialize Y register to 7 (for 8 iterations per word)
    pio_sm_exec(mPIO, mStateMachine,
                (pio_instr)(PIO_INSTR_SET | PIO_SET_DST_Y | PIO_SET_DATA(7)));

    pio_sm_set_enabled(mPIO, mStateMachine, true);

    // Configure DMA channel
    dma_channel_config dma_config = dma_channel_get_default_config(mDMAChannel);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);  // 32-bit transfers
    channel_config_set_dreq(&dma_config, pio_get_dreq(mPIO, mStateMachine, true));  // TX DREQ
    channel_config_set_read_increment(&dma_config, true);   // Increment read address
    channel_config_set_write_increment(&dma_config, false); // Fixed write to PIO FIFO

    dma_channel_configure(mDMAChannel,
                         &dma_config,
                         &mPIO->txf[mStateMachine],  // Write to PIO TX FIFO
                         nullptr,  // Source set in transmit
                         0,        // Count set in transmit
                         false);   // Don't start yet

    mInitialized = true;
    mTransactionActive = false;

    return true;
}

void SPIQuadRP2040::end() {
    cleanup();
}

DMABuffer SPIQuadRP2040::acquireDMABuffer(size_t bytes_per_lane) {
    if (!mInitialized) {
        return DMABuffer(SPIError::NOT_INITIALIZED);
    }

    // Auto-wait if previous transmission still active
    if (mTransactionActive) {
        if (!waitComplete()) {
            return DMABuffer(SPIError::BUSY);
        }
    }

    // For quad SPI: total size = bytes_per_lane × 4 lanes (interleaved)
    constexpr size_t num_lanes = 4;
    const size_t total_size = bytes_per_lane * num_lanes;

    // Calculate required buffer size in 32-bit words for PIO
    // Each 4 bytes becomes 1 word where bits are interleaved
    size_t word_count = (total_size + 3) / 4;
    size_t buffer_size_bytes = word_count * 4;  // 4 bytes per 32-bit word

    // Reallocate buffer only if we need more capacity
    if (bytes_per_lane > mMaxBytesPerLane) {
        if (!mDMABuffer.empty()) {
            free(mDMABuffer.data());
            mDMABuffer = fl::span<uint8_t>();
        }

        // Allocate DMA-capable memory (regular malloc for RP2040)
        uint8_t* ptr = (uint8_t*)malloc(buffer_size_bytes);
        if (!ptr) {
            return DMABuffer(SPIError::ALLOCATION_FAILED);
        }

        mDMABuffer = fl::span<uint8_t>(ptr, buffer_size_bytes);
        mMaxBytesPerLane = bytes_per_lane;
    }

    mBufferAcquired = true;
    mCurrentTotalSize = total_size;

    // Return DMABuffer constructed from size
    return DMABuffer(total_size);
}

bool SPIQuadRP2040::transmit(TransmitMode mode) {
    if (!mInitialized || !mBufferAcquired) {
        return false;
    }

    // Mode is a hint - platform may block
    (void)mode;

    if (mCurrentTotalSize == 0) {
        return true;  // Nothing to transmit
    }

    // mDMABuffer now contains interleaved data written by caller
    // We need to convert it to PIO word format for transmission
    // Each 4 bytes becomes 1 word where bits are interleaved

    size_t byte_count = mCurrentTotalSize;
    size_t word_count = (byte_count + 3) / 4;

    // Access the buffer data - already allocated with enough space for words
    uint8_t* byte_buffer = mDMABuffer.data();
    uint32_t* word_buffer = (uint32_t*)mDMABuffer.data();

    // Convert interleaved byte data to PIO word format
    // Work backwards to avoid overwriting data we haven't read yet
    for (int i = (int)word_count - 1; i >= 0; i--) {
        size_t byte_idx = i * 4;
        uint8_t byte0 = byte_buffer[byte_idx];
        uint8_t byte1 = (byte_idx + 1 < byte_count) ? byte_buffer[byte_idx + 1] : 0;
        uint8_t byte2 = (byte_idx + 2 < byte_count) ? byte_buffer[byte_idx + 2] : 0;
        uint8_t byte3 = (byte_idx + 3 < byte_count) ? byte_buffer[byte_idx + 3] : 0;

        // Interleave bits from byte0, byte1, byte2, byte3
        // Result: 32 bits where bits are grouped by nibbles from each byte
        // Clock cycle 0: byte0[7], byte1[7], byte2[7], byte3[7]
        // Clock cycle 1: byte0[6], byte1[6], byte2[6], byte3[6]
        // ... and so on
        uint32_t interleaved = 0;
        for (int bit = 0; bit < 8; bit++) {
            // Extract bits from all 4 bytes and place them in the output
            uint32_t bit0 = (byte0 >> (7 - bit)) & 1;
            uint32_t bit1 = (byte1 >> (7 - bit)) & 1;
            uint32_t bit2 = (byte2 >> (7 - bit)) & 1;
            uint32_t bit3 = (byte3 >> (7 - bit)) & 1;

            // Pack 4 bits into a nibble, MSB first
            uint32_t nibble = (bit0 << 3) | (bit1 << 2) | (bit2 << 1) | bit3;
            interleaved |= (nibble << (28 - bit * 4));
        }

        word_buffer[i] = interleaved;
    }

    // Start DMA transfer
    dma_channel_set_read_addr(mDMAChannel, word_buffer, false);
    dma_channel_set_trans_count(mDMAChannel, word_count, true);  // Start transfer

    mTransactionActive = true;
    return true;
}

bool SPIQuadRP2040::waitComplete(uint32_t timeout_ms) {
    if (!mTransactionActive) {
        return true;  // Nothing to wait for
    }

    // Poll with timeout checking
    if (timeout_ms == fl::numeric_limits<uint32_t>::max()) {
        // Infinite timeout - just wait
        dma_channel_wait_for_finish_blocking(mDMAChannel);
    } else {
        // Timeout-based polling with timestamp checking
        fl::u32 start_time = fl::millis();
        while (dma_channel_is_busy(mDMAChannel)) {
            if ((fl::millis() - start_time) >= timeout_ms) {
                return false;  // Timeout
            }
        }
    }

    mTransactionActive = false;

    // Auto-release DMA buffer
    mBufferAcquired = false;
    mCurrentTotalSize = 0;

    return true;
}

bool SPIQuadRP2040::isBusy() const {
    if (!mInitialized) {
        return false;
    }
    return mTransactionActive || dma_channel_is_busy(mDMAChannel);
}

bool SPIQuadRP2040::isInitialized() const {
    return mInitialized;
}

int SPIQuadRP2040::getBusId() const {
    return mBusId;
}

const char* SPIQuadRP2040::getName() const {
    return mName;
}

void SPIQuadRP2040::cleanup() {
    if (mInitialized) {
        // Wait for any pending transmission
        if (mTransactionActive) {
            waitComplete();
        }

        // Free DMA buffer
        if (!mDMABuffer.empty()) {
            free(mDMABuffer.data());
            mDMABuffer = fl::span<uint8_t>();
            mMaxBytesPerLane = 0;
            mCurrentTotalSize = 0;
            mBufferAcquired = false;
        }

        // Disable and unclaim PIO state machine
        if (mPIO != nullptr && mStateMachine != -1) {
            pio_sm_set_enabled(mPIO, mStateMachine, false);
            pio_sm_unclaim(mPIO, mStateMachine);
        }

        // Release DMA channel
        if (mDMAChannel != -1) {
            dma_channel_unclaim(mDMAChannel);
            mDMAChannel = -1;
        }

        mInitialized = false;
    }
}

// ============================================================================
// Static Registration - New Polymorphic Pattern
// ============================================================================

namespace platform {

/// @brief Initialize RP2040/RP2350 SpiHw4 instances
///
/// This function is called lazily by SpiHw4::getAll() on first access.
/// It replaces the old FL_INIT-based static initialization.
void initSpiHw4Instances() {
    // Create 2 logical SPI buses (each uses separate PIO state machine)
    static auto controller0 = fl::make_shared<SPIQuadRP2040>(0, "SPI0");
    static auto controller1 = fl::make_shared<SPIQuadRP2040>(1, "SPI1");

    SpiHw4::registerInstance(controller0);
    SpiHw4::registerInstance(controller1);
}

}  // namespace platform

}  // namespace fl

#endif  // PICO_RP2040 || PICO_RP2350 || ARDUINO_ARCH_RP2040 || ARDUINO_ARCH_RP2350
