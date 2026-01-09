/// @file channel_engine_spi.h
/// @brief SPI-based implementation of ChannelEngine for ESP32
///
/// Provides WS2812-over-SPI LED transmission using ESP-IDF SPI master driver.
/// Replaces third-party Espressif LED strip implementation with direct ESP-IDF integration.

#pragma once

#include "fl/compiler_control.h"

#ifdef ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_CLOCKLESS_SPI

#include "fl/channels/engine.h"
#include "fl/channels/data.h"
#include "fl/engine_events.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/stl/unordered_map.h"
#include "fl/stl/time.h"
#include "fl/timeout.h"
#include "fl/isr.h"
#include "fl/channels/wave8.h"

// Include ESP-IDF headers for SPI and GPIO types
#include "driver/spi_master.h"
#include "driver/gpio.h"

namespace fl {

/// @brief SPI timing configuration for LED protocols
///
/// Unlike RMT (which uses nanosecond-precise T1/T2/T3 timing), SPI-based
/// protocols use clock-synchronized bit patterns to encode LED data.
struct SpiTimingConfig {
    enum Protocol {
        WS2812_OVER_SPI,  ///< WS2812 encoded as SPI bits (2.5MHz, 3:1 expansion)
        APA102,           ///< Clocked protocol (future - not implemented in MVP)
        SK9822,           ///< Clocked protocol (future - not implemented in MVP)
        CUSTOM            ///< User-defined custom protocol
    };

    Protocol protocol;           ///< LED protocol type
    uint32_t clock_hz;           ///< SPI clock frequency (e.g., 2500000 for WS2812)
    uint8_t bits_per_led_bit;    ///< Encoding expansion ratio (3 for WS2812)
    uint32_t reset_time_us;      ///< Inter-frame reset time in microseconds

    // Calculated bit patterns for encoding (dynamic timing adaptation)
    uint32_t bit0_pattern;       ///< Bit pattern for logical '0' (MSB first)
    uint8_t bit0_count;          ///< Number of SPI bits in bit0_pattern
    uint32_t bit1_pattern;       ///< Bit pattern for logical '1' (MSB first)
    uint8_t bit1_count;          ///< Number of SPI bits in bit1_pattern

    // Achieved timing validation (for debugging)
    uint32_t achieved_t0h_ns;    ///< Actual T0H timing achieved
    uint32_t achieved_t0l_ns;    ///< Actual T0L timing achieved
    uint32_t achieved_t1h_ns;    ///< Actual T1H timing achieved
    uint32_t achieved_t1l_ns;    ///< Actual T1L timing achieved

    /// @brief Default constructor for WS2812-over-SPI
    constexpr SpiTimingConfig()
        : protocol(WS2812_OVER_SPI)
        , clock_hz(2500000)           // 2.5MHz
        , bits_per_led_bit(3)         // Each LED bit → 3 SPI bits
        , reset_time_us(50)           // 50μs reset time
        , bit0_pattern(0b100)         // WS2812: 100 binary
        , bit0_count(3)
        , bit1_pattern(0b110)         // WS2812: 110 binary
        , bit1_count(3)
        , achieved_t0h_ns(400)        // 1 bit @ 2.5MHz
        , achieved_t0l_ns(800)        // 2 bits @ 2.5MHz
        , achieved_t1h_ns(800)        // 2 bits @ 2.5MHz
        , achieved_t1l_ns(400)        // 1 bit @ 2.5MHz
    {}

    /// @brief Create WS2812-over-SPI timing configuration
    /// @param reset_us Reset time in microseconds (default 50μs)
    static constexpr SpiTimingConfig ws2812(uint32_t reset_us = 50) {
        SpiTimingConfig config;
        config.protocol = WS2812_OVER_SPI;
        config.clock_hz = 2500000;  // 2.5MHz
        config.bits_per_led_bit = 3;
        config.reset_time_us = reset_us;
        config.bit0_pattern = 0b100;
        config.bit0_count = 3;
        config.bit1_pattern = 0b110;
        config.bit1_count = 3;
        config.achieved_t0h_ns = 400;
        config.achieved_t0l_ns = 800;
        config.achieved_t1h_ns = 800;
        config.achieved_t1l_ns = 400;
        return config;
    }

    /// @brief Equality operator (required for hash map key)
    bool operator==(const SpiTimingConfig& other) const {
        return protocol == other.protocol &&
               clock_hz == other.clock_hz &&
               bits_per_led_bit == other.bits_per_led_bit &&
               reset_time_us == other.reset_time_us;
    }

