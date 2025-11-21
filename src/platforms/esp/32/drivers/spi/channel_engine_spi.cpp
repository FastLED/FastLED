/// @file channel_engine_spi.cpp
/// @brief SPI-based ChannelEngine implementation for ESP32

#include "channel_engine_spi.h"

#ifdef ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_CLOCKLESS_SPI

#include "platforms/esp/is_esp.h"  // Platform detection (FL_IS_ESP_32C6, etc.)
#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/math_macros.h"
#include "ftl/time.h"

FL_EXTERN_C_BEGIN
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "soc/spi_periph.h"
FL_EXTERN_C_END

namespace fl {

// Static member initialization
vector_inlined<ChannelEngineSpi::SpiHostTracking, 3> ChannelEngineSpi::sSpiHostUsage;

namespace {
    /// @brief Calculate greatest common divisor using Euclidean algorithm
    constexpr uint32_t gcd(uint32_t a, uint32_t b) {
        while (b != 0) {
            uint32_t temp = b;
            b = a % b;
            a = temp;
        }
        return a;
    }

    /// @brief Calculate GCD of four values
    constexpr uint32_t gcd4(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
        return gcd(gcd(gcd(a, b), c), d);
    }

    /// @brief Count number of set bits in a value
    constexpr uint8_t countBits(uint32_t value) {
        uint8_t count = 0;
        while (value) {
            count++;
            value >>= 1;
        }
        return count;
    }
}

ChannelEngineSpi::ChannelEngineSpi()
    : mAllocationFailed(false)
    , mLastRetryFrame(0)
{
    FL_DBG("ChannelEngineSpi: Constructor called");
}

ChannelEngineSpi::~ChannelEngineSpi() {
    FL_DBG("ChannelEngineSpi: Destructor called");
    mMultiLaneConfigs.clear();

    // Clean up all channels
    for (auto& channel : mChannels) {
        // Detach timer ISR
        if (channel.timerHandle.is_valid()) {
            fl::isr::detachHandler(channel.timerHandle);
        }

        // Free staging buffers
        if (channel.stagingA) {
            heap_caps_free(channel.stagingA);
        }
        if (channel.stagingB) {
            heap_caps_free(channel.stagingB);
        }

        // Remove SPI device
        if (channel.spi_device) {
            // Wait for any pending transactions
            if (channel.transAInFlight || channel.transBInFlight) {
                spi_transaction_t* trans_ptr;
                spi_device_get_trans_result(channel.spi_device, &trans_ptr, pdMS_TO_TICKS(1000));
                if (channel.transAInFlight || channel.transBInFlight) {
                    spi_device_get_trans_result(channel.spi_device, &trans_ptr, pdMS_TO_TICKS(1000));
                }
            }
            spi_bus_remove_device(channel.spi_device);
        }

        // Release SPI host
        if (channel.spi_host != SPI_HOST_MAX) {
            releaseSpiHost(channel.spi_host);
        }
    }
}

void ChannelEngineSpi::configureMultiLanePins(const MultiLanePinConfig& pinConfig) {
    if (pinConfig.data0_pin < 0) {
        FL_WARN("ChannelEngineSpi: Invalid multi-lane config - data0_pin must be >= 0");
        return;
    }

    uint8_t laneCount = pinConfig.getLaneCount();
    FL_DBG("ChannelEngineSpi: Configuring " << static_cast<int>(laneCount)
           << "-lane SPI for pin " << pinConfig.data0_pin
           << " (data0=" << pinConfig.data0_pin
           << ", data1=" << pinConfig.data1_pin
           << ", data2=" << pinConfig.data2_pin
           << ", data3=" << pinConfig.data3_pin << ")");

    // Validate platform capabilities
    #if defined(FL_IS_ESP_32C6) || defined(FL_IS_ESP_32C3) || defined(FL_IS_ESP_32H2)
        // ESP32-C6/C3/H2: Dual-lane max (no quad support)
        if (laneCount > 2) {
            FL_WARN("ChannelEngineSpi: ESP32-C6/C3/H2 only supports dual-lane SPI (max 2 lanes), "
                    << "requested " << static_cast<int>(laneCount) << " lanes");
            return;
        }
    #endif

    // Store the configuration
    mMultiLaneConfigs[pinConfig.data0_pin] = pinConfig;
    FL_DBG("ChannelEngineSpi: Multi-lane configuration stored for pin " << pinConfig.data0_pin);
}

ChannelEngine::EngineState ChannelEngineSpi::pollDerived() {
    bool anyBusy = false;
    bool anyError = false;

    // Check all active channels
    for (auto& channel : mChannels) {
        if (!channel.inUse) continue;

        // Check if streaming is complete
        // Complete when: ledBytesRemaining == 0 AND both transactions finished
        bool streaming_complete = (channel.ledBytesRemaining == 0) &&
                                 !channel.transAInFlight &&
                                 !channel.transBInFlight;

        if (streaming_complete && !channel.transmissionComplete) {
            // All encoding and transmission done
            FL_DBG("ChannelEngineSpi: Streaming complete for pin " << channel.pin);
            channel.transmissionComplete = true;
            releaseChannel(&channel);
        } else if (!streaming_complete) {
            // Still encoding/transmitting
            anyBusy = true;
        }
    }

    // Process pending channels if hardware available
    if (!mPendingChannels.empty() && !anyBusy) {
        processPendingChannels();
    }

    if (anyError) return EngineState::ERROR;
    if (anyBusy) return EngineState::BUSY;
    if (!mPendingChannels.empty()) return EngineState::DRAINING;
    return EngineState::READY;
}

void ChannelEngineSpi::beginTransmission(fl::span<const ChannelDataPtr> channelData) {
    FL_DBG("ChannelEngineSpi: beginTransmission called with " << channelData.size() << " channels");

    for (const auto& data : channelData) {
        gpio_num_t pin = static_cast<gpio_num_t>(data->getPin());

        // Get timing configuration from channel data
        SpiTimingConfig timing = getSpiTimingFromChannel(data);

        // Get LED data from channel
        const auto& ledData = data->getData();
        if (ledData.empty()) {
            FL_WARN("ChannelEngineSpi: Empty LED data for pin " << pin);
            continue;
        }

        // Acquire or reuse channel
        SpiChannelState* channel = acquireChannel(pin, timing, ledData.size());
        if (!channel) {
            // No hardware available - queue for later
            FL_DBG("ChannelEngineSpi: No HW available for pin " << pin << ", queuing");
            mPendingChannels.push_back({data, pin, timing});
            continue;
        }

        // Initialize streaming state for this channel
        channel->ledSource = ledData.data();
        channel->ledBytesRemaining = ledData.size();
        channel->stagingOffset = 0;
        channel->currentStaging = channel->stagingA;
        channel->transAInFlight = false;
        channel->transBInFlight = false;
        channel->transmissionComplete = false;

        // Kick-start the timer ISR by setting hasNewData flag
        // The timer ISR will start encoding chunks and posting transactions
        channel->hasNewData = true;

        FL_DBG("ChannelEngineSpi: Streaming initialized for pin " << channel->pin
               << ", " << ledData.size() << " LED bytes");
    }
}

ChannelEngineSpi::SpiChannelState* ChannelEngineSpi::acquireChannel(
    gpio_num_t pin, const SpiTimingConfig& timing, size_t dataSize) {

    // Try to find existing idle channel with matching pin and timing
    for (auto& channel : mChannels) {
        if (!channel.inUse && channel.pin == pin && channel.timing == timing) {
            FL_DBG("ChannelEngineSpi: Reusing channel for pin " << pin);
            channel.inUse = true;
            channel.transmissionComplete = false;
            channel.hasNewData = false;
            channel.ledBytesRemaining = 0;
            return &channel;
        }
    }

    // No matching idle channel - try to create new one
    SpiChannelState newChannel = {};
    newChannel.pin = pin;
    newChannel.timing = timing;
    newChannel.inUse = true;
    newChannel.transmissionComplete = false;
    newChannel.hasNewData = false;
    newChannel.spi_host = SPI_HOST_MAX;
    newChannel.spi_device = nullptr;

    // Check if there's a multi-lane configuration for this pin
    auto it = mMultiLaneConfigs.find(pin);
    if (it != mMultiLaneConfigs.end()) {
        // Apply multi-lane configuration
        const MultiLanePinConfig& config = it->second;
        newChannel.numLanes = config.getLaneCount();
        newChannel.data1_pin = config.data1_pin;
        newChannel.data2_pin = config.data2_pin;
        newChannel.data3_pin = config.data3_pin;
        FL_DBG("ChannelEngineSpi: Applying " << static_cast<int>(newChannel.numLanes)
               << "-lane configuration for pin " << pin);
    } else {
        // Default to single-lane mode
        newChannel.numLanes = 1;
        newChannel.data1_pin = static_cast<gpio_num_t>(-1);
        newChannel.data2_pin = static_cast<gpio_num_t>(-1);
        newChannel.data3_pin = static_cast<gpio_num_t>(-1);
    }

    if (!createChannel(&newChannel, pin, timing, dataSize)) {
        FL_WARN("ChannelEngineSpi: Failed to create channel for pin " << pin);
        return nullptr;
    }

    // Add to channel list
    mChannels.push_back(newChannel);
    FL_DBG("ChannelEngineSpi: Created new channel for pin " << pin << " (total: " << mChannels.size() << ")");
    return &mChannels.back();
}

void ChannelEngineSpi::releaseChannel(SpiChannelState* channel) {
    if (channel) {
        FL_DBG("ChannelEngineSpi: Releasing channel for pin " << channel->pin);
        channel->inUse = false;
        channel->transmissionComplete = false;
        channel->hasNewData = false;
        channel->ledBytesRemaining = 0;
    }
}

bool ChannelEngineSpi::createChannel(SpiChannelState* state, gpio_num_t pin,
                                      const SpiTimingConfig& timing, size_t dataSize) {
    FL_DBG("ChannelEngineSpi: Creating channel for pin " << pin);

    // Acquire SPI host
    state->spi_host = acquireSpiHost();
    if (state->spi_host == SPI_HOST_MAX) {
        FL_WARN("ChannelEngineSpi: No available SPI host");
        return false;
    }

    // Calculate buffer size (variable expansion ratio based on timing)
    // Total bits = dataSize * 8 * timing.bits_per_led_bit
    // Convert to bytes (round up)
    const size_t total_bits = dataSize * 8 * timing.bits_per_led_bit;
    const size_t spiBufferSize = (total_bits + 7) / 8;  // Round up to nearest byte

    // Determine if we should use DMA
    // Use DMA for larger buffers (>64 bytes) similar to Espressif driver
    state->useDMA = (spiBufferSize > 64);

    // Configure SPI bus with multi-lane support
    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = pin;                           // Data0 (always present)
    bus_config.miso_io_num = state->data1_pin;              // Data1 for dual/quad mode (-1 if unused)
    bus_config.sclk_io_num = -1;                            // Not used (data-only mode)
    bus_config.quadwp_io_num = state->data2_pin;            // Data2 for quad mode (-1 if unused)
    bus_config.quadhd_io_num = state->data3_pin;            // Data3 for quad mode (-1 if unused)
    bus_config.max_transfer_sz = spiBufferSize;

    // Set flags based on lane count (1, 2, or 4 lanes)
    bus_config.flags = SPICOMMON_BUSFLAG_MASTER;
    if (state->numLanes >= 4) {
        bus_config.flags |= SPICOMMON_BUSFLAG_QUAD;  // Quad mode (4 lanes)
    } else if (state->numLanes >= 2) {
        bus_config.flags |= SPICOMMON_BUSFLAG_DUAL;  // Dual mode (2 lanes)
    }
    // else: standard SPI (1 data line)

    // Initialize SPI bus
    esp_err_t ret = spi_bus_initialize(state->spi_host, &bus_config,
                                       state->useDMA ? SPI_DMA_CH_AUTO : SPI_DMA_DISABLED);
    if (ret != ESP_OK) {
        FL_WARN("ChannelEngineSpi: spi_bus_initialize failed: " << ret);
        releaseSpiHost(state->spi_host);
        state->spi_host = SPI_HOST_MAX;
        return false;
    }

    // Configure SPI device
    spi_device_interface_config_t dev_config = {};
    dev_config.command_bits = 0;
    dev_config.address_bits = 0;
    dev_config.dummy_bits = 0;
    dev_config.clock_speed_hz = timing.clock_hz;
    dev_config.mode = 0;  // SPI mode 0
    dev_config.spics_io_num = -1;  // No CS pin
    dev_config.queue_size = 4;  // Transaction queue size
    dev_config.post_cb = spiPostTransactionCallback;  // ISR callback when transaction completes

    // Set HALFDUPLEX flag for multi-lane modes (required for dual/quad SPI)
    if (state->numLanes >= 2) {
        dev_config.flags = SPI_DEVICE_HALFDUPLEX;
    } else {
        dev_config.flags = 0;
    }

    // Add device to bus
    ret = spi_bus_add_device(state->spi_host, &dev_config, &state->spi_device);
    if (ret != ESP_OK) {
        FL_WARN("ChannelEngineSpi: spi_bus_add_device failed: " << ret);
        spi_bus_free(state->spi_host);
        releaseSpiHost(state->spi_host);
        state->spi_host = SPI_HOST_MAX;
        return false;
    }

    // Verify actual clock frequency
    int actual_freq_khz = 0;
    spi_device_get_actual_freq(state->spi_device, &actual_freq_khz);
    int requested_freq_khz = timing.clock_hz / 1000;
    if (actual_freq_khz < requested_freq_khz - 300 || actual_freq_khz > requested_freq_khz + 300) {
        FL_WARN("ChannelEngineSpi: Clock frequency mismatch - requested " << requested_freq_khz
                << " kHz, actual " << actual_freq_khz << " kHz");
    }

    // Allocate double-buffered staging buffers (4KB each, DMA-capable)
    const size_t staging_size = 4096;

    state->stagingA = (uint8_t*)heap_caps_aligned_alloc(4, staging_size,
                                                        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    state->stagingB = (uint8_t*)heap_caps_aligned_alloc(4, staging_size,
                                                        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    if (!state->stagingA || !state->stagingB) {
        FL_WARN("ChannelEngineSpi: Failed to allocate staging buffers");
        if (state->stagingA) heap_caps_free(state->stagingA);
        if (state->stagingB) heap_caps_free(state->stagingB);
        spi_bus_remove_device(state->spi_device);
        spi_bus_free(state->spi_host);
        releaseSpiHost(state->spi_host);
        return false;
    }

    state->stagingCapacity = staging_size;
    state->currentStaging = state->stagingA;
    state->stagingOffset = 0;

    // Initialize streaming state
    state->hasNewData = false;
    state->ledSource = nullptr;
    state->ledBytesRemaining = 0;
    state->transAInFlight = false;
    state->transBInFlight = false;

    // Set up timer ISR (4 kHz = 250μs period)
    fl::isr::isr_config_t isr_config;
    isr_config.handler = timerEncodingISR;
    isr_config.user_data = state;
    isr_config.frequency_hz = 4000;  // 4 kHz
    isr_config.flags = 0;  // Auto-reload

    fl::isr::isr_handle_t isr_handle;
    int isr_ret = fl::isr::attachTimerHandler(isr_config, &isr_handle);
    if (isr_ret != 0) {
        FL_WARN("ChannelEngineSpi: Failed to attach timer ISR: " << isr_ret);
        heap_caps_free(state->stagingA);
        heap_caps_free(state->stagingB);
        spi_bus_remove_device(state->spi_device);
        spi_bus_free(state->spi_host);
        releaseSpiHost(state->spi_host);
        return false;
    }

    state->timerHandle = isr_handle;

    FL_DBG("ChannelEngineSpi: Channel created successfully - pin=" << pin
           << ", lanes=" << static_cast<int>(state->numLanes)
           << " (data0=" << pin
           << ", data1=" << state->data1_pin
           << ", data2=" << state->data2_pin
           << ", data3=" << state->data3_pin << ")"
           << ", host=" << state->spi_host
           << ", dma=" << state->useDMA
           << ", freq=" << timing.clock_hz << " Hz"
           << ", expansion=" << static_cast<int>(timing.bits_per_led_bit) << "x"
           << ", staging=" << staging_size << " bytes × 2");

    return true;
}

bool ChannelEngineSpi::encodeLedData(const fl::span<const uint8_t>& ledData,
                                      fl::vector<uint8_t>& spiBuffer,
                                      const SpiTimingConfig& timing) {
    // Calculate required SPI buffer size (variable expansion ratio)
    // Total bits = ledData.size() * 8 * timing.bits_per_led_bit
    // Convert to bytes (round up)
    const size_t total_bits = ledData.size() * 8 * timing.bits_per_led_bit;
    const size_t spiSize = (total_bits + 7) / 8;  // Round up to nearest byte

    spiBuffer.clear();
    spiBuffer.resize(spiSize);

    // Zero the buffer (required for bit encoding)
    fl::memset(spiBuffer.data(), 0, spiSize);

    // Encode each LED byte using dynamic pattern
    uint32_t output_bit_offset = 0;
    for (size_t i = 0; i < ledData.size(); i++) {
        output_bit_offset += encodeLedByte(ledData[i], spiBuffer.data(), timing, output_bit_offset);
    }

    FL_DBG("ChannelEngineSpi: Encoded " << ledData.size() << " LED bytes to "
           << spiSize << " SPI bytes (" << output_bit_offset << " bits)");

    return true;
}

uint32_t ChannelEngineSpi::encodeLedByte(uint8_t data, uint8_t* buf, const SpiTimingConfig& timing, uint32_t output_bit_offset) {
    // Dynamic encoding: Each LED bit expands to timing.bits_per_led_bit SPI bits
    // using the bit patterns from timing.bit0_pattern and timing.bit1_pattern
    //
    // Process LED byte MSB first (bit 7 → bit 0)

    uint32_t current_bit_offset = output_bit_offset;

    for (int led_bit = 7; led_bit >= 0; led_bit--) {
        // Select pattern based on LED bit value
        const uint32_t pattern = (data & (1 << led_bit)) ? timing.bit1_pattern : timing.bit0_pattern;
        const uint8_t pattern_bits = timing.bits_per_led_bit;

        // Write pattern bits to buffer (MSB first)
        for (uint8_t i = 0; i < pattern_bits; i++) {
            // Get bit from pattern (MSB first)
            const uint8_t pattern_bit = (pattern >> (pattern_bits - 1 - i)) & 1;

            if (pattern_bit) {
                // Set bit in output buffer
                const uint32_t byte_index = current_bit_offset / 8;
                const uint8_t bit_index = 7 - (current_bit_offset % 8);  // MSB first
                buf[byte_index] |= (1 << bit_index);
            }

            current_bit_offset++;
        }
    }

    return current_bit_offset - output_bit_offset;  // Return number of bits written
}

spi_host_device_t ChannelEngineSpi::acquireSpiHost() {
    // SPI host priority: SPI2 → SPI3 → SPI1
    // Note: SPI1 is often used for flash, so it's last priority
    static const spi_host_device_t hosts[] = {
#ifdef SPI2_HOST
        SPI2_HOST,
#endif
#ifdef SPI3_HOST
        SPI3_HOST,
#endif
#ifdef SPI1_HOST
        SPI1_HOST,
#endif
    };
    static const size_t num_hosts = sizeof(hosts) / sizeof(hosts[0]);

    // Try to find an available host
    for (size_t i = 0; i < num_hosts; i++) {
        spi_host_device_t host = hosts[i];

        // Find or create tracking entry
        SpiHostTracking* tracking = nullptr;
        for (auto& entry : sSpiHostUsage) {
            if (entry.host == host) {
                tracking = &entry;
                break;
            }
        }

        if (!tracking) {
            // First use of this host - create tracking entry
            SpiHostTracking newTracking = {};
            newTracking.host = host;
            newTracking.refCount = 0;
            newTracking.initialized = false;
            newTracking.activeLanes = 0;  // No lanes in use yet
            sSpiHostUsage.push_back(newTracking);
            tracking = &sSpiHostUsage.back();
        }

        // ESP32 SPI limitation: Each host can only have one bus configuration
        // This means one set of pins (data0/data1/data2/data3) per host
        // Multi-lane (dual/quad) uses multiple pins on the SAME host
        // So we limit to refCount == 0 (not yet in use)
        //
        // Future enhancement: Could allow multiple channels to share the same host
        // if they have identical pin configurations (bus sharing)
        if (tracking->refCount == 0) {
            tracking->refCount++;
            tracking->initialized = true;
            FL_DBG("ChannelEngineSpi: Acquired SPI host " << host << " (refCount=" << tracking->refCount << ")");
            return host;
        }
    }

    FL_WARN("ChannelEngineSpi: No available SPI hosts (max " << num_hosts << " hosts)");
    return SPI_HOST_MAX;
}

void ChannelEngineSpi::releaseSpiHost(spi_host_device_t host) {
    for (auto& entry : sSpiHostUsage) {
        if (entry.host == host) {
            if (entry.refCount > 0) {
                entry.refCount--;
                FL_DBG("ChannelEngineSpi: Released SPI host " << host << " (refCount=" << entry.refCount << ")");

                if (entry.refCount == 0) {
                    // Free the SPI bus
                    spi_bus_free(host);
                    entry.initialized = false;
                    FL_DBG("ChannelEngineSpi: Freed SPI bus " << host);
                }
            }
            return;
        }
    }
}

SpiTimingConfig ChannelEngineSpi::calculateSpiTiming(const ChipsetTimingConfig& chipsetTiming) {
    // Use symmetric 3-phase timing (T1/T2/T3) directly
    // All LED timings are symmetric: both bit 0 and bit 1 have period = T1 + T2 + T3
    //
    // For bit '0': high for T1, low for T2+T3
    // For bit '1': high for T1+T2, low for T3

    const uint32_t t1_ns = chipsetTiming.t1_ns;
    const uint32_t t2_ns = chipsetTiming.t2_ns;
    const uint32_t t3_ns = chipsetTiming.t3_ns;

    FL_DBG("ChannelEngineSpi: Chipset 3-phase timing - T1=" << t1_ns << "ns, T2=" << t2_ns
           << "ns, T3=" << t3_ns << "ns (period=" << (t1_ns + t2_ns + t3_ns) << "ns)");

    // Find GCD of original timings to get the optimal time quantum
    // This provides best memory efficiency while maintaining perfect timing accuracy
    uint32_t quantum_ns = gcd(gcd(t1_ns, t2_ns), t3_ns);

    // Ensure minimum quantum of 10ns to avoid excessively high frequencies
    if (quantum_ns < 10) {
        FL_WARN("ChannelEngineSpi: Quantum too small (" << quantum_ns << "ns), rounding up to 10ns");
        quantum_ns = 10;
    }

    // Calculate how many quanta each phase needs (use original timings)
    const uint32_t t1_quanta = (t1_ns + quantum_ns / 2) / quantum_ns;  // Round to nearest
    const uint32_t t2_quanta = (t2_ns + quantum_ns / 2) / quantum_ns;
    const uint32_t t3_quanta = (t3_ns + quantum_ns / 2) / quantum_ns;

    // Calculate SPI frequency: 1 / quantum_time
    // freq = 1 / (quantum_ns * 1e-9) = 1e9 / quantum_ns
    const uint32_t spi_freq_hz = 1000000000UL / quantum_ns;

    // Total bits per LED bit (symmetric for both bit 0 and bit 1)
    const uint32_t bits_per_led_bit = t1_quanta + t2_quanta + t3_quanta;

    // Validate maximum bit pattern length (32 bits max for storage)
    if (bits_per_led_bit > 32) {
        FL_WARN("ChannelEngineSpi: Bit pattern too long (" << bits_per_led_bit
                << " bits), timing may not be achievable with SPI");
        return SpiTimingConfig::ws2812(chipsetTiming.reset_us);  // Fall back to WS2812
    }

    // Generate bit patterns using 3-phase symmetric timing
    // Bit pattern: MSB first, 1 = high, 0 = low
    //
    // Bit '0': high for T1 quanta, low for T2+T3 quanta
    // Bit '1': high for T1+T2 quanta, low for T3 quanta

    uint32_t bit0_pattern = 0;
    uint32_t bit1_pattern = 0;

    // Set high bits for bit0 (T1 quanta high, T2+T3 low)
    for (uint32_t i = 0; i < t1_quanta; i++) {
        bit0_pattern |= (1UL << (bits_per_led_bit - 1 - i));
    }

    // Set high bits for bit1 (T1+T2 quanta high, T3 low)
    for (uint32_t i = 0; i < (t1_quanta + t2_quanta); i++) {
        bit1_pattern |= (1UL << (bits_per_led_bit - 1 - i));
    }

    // Calculate actual achieved timing (for validation/debugging)
    const uint32_t ns_per_bit = quantum_ns;
    const uint32_t achieved_t0h_ns = t1_quanta * ns_per_bit;           // Bit '0' high time = T1
    const uint32_t achieved_t0l_ns = (t2_quanta + t3_quanta) * ns_per_bit;  // Bit '0' low time = T2+T3
    const uint32_t achieved_t1h_ns = (t1_quanta + t2_quanta) * ns_per_bit;  // Bit '1' high time = T1+T2
    const uint32_t achieved_t1l_ns = t3_quanta * ns_per_bit;           // Bit '1' low time = T3

    FL_DBG("ChannelEngineSpi: Calculated SPI timing - freq=" << spi_freq_hz << " Hz, "
           << "bits_per_led_bit=" << bits_per_led_bit);
    FL_DBG("ChannelEngineSpi: Bit patterns - bit0=" << bit0_pattern
           << ", bit1=" << bit1_pattern);
    FL_DBG("ChannelEngineSpi: Achieved timing - T0H=" << achieved_t0h_ns << "ns, T0L=" << achieved_t0l_ns
           << "ns, T1H=" << achieved_t1h_ns << "ns, T1L=" << achieved_t1l_ns << "ns");

    // Construct SpiTimingConfig
    SpiTimingConfig config;
    config.protocol = SpiTimingConfig::CUSTOM;
    config.clock_hz = spi_freq_hz;
    config.bits_per_led_bit = static_cast<uint8_t>(bits_per_led_bit);
    config.reset_time_us = chipsetTiming.reset_us;
    config.bit0_pattern = bit0_pattern;
    config.bit0_count = static_cast<uint8_t>(bits_per_led_bit);
    config.bit1_pattern = bit1_pattern;
    config.bit1_count = static_cast<uint8_t>(bits_per_led_bit);
    config.achieved_t0h_ns = achieved_t0h_ns;
    config.achieved_t0l_ns = achieved_t0l_ns;
    config.achieved_t1h_ns = achieved_t1h_ns;
    config.achieved_t1l_ns = achieved_t1l_ns;

    return config;
}

SpiTimingConfig ChannelEngineSpi::getSpiTimingFromChannel(const ChannelDataPtr& data) {
    const auto& chipsetTiming = data->getTiming();

    // Calculate optimal SPI timing from chipset specification
    SpiTimingConfig spiTiming = calculateSpiTiming(chipsetTiming);

    FL_DBG("ChannelEngineSpi: Using calculated SPI timing - clock=" << spiTiming.clock_hz
           << " Hz, reset=" << spiTiming.reset_time_us << " μs");

    return spiTiming;
}

void ChannelEngineSpi::processPendingChannels() {
    // Try to process pending channels now that hardware may be available
    fl::vector_inlined<PendingChannel, 16> stillPending;

    for (auto& pending : mPendingChannels) {
        gpio_num_t pin = static_cast<gpio_num_t>(pending.pin);
        const auto& ledData = pending.data->getData();

        SpiChannelState* channel = acquireChannel(pin, pending.timing, ledData.size());
        if (!channel) {
            // Still no hardware - keep pending
            stillPending.push_back(pending);
            continue;
        }

        // Successfully acquired channel - initialize streaming
        channel->ledSource = ledData.data();
        channel->ledBytesRemaining = ledData.size();
        channel->stagingOffset = 0;
        channel->currentStaging = channel->stagingA;
        channel->transAInFlight = false;
        channel->transBInFlight = false;
        channel->transmissionComplete = false;

        // Kick-start the timer ISR
        channel->hasNewData = true;

        FL_DBG("ChannelEngineSpi: Streaming initialized for pending channel on pin " << pin);
    }

    // Replace pending list with those that couldn't be processed
    mPendingChannels = fl::move(stillPending);
}

void IRAM_ATTR ChannelEngineSpi::spiPostTransactionCallback(spi_transaction_t* trans) {
    SpiChannelState* channel = static_cast<SpiChannelState*>(trans->user);

    // Mark which transaction completed
    if (trans == &channel->transA) {
        channel->transAInFlight = false;
    } else if (trans == &channel->transB) {
        channel->transBInFlight = false;
    }

    // Arm timer ISR to start encoding next chunk
    channel->hasNewData = true;
}

void IRAM_ATTR ChannelEngineSpi::timerEncodingISR(void* user_data) {
    SpiChannelState* channel = static_cast<SpiChannelState*>(user_data);

    // Check if armed by transaction complete callback
    if (!channel->hasNewData) {
        return;  // No work, exit fast
    }

    // Check if there's data left to encode
    if (channel->ledBytesRemaining == 0) {
        channel->hasNewData = false;
        channel->transmissionComplete = true;
        return;  // All done
    }

    // Encode one chunk with lane-scaled throughput
    // Single-lane: 40 LEDs per ISR fire (250μs period)
    // Dual-lane: 80 LEDs (2x throughput, 2x encoding needed)
    // Quad-lane: 160 LEDs (4x throughput, 4x encoding needed)
    const size_t base_chunk_leds = 40;
    const size_t chunk_size_leds = base_chunk_leds * channel->numLanes;
    const size_t chunk_size_bytes = chunk_size_leds * 3;  // RGB

    size_t bytes_to_encode = fl_min(chunk_size_bytes, channel->ledBytesRemaining);

    // Encode into staging buffer at current offset
    uint32_t bit_offset = channel->stagingOffset * 8;  // Convert byte offset to bit offset
    for (size_t i = 0; i < bytes_to_encode; i++) {
        bit_offset += encodeLedByte(channel->ledSource[i],
                                    channel->currentStaging,
                                    channel->timing,
                                    bit_offset);
    }

    // Update staging buffer position (convert back to bytes)
    size_t bytes_written = (bit_offset + 7) / 8;
    channel->stagingOffset = bytes_written;

    // Update source data position
    channel->ledSource += bytes_to_encode;
    channel->ledBytesRemaining -= bytes_to_encode;

    // Check if staging buffer is full or this is the last chunk
    bool buffer_full = (channel->stagingOffset >= channel->stagingCapacity);
    bool last_chunk = (channel->ledBytesRemaining == 0);

    if (buffer_full || last_chunk) {
        // Select which transaction to use
        spi_transaction_t* trans = channel->transAInFlight ? &channel->transB : &channel->transA;

        // Setup transaction with multi-lane flags
        trans->length = channel->stagingOffset * 8;  // Convert to bits
        trans->tx_buffer = channel->currentStaging;
        trans->user = channel;

        // Set transaction mode based on lane count (1, 2, or 4 lanes)
        if (channel->numLanes >= 4) {
            trans->flags = SPI_TRANS_MODE_QIO;  // Quad I/O mode
        } else if (channel->numLanes >= 2) {
            trans->flags = SPI_TRANS_MODE_DIO;  // Dual I/O mode
        } else {
            trans->flags = 0;  // Standard SPI mode
        }

        // Try to queue transaction (non-blocking in ISR)
        esp_err_t ret = spi_device_queue_trans(channel->spi_device, trans, 0);

        if (ret == ESP_OK) {
            // Transaction posted successfully
            if (trans == &channel->transA) {
                channel->transAInFlight = true;
            } else {
                channel->transBInFlight = true;
            }

            // Flip to other staging buffer for next accumulation
            channel->currentStaging = (channel->currentStaging == channel->stagingA) ?
                                      channel->stagingB : channel->stagingA;
            channel->stagingOffset = 0;

            // Disarm until next transaction completes
            channel->hasNewData = false;
        }
        // If queue full (ret != ESP_OK), keep hasNewData=true and retry next ISR fire
    }
}

}  // namespace fl

#endif // FASTLED_ESP32_HAS_CLOCKLESS_SPI

#endif // ESP32
