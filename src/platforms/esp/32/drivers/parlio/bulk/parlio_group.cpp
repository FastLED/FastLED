/// @file parlio_group.cpp
/// @brief Implementation of ParlioGroup and BulkControllerImpl for ESP32-P4 PARLIO peripheral
///
/// This file contains:
/// - ParlioGroup: Group/write coordinator logic for strips with same timing
/// - BulkControllerImpl: User-facing bulk API wrapper

#ifdef CONFIG_IDF_TARGET_ESP32P4

#include "parlio_group.h"
#include "parlio_manager.h"
#include "parlio_channel.h"  // IParlioChannel
#include "parlio_engine.h"
#include "pixel_iterator.h"
#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/engine_events.h"
#include "fl/clockless/base.h"
#include "fl/rectangular_draw_buffer.h"
#include "fl/unique_ptr.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/led_timing.h"  // makeTimingConfig
#include "fl/map.h"                  // fl_map for chipset registry
#include "fl/fixed_vector.h"
#include "fl/span.h"
#include "fl/cstring.h"
#include "esp_heap_caps.h"
#include "crgb.h"

// Forward declaration from legacy controller (will be removed in Task 5.1)
namespace fl {
fl::unique_ptr<fl::IParlioChannel> createParlioChannelRuntime(uint8_t width, const ChipsetTimingConfig& timing);
}

namespace fl {

// ===== Concrete ParlioGroup Implementation =====

// Forward declaration of concrete class
class ParlioGroup : public IParlioGroup {
public:
    /// Frame lifecycle state tracking
    enum QueueState { IDLE, QUEUING, FLUSHED };

    /// @brief Driver factory function type
    /// Creates a channel driver of the correct width and chipset type
    using DriverFactory = fl::unique_ptr<fl::IParlioChannel> (*)(uint8_t width, const fl::ChipsetTimingConfig& timing);

    /// @brief Constructor with chipset timing configuration
    /// @param timing Chipset timing configuration for this group
    /// @param factory Driver factory function for creating channels
    explicit ParlioGroup(const ChipsetTimingConfig& timing, DriverFactory factory);

    /// @brief Destructor
    ~ParlioGroup() override;

    // Interface implementations
    void onQueuingStart() override;
    bool isQueuing() const override;
    void onQueuingDone() override;
    void registerStrip(uint8_t pin, CRGB* buffer, int count, bool is_rgbw) override;
    void unregisterStrip(uint8_t pin) override;
    void writePixels(uint8_t data_pin, PixelIterator& pixel_iterator) override;
    void flush() override;
    void prepare_write() override;
    ParlioHardwareConfig get_hardware_config() override;
    const uint8_t* get_dma_buffer() override;
    uint32_t get_total_bits() override;
    uint32_t get_bits_per_component() override;

private:
    /// @brief Validate GPIO pin for ESP32-P4 PARLIO peripheral
    static bool validatePin(uint8_t pin);

    /// @brief Select optimal width (1, 2, 4, 8, or 16) based on strip count
    uint8_t selectOptimalWidth(uint8_t num_strips);

    /// @brief Generate bitpatterns from timing configuration
    void generate_bitpatterns();

    /// @brief Pack pixel data into DMA buffer
    /// @param output_buffer DMA-capable buffer to pack data into
    void pack_data(uint8_t* output_buffer);

    /// @brief Calculate optimal clock frequency based on LED count
    uint32_t calculate_optimal_clock_freq(uint16_t num_leds, uint32_t base_freq);

    /// Chipset timing configuration
    ChipsetTimingConfig mTimingConfig;

    /// Frame lifecycle state
    QueueState mQueueState;

    /// Rectangular draw buffer (manages strip registration and pixel data)
    RectangularDrawBuffer mRectDrawBuffer;

    /// Channel driver instance (delegates to IParlioChannel)
    fl::unique_ptr<IParlioChannel> mDriver;

    /// Driver factory function
    DriverFactory mDriverFactory;

    /// Bitpattern lookup table for waveform generation (16 nibble patterns)
    uint16_t mBitpatterns[16];

    /// Minimal strip tracking for DMA packing
    struct StripInfo {
        CRGB* buffer;
        int count;
        bool is_rgbw;
    };
    fl_map<int, StripInfo> mStripInfo;  // Map: pin → strip info

    // DMA-related fields
    /// DMA buffer for packed pixel data
    uint8_t* mDmaBuffer;

