/// @file channel_engine_spi.h
/// @brief Clockless-over-SPI implementation of ChannelEngine for ESP32
///
/// ⚠️ ARCHITECTURE NOTE: This is NOT a general SPI LED driver (e.g., APA102, SK9822)!
///
/// This driver implements CLOCKLESS protocols (WS2812, SK6812, etc.) using SPI hardware
/// as a bit-banging engine. The SPI clock pin is specified for internal timing generation
/// but is NEVER physically connected to the LED strip - only the MOSI/data pin is used.
///
/// How it works:
/// 1. The SPI peripheral generates a clock signal internally
/// 2. This clock controls the precise timing of MOSI bit transitions
/// 3. Each LED bit (0 or 1) expands to 8 SPI bits using wave8 encoding:
///    - LED bit '0' → wave8 pattern with ~2 HIGH pulses (short high, long low)
///    - LED bit '1' → wave8 pattern with ~5 HIGH pulses (long high, short low)
/// 4. The LEDs decode these patterns based on pulse width (T0H/T0L vs T1H/T1L)
/// 5. The clock signal itself is ignored by the LEDs (never leaves the ESP32)
///
/// Why use SPI for clockless protocols?
/// - Precise timing: SPI hardware generates exact bit patterns without CPU intervention
/// - DMA support: Large LED buffers transmit without blocking the CPU
///
/// Uses wave8 encoding (8-bit expansion):
/// - Each LED bit expands to 8 SPI bits (1 byte per LED bit)
/// - Each LED byte (8 bits) expands to 8 SPI bytes (64 bits)
/// - SPI clock: ~6.4 MHz for WS2812 (8 pulses per 1250ns bit period)
/// - Wave8 LUT built from chipset T1/T2/T3 timing for any LED protocol

#pragma once

#include "fl/compiler_control.h"
#include "platforms/is_platform.h"

#ifdef FL_IS_ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_CLOCKLESS_SPI

#include "fl/channels/engine.h"
#include "fl/channels/data.h"
#include "fl/channels/wave8.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/engine_events.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/stl/unordered_map.h"
#include "fl/stl/chrono.h"

// EXCEPTION: Platform headers in .h file (technical debt)
// Normally platform-specific headers (driver/*.h, soc/*.h) should only be in .cpp files
// to improve IDE support and cross-platform compatibility. However, this driver's public
// interface requires ESP-IDF types (spi_device_handle_t, spi_host_device_t, gpio_num_t).
// TODO: Consider refactoring to use forward declarations + opaque handles in future.
#include "driver/spi_master.h"
#include "driver/gpio.h"

namespace fl {

/// @brief Multi-lane SPI pin configuration
///
/// Configures which GPIO pins to use for dual/quad SPI transmission.
/// The number of lanes is determined by which pins are set (>= 0):
/// - 1 lane: Only data0_pin set
/// - 2 lanes (dual): data0_pin and data1_pin set
/// - 4 lanes (quad): data0_pin, data1_pin, data2_pin, and data3_pin set
///
/// Platform support:
/// - ESP32 classic, S2, S3: Supports 1, 2, or 4 lanes
/// - ESP32-C6, C3, H2: Supports 1 or 2 lanes only (no quad)
struct MultiLanePinConfig {
    gpio_num_t data0_pin;  ///< Data0/MOSI pin (always required)
    gpio_num_t data1_pin;  ///< Data1/MISO pin for dual/quad mode (-1 if unused)
    gpio_num_t data2_pin;  ///< Data2/WP pin for quad mode (-1 if unused, ESP32/S2/S3 only)
    gpio_num_t data3_pin;  ///< Data3/HD pin for quad mode (-1 if unused, ESP32/S2/S3 only)

    /// @brief Default constructor (single-lane mode)
    constexpr MultiLanePinConfig()
        : data0_pin(static_cast<gpio_num_t>(-1))
        , data1_pin(static_cast<gpio_num_t>(-1))
        , data2_pin(static_cast<gpio_num_t>(-1))
        , data3_pin(static_cast<gpio_num_t>(-1))
    {}