    /// @brief Inequality operator
    bool operator!=(const SpiTimingConfig& other) const {
        return !(*this == other);
    }
};

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
    uint8_t getLaneCount() const {
        uint8_t count = (data0_pin >= 0) ? 1 : 0;
        if (data1_pin >= 0) count++;
        if (data2_pin >= 0) count++;
        if (data3_pin >= 0) count++;
        return count;
    }
};

/// @brief SPI-based IChannelEngine implementation
///
/// Consolidates SPI LED strip functionality:
/// - Direct ESP-IDF SPI master driver integration
/// - WS2812-over-SPI bit encoding (2.5MHz, 3:1 expansion)
/// - Multi-lane SPI support (dual/quad modes for higher throughput)
/// - Channel persistence between frames (avoid recreation overhead)
/// - On-demand SPI bus allocation with reference counting
/// - DMA support with PSRAM→DRAM buffer copying
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
    const char* getName() const override { return "SPI"; }

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
    uint8_t determineLaneCapacity(fl::span<const ChannelDataPtr> channels);
    /// @brief SPI channel state (per-pin tracking)
    struct SpiChannelState {
        spi_host_device_t spi_host;        ///< SPI peripheral (SPI2_HOST, SPI3_HOST, etc.)
        spi_device_handle_t spi_device;    ///< Device handle from spi_bus_add_device
        gpio_num_t pin;                    ///< Output GPIO pin (data0/MOSI)
        SpiTimingConfig timing;            ///< Timing configuration
        volatile bool transmissionComplete; ///< Set when all encoding/transmission complete
        bool inUse;                        ///< Channel active flag
        bool useDMA;                       ///< DMA enabled for this channel

        // Multi-lane SPI support (dual/quad modes)
        uint8_t numLanes;                  ///< Number of active data lanes (1, 2, or 4)
        gpio_num_t data1_pin;              ///< Data1/MISO pin for dual/quad mode (-1 if unused)
        gpio_num_t data2_pin;              ///< Data2/WP pin for quad mode (-1 if unused)
        gpio_num_t data3_pin;              ///< Data3/HD pin for quad mode (-1 if unused)

        // wave8 encoding LUT (cached to avoid rebuilding in ISR)
        Wave8BitExpansionLut mWave8Lut;    ///< Precomputed wave8 expansion lookup table

        // Event-driven streaming state
        volatile bool hasNewData;          ///< Set by post_cb, cleared by timer ISR after posting transaction

        // Double-buffered staging (4KB each for DMA efficiency)
        uint8_t* stagingA;                 ///< Staging buffer A (DMA-capable memory)
        uint8_t* stagingB;                 ///< Staging buffer B (DMA-capable memory)
        uint8_t* currentStaging;           ///< Current staging buffer (points to A or B)
        size_t stagingOffset;              ///< Current write position in staging buffer
        size_t stagingCapacity;            ///< Size of each staging buffer (4096 bytes)

        // LED source data
        const uint8_t* ledSource;          ///< Current position in source LED data
        size_t ledBytesRemaining;          ///< LED bytes left to encode
        ChannelDataPtr sourceData;         ///< Reference to source ChannelData for cleanup

        // Double-buffered transactions
        spi_transaction_t transA;          ///< Transaction A
        spi_transaction_t transB;          ///< Transaction B
        volatile bool transAInFlight;      ///< Transaction A queued/transmitting
        volatile bool transBInFlight;      ///< Transaction B queued/transmitting

        // Timer ISR handle
        fl::isr::isr_handle_t timerHandle; ///< Timer ISR handle
    };

    /// @brief Pending channel data waiting for hardware availability
    struct PendingChannel {
        ChannelDataPtr data;
        int pin;
        SpiTimingConfig timing;
    };

    /// @brief SPI host allocation tracking (global across all channels)
    struct SpiHostTracking {
        spi_host_device_t host;
        int refCount;  ///< Reference counting (multiple channels can share a bus)
        bool initialized; ///< Bus has been initialized
        uint8_t activeLanes; ///< Number of lanes in use (0=unused, 1=single, 2=dual, 4=quad)
    };

    /// @brief Hash function for SpiTimingConfig (future use for caching)
    struct TimingHash {
        size_t operator()(const SpiTimingConfig& t) const {
            // FNV-1a hash algorithm for better distribution
            size_t hash = 2166136261u;
            hash = (hash ^ static_cast<uint32_t>(t.protocol)) * 16777619u;
            hash = (hash ^ t.clock_hz) * 16777619u;
            hash = (hash ^ t.bits_per_led_bit) * 16777619u;
            hash = (hash ^ t.reset_time_us) * 16777619u;
            return hash;
        }
    };

    /// @brief Equality function for SpiTimingConfig (hash map key)
    struct TimingEqual {
        bool operator()(const SpiTimingConfig& a, const SpiTimingConfig& b) const {
            return a == b;
        }
    };

    /// @brief Acquire a channel for given pin and timing
    /// @param dataSize Size of LED data in bytes
    /// @return Pointer to channel state, or nullptr if no hardware available
    SpiChannelState* acquireChannel(gpio_num_t pin, const SpiTimingConfig& timing, size_t dataSize);

    /// @brief Release a channel (marks as available for reuse)
    void releaseChannel(SpiChannelState* channel);

    /// @brief Create new SPI channel with given configuration
    /// @param state Channel state to initialize
    /// @param pin GPIO pin
    /// @param timing SPI timing configuration
    /// @param dataSize Size of LED data in bytes
    /// @return true if channel created successfully
    bool createChannel(SpiChannelState* state, gpio_num_t pin, const SpiTimingConfig& timing, size_t dataSize);

    /// @brief Encode LED data to SPI buffer
    /// @param ledData Source LED data (RGB/RGBW bytes)
    /// @param spiBuffer Output SPI buffer (must be 3x size of ledData for WS2812)
    /// @param timing SPI timing configuration
    /// @return true if encoding successful
    bool encodeLedData(const fl::span<const uint8_t>& ledData,
                       fl::vector<uint8_t>& spiBuffer,
                       const SpiTimingConfig& timing);

    /// @brief Encode a single LED color byte to SPI bits (dynamic pattern)
    /// @param data LED color byte (0-255)
    /// @param buf Output buffer (must be zeroed, sized for bits_per_led_bit * 8 bits)
    /// @param timing SPI timing configuration with bit patterns
    /// @param output_bit_offset Bit offset in output buffer (for non-byte-aligned patterns)
    /// @return Number of bits written to buffer
    static uint32_t encodeLedByte(uint8_t data, uint8_t* buf, const SpiTimingConfig& timing, uint32_t output_bit_offset = 0);

    /// @brief Acquire SPI host for new channel
    /// @return SPI host, or SPI_HOST_MAX on failure
    spi_host_device_t acquireSpiHost();

    /// @brief Release SPI host when channel is destroyed
    void releaseSpiHost(spi_host_device_t host);

    /// @brief Get SpiTimingConfig from ChannelData
    /// @param data Channel data
    /// @return SPI timing configuration (converts from ChipsetTimingConfig)
    SpiTimingConfig getSpiTimingFromChannel(const ChannelDataPtr& data);

    /// @brief Calculate optimal SPI timing from chipset timing
    /// @param chipsetTiming LED chipset timing specification (T1/T2/T3 model)
    /// @return SPI timing configuration with calculated frequency and bit patterns
    static SpiTimingConfig calculateSpiTiming(const ChipsetTimingConfig& chipsetTiming);

    /// @brief SPI post-transaction callback (sets hasNewData flag)
    /// @param trans Completed transaction
    static void IRAM_ATTR spiPostTransactionCallback(spi_transaction_t* trans);

    /// @brief Timer ISR callback (encodes chunks when hasNewData is set)
    /// @param user_data Pointer to SpiChannelState
    static void IRAM_ATTR timerEncodingISR(void* user_data);

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
    uint32_t mLastRetryFrame;

    /// @brief Multi-lane pin configurations (keyed by data0_pin)
    fl::hash_map<gpio_num_t, MultiLanePinConfig> mMultiLaneConfigs;

    // IChannelEngine state management
    /// @brief Channels enqueued via enqueue(), waiting for show()
    fl::vector_inlined<ChannelDataPtr, 16> mEnqueuedChannels;

    /// @brief Channels currently transmitting (tracked by poll() for cleanup)
    fl::vector_inlined<ChannelDataPtr, 16> mTransmittingChannels;
};

} // namespace fl

#endif // FASTLED_ESP32_HAS_CLOCKLESS_SPI

#endif // ESP32