    /// DMA buffer size in bytes
    size_t mDmaBufferSize;

    /// Data width (1, 2, 4, 8, or 16 lanes)
    uint8_t mDataWidth;
};

// ===== ParlioGroup Factory Implementation =====

// Static registry mapping timing configs to ParlioGroup instances
// Each unique timing config gets its own ParlioGroup singleton
static fl_map<uint32_t, IParlioGroup*>& getGroupRegistry() {
    static fl_map<uint32_t, IParlioGroup*> registry;
    return registry;
}

// Hash function for ChipsetTimingConfig
static uint32_t hashTiming(const ChipsetTimingConfig& timing) {
    // Simple hash: XOR all timing values
    // Note: This could produce collisions, but unlikely for LED chipsets
    return timing.t1_ns ^ timing.t2_ns ^ timing.t3_ns ^ timing.reset_us;
}

// Driver factory function for ParlioGroup constructor
// This lambda creates IParlioChannel instances with the correct width and timing
static ParlioGroup::DriverFactory getDriverFactory() {
    // Return stateless lambda that can be converted to function pointer
    return [](uint8_t width, const ChipsetTimingConfig& timing) -> fl::unique_ptr<fl::IParlioChannel> {
        return createParlioChannelRuntime(width, timing);
    };
}

// Static method - returns interface but creates concrete singleton
IParlioGroup& IParlioGroup::getOrCreate(const ChipsetTimingConfig& timing) {
    // Hash the timing to find existing instance
    uint32_t hash = hashTiming(timing);

    auto& registry = getGroupRegistry();
    auto it = registry.find(hash);

    if (it != registry.end()) {
        return *it->second;
    }

    // Create new ParlioGroup instance for this chipset timing
    ParlioGroup* group = new ParlioGroup(timing, getDriverFactory());
    registry[hash] = group;

    return *group;
}

// ===== Helper Functions (migrated from legacy/parlio_channel.cpp) =====
namespace detail {

// Runtime waveform generator using chipset timing configuration
inline uint32_t generate_waveform(uint8_t value, const uint16_t* patterns) {
    const uint16_t p1 = patterns[value >> 4];    // High nibble
    const uint16_t p2 = patterns[value & 0x0F];  // Low nibble
    return (uint32_t(p2) << 16) | p1;
}

// Pack 32 time-slices for 1-bit width (32 slices → 4 bytes)
inline void process_1bit(uint8_t* buffer, const uint32_t* slices) {
    uint32_t packed = 0;
    for (int i = 0; i < 32; ++i) {
        if (slices[i] & 0x01) packed |= (1 << i);
    }
    *reinterpret_cast<uint32_t*>(buffer) = packed;
}

// Pack 32 time-slices for 2-bit width (32 slices → 8 bytes)
inline void process_2bit(uint8_t* buffer, const uint32_t* slices) {
    uint32_t* out = reinterpret_cast<uint32_t*>(buffer);
    uint32_t word0 = 0, word1 = 0;
    for (int i = 0; i < 16; ++i) {
        word0 |= ((slices[i] & 0x03) << (i * 2));
        word1 |= ((slices[i+16] & 0x03) << (i * 2));
    }
    out[0] = word0;
    out[1] = word1;
}

// Pack 32 time-slices for 4-bit width (32 slices → 16 bytes)
inline void process_4bit(uint8_t* buffer, const uint32_t* slices) {
    uint32_t* out = reinterpret_cast<uint32_t*>(buffer);
    uint32_t word0 = 0, word1 = 0, word2 = 0, word3 = 0;
    for (int i = 0; i < 8; ++i) {
        word0 |= ((slices[i] & 0x0F) << (i * 4));
        word1 |= ((slices[i+8] & 0x0F) << (i * 4));
        word2 |= ((slices[i+16] & 0x0F) << (i * 4));
        word3 |= ((slices[i+24] & 0x0F) << (i * 4));
    }
    out[0] = word0; out[1] = word1; out[2] = word2; out[3] = word3;
}

// Pack 32 time-slices for 8-bit width (32 slices → 32 bytes)
inline void process_8bit(uint8_t* buffer, const uint32_t* slices) {
    uint32_t* out = reinterpret_cast<uint32_t*>(buffer);
    for (int i = 0; i < 8; ++i) {
        const int base = i * 4;
        out[i] = (slices[base] & 0xFF) |
                 ((slices[base+1] & 0xFF) << 8) |
                 ((slices[base+2] & 0xFF) << 16) |
                 ((slices[base+3] & 0xFF) << 24);
    }
}

// Pack 32 time-slices for 16-bit width (32 slices → 64 bytes)
inline void process_16bit(uint8_t* buffer, const uint32_t* slices) {
    uint32_t* out = reinterpret_cast<uint32_t*>(buffer);
    for (int i = 0; i < 16; ++i) {
        const int base = i * 2;
        out[i] = (slices[base] & 0xFFFF) |
                 ((slices[base+1] & 0xFFFF) << 16);
    }
}

} // namespace detail

// ===== ParlioGroup Implementation =====

ParlioGroup::ParlioGroup(const ChipsetTimingConfig& timing, DriverFactory factory)
    : mTimingConfig(timing)
    , mQueueState(IDLE)
    , mDriverFactory(factory)
    , mBitpatterns{}
    , mDmaBuffer(nullptr)
    , mDmaBufferSize(0)
    , mDataWidth(0) {
    // Generate bitpatterns from timing configuration
    generate_bitpatterns();
}

ParlioGroup::~ParlioGroup() {
    mDriver.reset();
    if (mDmaBuffer) {
        heap_caps_free(mDmaBuffer);
        mDmaBuffer = nullptr;
    }
}

void ParlioGroup::onQueuingStart() {
    if (mQueueState == QUEUING) {
        FL_DBG("PARLIO Group: Already queuing");
        return;
    }
    FL_DBG("PARLIO Group: Start queuing");
    mQueueState = QUEUING;
    mRectDrawBuffer.onQueuingStart();
}

bool ParlioGroup::isQueuing() const {
    return mQueueState == QUEUING;
}

void ParlioGroup::onQueuingDone() {
    mRectDrawBuffer.onQueuingDone();
}

void ParlioGroup::registerStrip(uint8_t pin, CRGB* buffer, int count, bool is_rgbw) {
    if (!validatePin(pin)) {
        FL_WARN("PARLIO Group: Invalid pin " << int(pin));
        return;
    }

    // Check LED count uniformity
    if (!mRectDrawBuffer.mDrawList.empty()) {
        uint16_t existing_led_count = mRectDrawBuffer.mDrawList[0].mNumBytes / (is_rgbw ? 4 : 3);
        if (static_cast<uint16_t>(count) != existing_led_count) {
            FL_WARN("PARLIO Group: LED count mismatch");
            return;
        }
    }

    // Check RGBW consistency
    if (!mRectDrawBuffer.mDrawList.empty()) {
        bool existing_is_rgbw = (mRectDrawBuffer.mDrawList[0].mNumBytes % 4 == 0);
        if (is_rgbw != existing_is_rgbw) {
            FL_WARN("PARLIO Group: RGBW mode mismatch");
            return;
        }
    }

    if (mRectDrawBuffer.mDrawList.size() >= 16) {
        FL_WARN("PARLIO Group: Max 16 strips");
        return;
    }

    // Store minimal strip info
    mStripInfo[pin] = {buffer, count, is_rgbw};

    FL_DBG("PARLIO Group: Register strip pin=" << int(pin) << " leds=" << count);
    mRectDrawBuffer.queue(DrawItem(pin, static_cast<uint16_t>(count), is_rgbw));
}

void ParlioGroup::unregisterStrip(uint8_t pin) {
    // Remove from strip info map
    auto it = mStripInfo.find(pin);
    if (it != mStripInfo.end()) {
        mStripInfo.erase(it);
        FL_DBG("PARLIO Group: Unregistered strip pin=" << int(pin));
    }
    // Note: RectangularDrawBuffer will be reconfigured on next frame
}

void ParlioGroup::writePixels(uint8_t data_pin, PixelIterator& pixel_iterator) {
    fl::span<uint8_t> strip_bytes = mRectDrawBuffer.getLedsBufferBytesForPin(data_pin, true);
    const Rgbw rgbw = pixel_iterator.get_rgbw();

    if (rgbw.active()) {
        uint8_t r, g, b, w;
        while (pixel_iterator.has(1)) {
            pixel_iterator.loadAndScaleRGBW(&r, &g, &b, &w);
            strip_bytes[0] = r;
            strip_bytes[1] = g;
            strip_bytes[2] = b;
            strip_bytes[3] = w;
            strip_bytes.pop_front();
            strip_bytes.pop_front();
            strip_bytes.pop_front();
            strip_bytes.pop_front();
            pixel_iterator.advanceData();
            pixel_iterator.stepDithering();
        }
    } else {
        uint8_t r, g, b;
        while (pixel_iterator.has(1)) {
            pixel_iterator.loadAndScaleRGB(&r, &g, &b);
            strip_bytes[0] = r;
            strip_bytes[1] = g;
            strip_bytes[2] = b;
            strip_bytes.pop_front();
            strip_bytes.pop_front();
            strip_bytes.pop_front();
            pixel_iterator.advanceData();
            pixel_iterator.stepDithering();
        }
    }
}

void ParlioGroup::flush() {
    if (mQueueState == FLUSHED) {
        FL_DBG("PARLIO Group: Already flushed");
        return;
    }
    mQueueState = FLUSHED;

    if (!mRectDrawBuffer.mAllLedsBufferUint8Size || mRectDrawBuffer.mDrawList.empty()) {
        FL_DBG("PARLIO Group: No data");
        return;
    }

    // ===== NEW ARCHITECTURE (Task 2.1): Register with manager for batched write operations =====
    // Instead of directly calling mDriver->show(), we register with the manager.
    // The manager will call write_all_groups() which will invoke our methods.
    // Full implementation of data packing will come in Task 3.1.

    // NEW ARCHITECTURE ENABLED (Task 3.1 complete, Task 4.1)
    // The new architecture is now fully functional with complete data packing implementation.
    bool use_new_architecture = true;  // Switched to true in Iteration 4

    if (use_new_architecture) {
        // NEW PATH: Register with manager for batched write operations
        FL_DBG("PARLIO Group: Registering with manager for batched write operations");
        IParlioManager& manager = IParlioManager::getInstance();
        manager.onEndShow(this);
        mQueueState = IDLE;
        return;
    }

    // LEGACY PATH: Direct driver write operation (temporary, until Task 3.1)
    bool needs_reconfig = !mDriver.get() || mRectDrawBuffer.mDrawListChangedThisFrame;

    if (needs_reconfig) {
        FL_DBG("PARLIO Group: Reconfiguring driver [LEGACY PATH]");
        mDriver.reset();

        FixedVector<int, 16> pinList;
        for (auto it = mRectDrawBuffer.mDrawList.begin(); it != mRectDrawBuffer.mDrawList.end(); ++it) {
            if (validatePin(it->mPin)) {
                pinList.push_back(it->mPin);
            }
        }

        if (pinList.empty()) {
            FL_WARN("PARLIO Group: No valid pins");
            return;
        }

        uint32_t num_strips = 0, bytes_per_strip = 0, total_bytes = 0;
        mRectDrawBuffer.getBlockInfo(&num_strips, &bytes_per_strip, &total_bytes);

        bool is_rgbw = (bytes_per_strip % 4 == 0);
        int num_leds_per_strip = bytes_per_strip / (is_rgbw ? 4 : 3);

        uint8_t optimal_width = selectOptimalWidth(num_strips);
        mDriver = mDriverFactory(optimal_width, mTimingConfig);

        if (!mDriver.get()) {
            FL_WARN("PARLIO Group: Driver creation failed");
            return;
        }

        ParlioChannelConfig config = {};
        config.clk_gpio = -1;
        config.num_lanes = optimal_width;
        config.clock_freq_hz = 0;
        config.is_rgbw = is_rgbw;
        config.auto_clock_adjustment = false;

        for (int i = 0; i < num_strips; i++) {
            config.data_gpios[i] = pinList[i];
        }
        for (int i = num_strips; i < optimal_width; i++) {
            config.data_gpios[i] = -1;
        }

        bool ok = mDriver->begin(config, static_cast<uint16_t>(num_leds_per_strip));
        if (!ok) {
            FL_WARN("PARLIO Group: begin() failed");
            mDriver.reset();
            return;
        }

        for (int i = 0; i < num_strips; i++) {
            fl::span<uint8_t> pin_buffer = mRectDrawBuffer.getLedsBufferBytesForPin(pinList[i], false);
            if (!pin_buffer.empty()) {
                CRGB* strip = reinterpret_cast<CRGB*>(pin_buffer.data());
                mDriver->set_strip(i, strip);
            }
        }
    }

    if (!mDriver.get()) {
        FL_WARN("PARLIO Group: Driver not ready");
        return;
    }

    FL_DBG("PARLIO Group: Writing [LEGACY PATH]");
    mDriver->show();
    mDriver->wait();

    mQueueState = IDLE;
}

bool ParlioGroup::validatePin(uint8_t pin) {
    if (pin > 54) {
        FL_WARN("PARLIO: GPIO" << int(pin) << " invalid");
        return false;
    }
    if (pin >= 34 && pin <= 38) {
        FL_WARN("PARLIO: GPIO" << int(pin) << " is strapping pin");
        return false;
    }
    if (pin == 24 || pin == 25) {
        FL_WARN("PARLIO: GPIO" << int(pin) << " is USB-JTAG");
        return false;
    }
    return true;
}

uint8_t ParlioGroup::selectOptimalWidth(uint8_t num_strips) {
    if (num_strips <= 1) return 1;
    if (num_strips <= 2) return 2;
    if (num_strips <= 4) return 4;
    if (num_strips <= 8) return 8;
    return 16;
}

void ParlioGroup::generate_bitpatterns() {
    // Generate bitpatterns from runtime timing configuration
    static constexpr uint32_t CLOCKS_PER_BIT = 4;
    static constexpr uint32_t CLOCK_FREQ_HZ = 3200000;  // 3.2 MHz for WS2812
    static constexpr uint32_t CLOCK_PERIOD_NS = 1000000000UL / CLOCK_FREQ_HZ;

    // Convert timing to clock counts
    const uint32_t T0H_NS = mTimingConfig.t1_ns;
    const uint32_t T1H_NS = mTimingConfig.t1_ns + mTimingConfig.t2_ns;

    const uint32_t T0H_CLOCKS = (T0H_NS + CLOCK_PERIOD_NS / 2) / CLOCK_PERIOD_NS;
    const uint32_t T1H_CLOCKS = (T1H_NS + CLOCK_PERIOD_NS / 2) / CLOCK_PERIOD_NS;

    // Lambda: Generate 4-bit pattern for a single LED bit
    auto generate_bit_pattern = [T0H_CLOCKS, T1H_CLOCKS](uint8_t bit_value) -> uint16_t {
        uint16_t pattern = 0;
        uint32_t high_clocks = bit_value ? T1H_CLOCKS : T0H_CLOCKS;

        // Set bits to 1 for high duration
        for (uint32_t i = 0; i < high_clocks && i < CLOCKS_PER_BIT; ++i) {
            pattern |= (1 << i);
        }

        return pattern;
    };

    // Lambda: Generate 16-bit pattern for a 4-bit nibble
    auto generate_nibble_pattern = [&generate_bit_pattern](uint8_t nibble) -> uint16_t {
        uint16_t pattern = 0;

        // Process each of 4 LED bits in the nibble (MSB-first)
        for (int bit_pos = 3; bit_pos >= 0; --bit_pos) {
            uint8_t led_bit = (nibble >> bit_pos) & 1;
            uint16_t bit_pattern = generate_bit_pattern(led_bit);

            // Shift and combine (LSB of pattern is first in time)
            pattern = (pattern << CLOCKS_PER_BIT) | bit_pattern;
        }

        return pattern;
    };

    // Generate all 16 nibble patterns
    for (uint8_t i = 0; i < 16; ++i) {
        mBitpatterns[i] = generate_nibble_pattern(i);
    }
}

uint32_t ParlioGroup::calculate_optimal_clock_freq(uint16_t num_leds, uint32_t base_freq) {
    // Strategy from WLED Moon Modules
    if (num_leds <= 256) {
        // Small counts: 150% speed (e.g., 3.2 MHz → 4.8 MHz)
        return (base_freq * 3) / 2;
    } else if (num_leds <= 512) {
        // Medium counts: 137.5% speed (e.g., 3.2 MHz → 4.4 MHz)
        return (base_freq * 11) / 8;
    } else {
        // Large counts: 100% speed (e.g., 3.2 MHz → 3.2 MHz)
        return base_freq;
    }
}

void ParlioGroup::pack_data(uint8_t* output_buffer) {
    // Get strip information from rectangular draw buffer
    if (mRectDrawBuffer.mDrawList.empty()) {
        FL_WARN("PARLIO Group: pack_data called with no strips");
        return;
    }

    uint32_t num_strips = 0, bytes_per_strip = 0, total_bytes = 0;
    mRectDrawBuffer.getBlockInfo(&num_strips, &bytes_per_strip, &total_bytes);

    bool is_rgbw = (bytes_per_strip % 4 == 0);
    uint16_t num_leds = bytes_per_strip / (is_rgbw ? 4 : 3);
    const uint8_t num_components = is_rgbw ? 4 : 3;

    // Calculate bytes per component based on data width
    uint32_t bytes_per_component = 0;
    switch (mDataWidth) {
        case 1: bytes_per_component = 4; break;
        case 2: bytes_per_component = 8; break;
        case 4: bytes_per_component = 16; break;
        case 8: bytes_per_component = 32; break;
        case 16: bytes_per_component = 64; break;
        default:
            FL_WARN("PARLIO Group: Invalid data_width: " << int(mDataWidth));
            return;
    }

    FL_DBG("PARLIO Group pack_data: " << num_strips << " strips, " << num_leds << " LEDs, "
           << (is_rgbw ? "RGBW" : "RGB") << " mode, width=" << int(mDataWidth));

    uint8_t* out_ptr = output_buffer;

    // Process each LED
    for (uint16_t led = 0; led < num_leds; led++) {
        // Process each color component (G, R, B, W in order)
        // G=1, R=0, B=2, W=3 in CRGB
        const uint8_t component_order[4] = {1, 0, 2, 3};

        for (uint8_t comp_idx = 0; comp_idx < num_components; comp_idx++) {
            const uint8_t component = component_order[comp_idx];

            // Initialize 32 time-slices (each holds bits for all strips)
            uint32_t transposed_slices[32] = {0};

            // Generate and transpose waveforms for all active strips
            for (uint8_t channel = 0; channel < num_strips && channel < mDataWidth; channel++) {
                // Get pixel buffer for this strip
                int pin = mRectDrawBuffer.mDrawList[channel].mPin;
                fl::span<uint8_t> strip_bytes = mRectDrawBuffer.getLedsBufferBytesForPin(pin, false);

                if (strip_bytes.empty()) continue;

                // Calculate offset to this LED
                size_t led_offset = led * (is_rgbw ? 4 : 3);
                if (led_offset + 2 >= strip_bytes.size()) continue;

                const uint8_t* led_bytes = strip_bytes.data() + led_offset;

                // For RGBW, white component (index 3) needs special handling
                const uint8_t component_value = (component < 3) ? led_bytes[component] :
                                                (led_offset + 3 < strip_bytes.size() ? led_bytes[3] : 0);

                // Generate 32-bit waveform for this component value
                const uint32_t waveform = detail::generate_waveform(component_value, mBitpatterns);
                const uint32_t pin_bit = (1u << channel);

                // Transpose: Extract each of 32 bits from waveform
                // and distribute to corresponding time-slices
                for (int slice = 0; slice < 32; slice++) {
                    if ((waveform >> slice) & 1) {
                        transposed_slices[slice] |= pin_bit;
                    }
                }
            }

            // Pack transposed slices based on data_width (runtime dispatch)
            switch (mDataWidth) {
                case 1:
                    detail::process_1bit(out_ptr, transposed_slices);
                    break;
                case 2:
                    detail::process_2bit(out_ptr, transposed_slices);
                    break;
                case 4:
                    detail::process_4bit(out_ptr, transposed_slices);
                    break;
                case 8:
                    detail::process_8bit(out_ptr, transposed_slices);
                    break;
                case 16:
                    detail::process_16bit(out_ptr, transposed_slices);
                    break;
            }

            out_ptr += bytes_per_component;
        }
    }
    FL_DBG("PARLIO Group: pack_data completed");
}

// ===== Hub Integration Methods (Task 3.1 full implementation) =====

void ParlioGroup::prepare_write() {
    FL_DBG("PARLIO Group: prepare_write() - allocating and packing DMA buffer");

    // Get strip information
    if (mRectDrawBuffer.mDrawList.empty()) {
        FL_WARN("PARLIO Group: No strips to write");
        return;
    }

    uint32_t num_strips = 0, bytes_per_strip = 0, total_bytes = 0;
    mRectDrawBuffer.getBlockInfo(&num_strips, &bytes_per_strip, &total_bytes);

    bool is_rgbw = (bytes_per_strip % 4 == 0);
    uint16_t num_leds = bytes_per_strip / (is_rgbw ? 4 : 3);

    // Select optimal data width
    mDataWidth = selectOptimalWidth(num_strips);

    // Calculate buffer size
    const uint32_t COMPONENTS_PER_LED = is_rgbw ? 4 : 3;
    const uint32_t SYMBOLS_PER_LED = COMPONENTS_PER_LED * 32;  // Each component = 32-bit waveform
    const uint32_t BITS_PER_LED = SYMBOLS_PER_LED * mDataWidth;
    size_t required_buffer_size = (num_leds * BITS_PER_LED + 7) / 8;  // Convert to bytes

    // Allocate or reallocate DMA buffer if needed
    if (mDmaBufferSize != required_buffer_size || !mDmaBuffer) {
        if (mDmaBuffer) {
            heap_caps_free(mDmaBuffer);
            mDmaBuffer = nullptr;
        }

        mDmaBuffer = (uint8_t*)heap_caps_malloc(required_buffer_size, MALLOC_CAP_DMA);
        if (!mDmaBuffer) {
            FL_WARN("PARLIO Group: Failed to allocate DMA buffer (" << required_buffer_size << " bytes)");
            mDmaBufferSize = 0;
            return;
        }

        mDmaBufferSize = required_buffer_size;
        FL_DBG("PARLIO Group: Allocated DMA buffer: " << mDmaBufferSize << " bytes at " << (void*)mDmaBuffer);
    }

    // Clear buffer
    fl::memset(mDmaBuffer, 0, mDmaBufferSize);

    // Pack pixel data into DMA buffer
    pack_data(mDmaBuffer);
}

ParlioHardwareConfig ParlioGroup::get_hardware_config() {
    FL_DBG("PARLIO Group: get_hardware_config()");

    ParlioHardwareConfig config = {};

    // Get strip information
    if (mRectDrawBuffer.mDrawList.empty()) {
        FL_WARN("PARLIO Group: No strips configured");
        return config;
    }

    uint32_t num_strips = 0, bytes_per_strip = 0, total_bytes = 0;
    mRectDrawBuffer.getBlockInfo(&num_strips, &bytes_per_strip, &total_bytes);

    bool is_rgbw = (bytes_per_strip % 4 == 0);
    uint16_t num_leds = bytes_per_strip / (is_rgbw ? 4 : 3);

    // Populate GPIO pins
    for (uint32_t i = 0; i < num_strips && i < 16; i++) {
        config.data_gpios[i] = mRectDrawBuffer.mDrawList[i].mPin;
    }
    for (uint32_t i = num_strips; i < 16; i++) {
        config.data_gpios[i] = -1;
    }

    config.num_lanes = mDataWidth;
    config.max_transfer_size = mDmaBufferSize;

    // Calculate clock frequency from chipset timing
    uint32_t total_period_ns = mTimingConfig.t1_ns + mTimingConfig.t2_ns + mTimingConfig.t3_ns;
    uint32_t base_freq = (1000000000UL * 4) / total_period_ns;

    // Apply optimization based on LED count
    config.clock_freq_hz = calculate_optimal_clock_freq(num_leds, base_freq);

    // Note: Callback will be set by hub
    config.on_trans_done = nullptr;
    config.callback_arg = nullptr;

    FL_DBG("PARLIO Group: Hardware config - lanes=" << config.num_lanes << ", freq=" << config.clock_freq_hz << " Hz");

    return config;
}

const uint8_t* ParlioGroup::get_dma_buffer() {
    return mDmaBuffer;
}

uint32_t ParlioGroup::get_total_bits() {
    if (mRectDrawBuffer.mDrawList.empty()) {
        return 0;
    }

    uint32_t num_strips = 0, bytes_per_strip = 0, total_bytes = 0;
    mRectDrawBuffer.getBlockInfo(&num_strips, &bytes_per_strip, &total_bytes);

    bool is_rgbw = (bytes_per_strip % 4 == 0);
    uint16_t num_leds = bytes_per_strip / (is_rgbw ? 4 : 3);

    const uint32_t COMPONENTS_PER_LED = is_rgbw ? 4 : 3;
    const uint32_t SYMBOLS_PER_LED = COMPONENTS_PER_LED * 32;  // Each component = 32-bit waveform
    const uint32_t BITS_PER_LED = SYMBOLS_PER_LED * mDataWidth;

    return num_leds * BITS_PER_LED;
}

uint32_t ParlioGroup::get_bits_per_component() {
    // Each component is 32 bits × data_width
    return 32 * mDataWidth;
}

// Strip management methods removed - now handled in BulkClockless<PARLIO> specialization

} // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32P4