    /// @brief Single-lane constructor
    constexpr explicit MultiLanePinConfig(gpio_num_t data0)
        : data0_pin(data0)
        , data1_pin(static_cast<gpio_num_t>(-1))
        , data2_pin(static_cast<gpio_num_t>(-1))
        , data3_pin(static_cast<gpio_num_t>(-1))
    {}

    /// @brief Dual-lane constructor
    constexpr MultiLanePinConfig(gpio_num_t data0, gpio_num_t data1)
        : data0_pin(data0)
        , data1_pin(data1)
        , data2_pin(static_cast<gpio_num_t>(-1))
        , data3_pin(static_cast<gpio_num_t>(-1))
    {}

    /// @brief Quad-lane constructor (ESP32/S2/S3 only)
    constexpr MultiLanePinConfig(gpio_num_t data0, gpio_num_t data1,
                                  gpio_num_t data2, gpio_num_t data3)
        : data0_pin(data0)
        , data1_pin(data1)
        , data2_pin(data2)
        , data3_pin(data3)
    {}

    /// @brief Get number of active lanes
    u8 getLaneCount() const {
        u8 count = (data0_pin >= 0) ? 1 : 0;
        if (data1_pin >= 0) count++;
        if (data2_pin >= 0) count++;
        if (data3_pin >= 0) count++;
        return count;
    }
};

/// @brief SPI timing configuration for clockless LED encoding
///
/// Describes the bit patterns and clock frequency for encoding LED data
/// via SPI hardware. Built from chipset timing (T1/T2/T3) using
/// calculateSpiTiming().
struct SpiTimingConfig {
    enum Protocol { WS2812, SK6812, CUSTOM };

    Protocol protocol;             ///< Protocol type
    u32 clock_hz;                  ///< SPI clock frequency in Hz
    u8 bits_per_led_bit;           ///< Number of SPI bits per LED bit
    u32 reset_time_us;             ///< Reset time in microseconds
    u32 bit0_pattern;              ///< SPI bit pattern for LED bit '0'
    u8 bit0_count;                 ///< Number of valid bits in bit0_pattern
    u32 bit1_pattern;              ///< SPI bit pattern for LED bit '1'
    u8 bit1_count;                 ///< Number of valid bits in bit1_pattern
    u32 achieved_t0h_ns;           ///< Actual T0H timing achieved
    u32 achieved_t0l_ns;           ///< Actual T0L timing achieved
    u32 achieved_t1h_ns;           ///< Actual T1H timing achieved
    u32 achieved_t1l_ns;           ///< Actual T1L timing achieved

    /// @brief Create default WS2812 timing config
    static SpiTimingConfig ws2812(u32 reset_us = 280) {
        SpiTimingConfig cfg = {};
        cfg.protocol = WS2812;
        cfg.clock_hz = 6400000;
        cfg.bits_per_led_bit = 8;
        cfg.reset_time_us = reset_us;
        cfg.bit0_pattern = 0b11000000;
        cfg.bit0_count = 8;
        cfg.bit1_pattern = 0b11111100;
        cfg.bit1_count = 8;
        cfg.achieved_t0h_ns = 312;
        cfg.achieved_t0l_ns = 938;
        cfg.achieved_t1h_ns = 938;
        cfg.achieved_t1l_ns = 312;
        return cfg;
    }
};

/// @brief Clockless-over-SPI IChannelEngine implementation
///
/// ⚠️ This is a CLOCKLESS LED driver using SPI hardware, NOT a true SPI chipset driver!
///
/// Consolidates clockless LED strip functionality using SPI peripheral as bit-banger:
/// - Implements clockless protocols (WS2812, SK6812, etc.) via SPI bit patterns
/// - Direct ESP-IDF SPI master driver integration
/// - Wave8 encoding (SPI clock from chipset timing, 8:1 bit expansion)
/// - Multi-lane SPI support (dual/quad modes for parallel strip transmission)
/// - Channel persistence between frames (avoid recreation overhead)
/// - On-demand SPI bus allocation with reference counting
/// - DMA support with PSRAM→DRAM buffer copying
///
/// How clockless-over-SPI works:
/// - SPI clock generates precise timing internally (e.g., 2.5MHz = 400ns/bit)
/// - Clock pin is specified but NEVER physically connected to LEDs
/// - Only MOSI/data pin carries signals to the LED strip
/// - Each LED bit expands to multiple SPI bits with specific patterns
/// - LEDs decode based on pulse widths, ignoring the clock signal
///
/// Managed by ChannelBusManager which handles frame lifecycle events.
class ChannelEngineSpi : public IChannelEngine {
public:
    ChannelEngineSpi();
    ~ChannelEngineSpi() override;

    /// @brief Configure multi-lane SPI pins for a specific data pin
    /// @param pinConfig Multi-lane pin configuration
    ///
    /// Example usage:
    /// @code
    /// // Dual-lane SPI (2x throughput)
    /// engine.configureMultiLanePins(MultiLanePinConfig(23, 19));
    ///
    /// // Quad-lane SPI (4x throughput, ESP32/S2/S3 only)
    /// engine.configureMultiLanePins(MultiLanePinConfig(23, 19, 18, 5));
    /// @endcode
    void configureMultiLanePins(const MultiLanePinConfig& pinConfig);

    // IChannelEngine interface implementation
    void enqueue(ChannelDataPtr channelData) override;
    void show() override;
    EngineState poll() override;

    /// @brief Get the engine name for affinity binding
    /// @return "SPI"
    fl::string getName() const override { return fl::string::from_literal("SPI"); }

    /// @brief Get engine capabilities (CLOCKLESS protocols only)
    /// @return Capabilities with supportsClockless=true, supportsSpi=false
    /// @note This is a clockless-over-SPI driver, not a true SPI LED driver
    Capabilities getCapabilities() const override {
        return Capabilities(true, false);  // Clockless only
    }

    /// @brief Check if this engine can handle the given channel data
    /// @param data Channel data to check
    /// @return true for CLOCKLESS chipsets (WS2812, SK6812, etc.), false for true SPI chipsets
    ///
    /// This engine is a CLOCKLESS-over-SPI driver:
    ///   - Returns true for clockless protocols (WS2812, SK6812, etc.)
    ///   - Returns false for true SPI protocols (APA102, SK9822, etc.)
    bool canHandle(const ChannelDataPtr& data) const override;

private:
    /// @brief Begin LED data transmission with internal batching (internal helper)
    /// @param channelData Span of channel data to transmit
    ///
    /// Groups channels by timing compatibility and batches them when N > K,
    /// where N = number of channels with compatible timings and K = available lanes.
    void beginBatchedTransmission(fl::span<const ChannelDataPtr> channelData);

    /// @brief Begin LED data transmission for all channels (internal helper)
    /// @param channelData Span of channel data to transmit
    void beginTransmission(fl::span<const ChannelDataPtr> channelData);

    /// @brief Determine maximum parallel transmission capacity
    /// @param channels Channels in a timing group (unused, for future extension)
    /// @return Number of SPI hosts available (2 for ESP32/S2/S3/P4, 1 for ESP32-C3)
    u8 determineLaneCapacity(fl::span<const ChannelDataPtr> channels);
    /// @brief SPI channel state (per-pin tracking)
    struct SpiChannelState {
        spi_host_device_t spi_host;        ///< SPI peripheral (SPI2_HOST, SPI3_HOST, etc.)
        spi_device_handle_t spi_device;    ///< Device handle from spi_bus_add_device
        gpio_num_t pin;                    ///< Output GPIO pin (data0/MOSI)
        ChipsetTimingConfig timing;        ///< Clockless timing configuration
        volatile bool transmissionComplete; ///< Set when all encoding/transmission complete
        bool inUse;                        ///< Channel active flag
        bool useDMA;                       ///< DMA enabled for this channel

        // Multi-lane SPI support (dual/quad modes)
        u8 numLanes;                       ///< Number of active data lanes
        gpio_num_t data1_pin;              ///< Data1/MISO pin for dual/quad mode (-1, unused)
        gpio_num_t data2_pin;              ///< Data2/WP pin for quad mode (-1, unused)
        gpio_num_t data3_pin;              ///< Data3/HD pin for quad mode (-1, unused)

        // Cache coherence control (ESP32-S3/C6 workaround)
        bool cacheSyncDisabled;            ///< Set when esp_cache_msync() returns ESP_ERR_INVALID_ARG

        // Staging buffers (4KB each for DMA efficiency)
        // Double-buffered: encode into one while DMA transmits the other.
        u8* stagingA;                      ///< Staging buffer A (DMA-capable memory)
        u8* stagingB;                      ///< Staging buffer B (DMA-capable memory, for double-buffering)
        u8* currentStaging;                ///< Points to active encoding buffer
        size_t stagingOffset;              ///< Current write position in staging buffer
        size_t stagingCapacity;            ///< Size of each staging buffer (4096 bytes)

        // LED source data
        u8* ledSourceBuffer;               ///< Internal SRAM copy of LED data (DMA-capable)
        size_t ledSourceBufferSize;        ///< Size of ledSourceBuffer allocation
        const u8* ledSource;          ///< Current position in source LED data
        size_t ledBytesRemaining;          ///< LED bytes left to encode
        ChannelDataPtr sourceData;         ///< Reference to source ChannelData for cleanup

        // Transactions (double-buffered)
        spi_transaction_t transA;          ///< Transaction struct for buffer A
        spi_transaction_t transB;          ///< Transaction struct for buffer B
        volatile bool transAInFlight;      ///< Transaction A in progress flag
        volatile bool transBInFlight;      ///< Transaction B in progress flag

        // Wave8 encoding LUT (built once from chipset timing)
        Wave8BitExpansionLut wave8Lut;     ///< Wave8 expansion lookup table (64 bytes)
        bool wave8LutInitialized;          ///< LUT has been built from timing

        // ISR coordination flags
        volatile bool hasNewData;          ///< Set by post-transaction callback to trigger encoding
        volatile bool isShuttingDown;      ///< Set during teardown to prevent ISR access

        // Debug capture (diagnostics)
        bool debugTxCaptured;              ///< First TX buffer bytes captured for diagnostics
        u8 debugTxBuffer[8];               ///< Captured first 8 bytes of TX buffer
    };

    /// @brief DMA pipeline state for poll()-driven async transmission
    ///
    /// Phase 3: No FreeRTOS worker task. show() starts the first DMA transaction
    /// and returns immediately. poll() advances the pipeline by checking DMA
    /// completion and encoding/queueing the next chunk.
    struct DmaPipelineState {
        enum Phase {
            IDLE,           ///< No transmission in progress
            STREAMING,      ///< DMA in flight, encoding pipeline active
            RESET_DELAY,    ///< Waiting for inter-batch reset delay
            COMPLETING,     ///< Final DMA in flight, no more encoding needed
        };

        Phase mPhase;
        SpiChannelState* mActiveChannel;
        int mEncodeIdx;
        bool mDmaInFlight;

        size_t mCurrentBatchIdx;
        size_t mCurrentChannelInBatch;
        size_t mCurrentTimingGroup;

        struct TimingGroupInfo {
            ChipsetTimingConfig timing;
            fl::vector<ChannelDataPtr> channels;
            u8 laneCapacity;
        };
        fl::vector<TimingGroupInfo> mTimingGroups;

        u32 mResetDelayStartUs;
        u32 mResetDelayDurationUs;

        DmaPipelineState()
            : mPhase(IDLE)
            , mActiveChannel(nullptr)
            , mEncodeIdx(0)
            , mDmaInFlight(false)
            , mCurrentBatchIdx(0)
            , mCurrentChannelInBatch(0)
            , mCurrentTimingGroup(0)
            , mResetDelayStartUs(0)
            , mResetDelayDurationUs(0)
        {}
    };

    /// @brief Advance the DMA pipeline state machine (called from poll())
    EngineState advancePipeline();

    /// @brief Start transmitting the next channel in the pipeline
    bool startNextChannel();

    /// @brief Start the first DMA transaction for the active channel
    void startFirstDma();

    /// @brief SPI post-transaction callback (ISR context)
    static void IRAM_ATTR spiPostTransactionCallback(spi_transaction_t *trans);

    /// @brief Timer encoding ISR callback
    static void IRAM_ATTR timerEncodingISR(void *user_data);

    /// @brief Poll channel states for cleanup
    EngineState pollChannels();

    /// @brief Pending channel data waiting for hardware availability
    struct PendingChannel {
        ChannelDataPtr data;
        int pin;
        ChipsetTimingConfig timing;
        int retry_count = 0;  ///< Number of failed acquisition attempts
    };

    /// @brief SPI host allocation tracking (global across all channels)
    struct SpiHostTracking {
        spi_host_device_t host;
        int refCount;  ///< Reference counting (multiple channels can share a bus)
        bool initialized; ///< Bus has been initialized
        u8 activeLanes; ///< Number of lanes in use (0=unused, 1=single, 2=dual, 4=quad)
    };

    /// @brief Hash function for ChipsetTimingConfig (timing group key)
    struct TimingHash {
        size_t operator()(const ChipsetTimingConfig& t) const {
            size_t hash = 2166136261u;
            hash = (hash ^ t.t1_ns) * 16777619u;
            hash = (hash ^ t.t2_ns) * 16777619u;
            hash = (hash ^ t.t3_ns) * 16777619u;
            hash = (hash ^ t.reset_us) * 16777619u;
            return hash;
        }
    };

    /// @brief Equality function for ChipsetTimingConfig (hash map key)
    struct TimingEqual {
        bool operator()(const ChipsetTimingConfig& a, const ChipsetTimingConfig& b) const {
            return a == b;
        }
    };

    /// @brief Acquire a channel for given pin and timing
    /// @param dataSize Size of LED data in bytes
    /// @return Pointer to channel state, or nullptr if no hardware available
    SpiChannelState* acquireChannel(gpio_num_t pin, const ChipsetTimingConfig& timing, size_t dataSize);

    /// @brief Release a channel (marks as available for reuse)
    void releaseChannel(SpiChannelState* channel);

    /// @brief Create new SPI channel with given configuration
    /// @param state Channel state to initialize
    /// @param pin GPIO pin
    /// @param timing Clockless timing configuration (T1/T2/T3)
    /// @param dataSize Size of LED data in bytes
    /// @return true if channel created successfully
    bool createChannel(SpiChannelState* state, gpio_num_t pin, const ChipsetTimingConfig& timing, size_t dataSize);

    /// @brief Lightweight SPI bus + device re-initialization for reused channels.
    /// Only sets up SPI bus and device; staging buffers and timer ISR are kept.
    bool reinitSpiHardware(SpiChannelState* state, gpio_num_t pin, size_t dataSize);

    /// @brief Encode one chunk of LED data into a staging buffer
    /// @param channel Channel state with LED data to encode
    /// @param output Output buffer to write encoded SPI bytes into
    /// @param output_capacity Size of the output buffer in bytes
    /// @return Number of SPI bytes written to the output buffer
    ///
    /// Used by transmitStreaming() for double-buffered DMA pipeline.
    /// Advances channel->ledSource and decrements channel->ledBytesRemaining.
    /// Appends reset zeros after the last chunk.
    size_t encodeChunk(SpiChannelState* channel, u8* output, size_t output_capacity);

    /// @brief Double-buffered streaming transmission
    /// @param channel Channel state with LED data to transmit
    ///
    /// Overlaps DMA transfer with CPU encoding using ping-pong buffers:
    ///   1. Encode chunk into buffer A
    ///   2. Queue buffer A for DMA, encode chunk into buffer B
    ///   3. Wait for A to complete, queue B, encode into A
    ///   ... until all LED data is transmitted.
    void transmitStreaming(SpiChannelState* channel);

    /// @brief Blocking polled transmission of pre-encoded staging buffer
    /// @param channel Channel state with encoded data in staging buffer
    void transmitBlockingPolled(SpiChannelState* channel);

    /// @brief Encode LED data into SPI buffer using dynamic timing config
    /// @param ledData Input LED data bytes
    /// @param spiBuffer Output SPI buffer (resized internally)
    /// @param timing SPI timing configuration
    /// @return true if encoding succeeded
    bool encodeLedData(const fl::span<const u8>& ledData,
                       fl::vector<u8>& spiBuffer,
                       const SpiTimingConfig& timing);

    /// @brief Encode a single LED byte into SPI bit patterns
    /// @param data LED byte to encode
    /// @param buf Output buffer
    /// @param timing SPI timing configuration
    /// @param output_bit_offset Bit offset into output buffer
    /// @return Number of bits written
    u32 encodeLedByte(u8 data, u8* buf,
                      const SpiTimingConfig& timing,
                      u32 output_bit_offset);

    /// @brief Calculate SPI timing from chipset timing configuration
    /// @param chipsetTiming Chipset timing (T1/T2/T3)
    /// @return SPI timing configuration
    static SpiTimingConfig calculateSpiTiming(const ChipsetTimingConfig& chipsetTiming);

    /// @brief Get SPI timing from channel data
    /// @param data Channel data
    /// @return SPI timing configuration
    static SpiTimingConfig getSpiTimingFromChannel(const ChannelDataPtr& data);

    /// @brief Acquire SPI host for new channel
    /// @return SPI host, or SPI_HOST_MAX on failure
    spi_host_device_t acquireSpiHost();

    /// @brief Release SPI host when channel is destroyed
    void releaseSpiHost(spi_host_device_t host);

    /// @brief Process pending channels that couldn't be started earlier
    void processPendingChannels();

    /// @brief All SPI channels (active and idle)
    fl::vector_inlined<SpiChannelState, 16> mChannels;

    /// @brief Pending channels waiting for available hardware
    fl::vector_inlined<PendingChannel, 16> mPendingChannels;

    /// @brief SPI host usage tracking (static for global coordination)
    static fl::vector_inlined<SpiHostTracking, 3> sSpiHostUsage;

    /// @brief Track allocation failures to avoid hammering the driver
    bool mAllocationFailed;

    /// @brief Track last frame number to allow retry once per frame
    u32 mLastRetryFrame;

    /// @brief Multi-lane pin configurations (keyed by data0_pin)
    fl::hash_map<gpio_num_t, MultiLanePinConfig> mMultiLaneConfigs;

    // IChannelEngine state management
    /// @brief Channels enqueued via enqueue(), waiting for show()
    fl::vector_inlined<ChannelDataPtr, 16> mEnqueuedChannels;

    /// @brief Channels currently transmitting (tracked by poll() for cleanup)
    fl::vector_inlined<ChannelDataPtr, 16> mTransmittingChannels;

    /// @brief DMA pipeline state (Phase 3: poll()-driven async show())
    DmaPipelineState mPipeline;
};

} // namespace fl

#endif // FASTLED_ESP32_HAS_CLOCKLESS_SPI

#endif // FL_IS_ESP32
