#if defined(ESP32)
#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32P4)

#define FASTLED_INTERNAL
#include "fl/fastled.h"

#include "crgb.h"
#include "eorder.h"
#include "fl/singleton.h"
#include "fl/vector.h"
#include "fl/warn.h"
#include "pixel_iterator.h"
#include "fl/unique_ptr.h"
#include "fl/rectangular_draw_buffer.h"
#include "cpixel_ledcontroller.h"

#include "clockless_parlio_esp32p4.h"
#include "parlio_channel.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "parlio_transmitter.h"
#include "parlio_hub.h"

// Runtime driver creation with chipset timing config
namespace fl {
    fl::unique_ptr<fl::IParlioChannel> createParlioChannelRuntime(uint8_t width, const ChipsetTimingConfig& timing) {
        return fl::IParlioChannel::create(timing, width);
    }
}

namespace { // anonymous namespace

typedef uint8_t ParlioPin;

/// @brief Base transmitter that manages parallel output to multiple LED channels
///
/// This class broadcasts to LED strips sharing identical chipset timing.
/// One transmitter instance per chipset type (WS2812, WS2816, etc.)
class ParlioTransmitterBase {
public:
    fl::unique_ptr<fl::IParlioChannel> mDriver;
    fl::RectangularDrawBuffer mRectDrawBuffer;
    fl::ChipsetTimingConfig mTimingConfig;  // Chipset timing configuration

    // Frame lifecycle state tracking
    enum QueueState { IDLE, QUEUING, FLUSHED };
    QueueState mQueueState = IDLE;

    /// @brief Driver factory function type
    /// Creates a driver of the correct width and chipset type
    using DriverFactory = fl::unique_ptr<fl::IParlioChannel> (*)(uint8_t width, const fl::ChipsetTimingConfig& timing);

    DriverFactory mDriverFactory;  // Factory function for creating drivers

    /// @brief Constructor with chipset timing configuration
    /// @param timing Chipset timing configuration for this transmitter
    /// @param factory Driver factory function for the chipset
    explicit ParlioTransmitterBase(const fl::ChipsetTimingConfig& timing, DriverFactory factory)
        : mTimingConfig(timing)
        , mDriverFactory(factory) {}

    ~ParlioTransmitterBase() { mDriver.reset(); }

    /// @brief Validate GPIO pin for ESP32-P4 PARLIO peripheral
    ///
    /// Checks hardware constraints:
    /// - Valid GPIO range (0-54)
    /// - Not strapping pins (GPIO34-38) - WILL PREVENT BOOT
    /// - Not USB-JTAG pins (GPIO24-25) - WILL DISABLE DEBUG
    ///
    /// @param pin GPIO pin number to validate
    /// @returns true if pin is safe to use, false otherwise
    static bool validatePin(uint8_t pin) {
        // ESP32-P4 has GPIO 0-54
        if (pin > 54) {
            FL_WARN("PARLIO: GPIO pin " << int(pin) << " is invalid. ESP32-P4 has GPIO 0-54.");
            return false;
        }

        // Reject strapping pins (GPIO34-38) - these control boot behavior
        // Using these pins WILL PREVENT BOOT or cause unpredictable behavior
        if (pin >= 34 && pin <= 38) {
            FL_WARN("PARLIO: GPIO" << int(pin) << " is a strapping pin and CANNOT be used for LED output. "
                    "Using this pin WILL PREVENT BOOT. Please choose a different pin.");
            return false;
        }

        // Reject USB-JTAG pins (GPIO24-25) - these are needed for debugging
        if (pin == 24 || pin == 25) {
            FL_WARN("PARLIO: GPIO" << int(pin) << " is reserved for USB-JTAG on ESP32-P4. "
                    "Using this pin WILL DISABLE USB-JTAG debugging. Please choose a different pin.");
            return false;
        }

        // Note: Flash/PSRAM pins vary by board configuration and cannot be checked at compile time
        // Users should consult their board documentation for Flash/PSRAM pin assignments

        return true;
    }

    void onQueuingStart() {
        // Only initialize queuing state once per frame
        if (mQueueState == QUEUING) {
            FL_DBG("PARLIO: Already queuing, skipping onQueuingStart()");
            return;
        }

        FL_DBG("PARLIO: Starting new frame queuing");
        mQueueState = QUEUING;
        mRectDrawBuffer.onQueuingStart();
    }

    bool isQueuing() const {
        return mQueueState == QUEUING;
    }

    void onQueuingDone() {
        mRectDrawBuffer.onQueuingDone();
    }

    void addObject(ParlioPin pin, uint16_t numLeds, bool is_rgbw) {
        // Validate pin before adding
        if (!validatePin(pin)) {
            FL_WARN("PARLIO: Rejecting strip on invalid pin " << int(pin));
            return;
        }

        // Check LED count uniformity (PARLIO limitation)
        // All strips MUST have the same LED count because PARLIO transmits fixed-length frames
        if (!mRectDrawBuffer.mDrawList.empty()) {
            uint16_t existing_led_count = mRectDrawBuffer.mDrawList[0].mNumBytes / (is_rgbw ? 4 : 3);
            if (numLeds != existing_led_count) {
                FL_WARN("PARLIO: LED count mismatch! All strips must have the same LED count. "
                        "Existing strips have " << existing_led_count << " LEDs, "
                        "but strip on pin " << int(pin) << " has " << numLeds << " LEDs. "
                        "Rejecting this strip.");
                return;
            }
        }

        // Check RGBW consistency
        if (!mRectDrawBuffer.mDrawList.empty()) {
            bool existing_is_rgbw = (mRectDrawBuffer.mDrawList[0].mNumBytes % 4 == 0);
            if (is_rgbw != existing_is_rgbw) {
                FL_WARN("PARLIO: RGBW mode mismatch! All strips must use the same mode. "
                        "Existing strips are " << (existing_is_rgbw ? "RGBW" : "RGB") << ", "
                        "but strip on pin " << int(pin) << " is " << (is_rgbw ? "RGBW" : "RGB") << ". "
                        "Rejecting this strip.");
                return;
            }
        }

        // Enforce maximum strip count (PARLIO hardware limit)
        if (mRectDrawBuffer.mDrawList.size() >= 16) {
            FL_WARN("PARLIO: Cannot add strip on pin " << int(pin) << ". "
                    "Maximum 16 strips supported by PARLIO peripheral.");
            return;
        }

        FL_DBG("PARLIO: Adding strip on pin " << int(pin) << " with " << numLeds << " LEDs "
               << (is_rgbw ? "(RGBW)" : "(RGB)"));
        mRectDrawBuffer.queue(fl::DrawItem(pin, numLeds, is_rgbw));
    }

    /// @brief Write pixel data for a specific strip
    ///
    /// Writes RGB or RGBW pixel data from a PixelIterator into the internal
    /// DMA buffer for the specified pin.
    ///
    /// @param data_pin GPIO pin number of the strip
    /// @param pixel_iterator Iterator providing pixel data with scaling/dithering
    void writePixels(uint8_t data_pin, fl::PixelIterator& pixel_iterator) {
        // Get the buffer span for this pin's strip
        fl::span<uint8_t> strip_bytes = mRectDrawBuffer.getLedsBufferBytesForPin(data_pin, true);

        // Determine if RGBW based on pixel iterator
        const fl::Rgbw rgbw = pixel_iterator.get_rgbw();

        if (rgbw.active()) {
            // RGBW mode: 4 bytes per pixel
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
            // RGB mode: 3 bytes per pixel
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

    void showPixelsOnceThisFrame() {
        if (mQueueState == FLUSHED) {
            FL_DBG("PARLIO: Already flushed this frame, skipping");
            return;
        }
        mQueueState = FLUSHED;  // Mark frame as flushed

        if (!mRectDrawBuffer.mAllLedsBufferUint8Size) {
            FL_DBG("PARLIO: No LED data to transmit (buffer size is 0)");
            return;
        }

        if (mRectDrawBuffer.mDrawList.empty()) {
            FL_DBG("PARLIO: No strips registered, skipping transmission");
            return;
        }

        bool drawlist_changed = mRectDrawBuffer.mDrawListChangedThisFrame;
        bool needs_validation = !mDriver.get() || drawlist_changed;

        FL_DBG("PARLIO Group showPixelsOnceThisFrame:");
        FL_DBG("  drawlist_changed: " << (drawlist_changed ? "true" : "false"));
        FL_DBG("  needs_validation: " << (needs_validation ? "true" : "false"));
        FL_DBG("  mDrawList.size(): " << (int)mRectDrawBuffer.mDrawList.size());

        if (needs_validation) {
            FL_DBG("PARLIO: Reconfiguring driver...");
            mDriver.reset();

            // Build pin list and validate all pins again (safety check)
            fl::FixedVector<int, 16> pinList;
            for (auto it = mRectDrawBuffer.mDrawList.begin();
                 it != mRectDrawBuffer.mDrawList.end(); ++it) {
                // Re-validate pin (should never fail here, but defensive programming)
                if (!validatePin(it->mPin)) {
                    FL_WARN("PARLIO: Strip on pin " << int(it->mPin) << " failed validation during driver init. Skipping.");
                    continue;
                }
                pinList.push_back(it->mPin);
                FL_DBG("  Registered pin " << int(it->mPin) << " with " << int(it->mNumBytes) << " bytes");
            }

            if (pinList.empty()) {
                FL_WARN("PARLIO: No valid pins after validation. Cannot initialize driver.");
                return;
            }

            // Get buffer info
            uint32_t num_strips = 0;
            uint32_t bytes_per_strip = 0;
            uint32_t total_bytes = 0;
            mRectDrawBuffer.getBlockInfo(&num_strips, &bytes_per_strip, &total_bytes);

            // Determine if RGBW based on first strip's byte count
            bool is_rgbw = (bytes_per_strip % 4 == 0);
            int bytes_per_led = is_rgbw ? 4 : 3;
            int num_leds_per_strip = bytes_per_strip / bytes_per_led;

            FL_DBG("  num_strips: " << int(num_strips));
            FL_DBG("  bytes_per_strip: " << int(bytes_per_strip));
            FL_DBG("  num_leds_per_strip: " << num_leds_per_strip);
            FL_DBG("  is_rgbw: " << (is_rgbw ? "true" : "false"));

            // Validate LED count
            if (num_leds_per_strip == 0) {
                FL_WARN("PARLIO: Invalid LED count (0). Cannot initialize driver.");
                return;
            }

            // Auto-select optimal width
            uint8_t optimal_width = selectOptimalWidth(num_strips);
            FL_DBG("  optimal_width selected: " << int(optimal_width));

            // Instantiate driver using factory function (chipset-agnostic)
            FL_DBG("  Creating " << int(optimal_width) << "-lane driver for chipset: " << mTimingConfig.name);
            mDriver = mDriverFactory(optimal_width, mTimingConfig);

            if (!mDriver.get()) {
                FL_WARN("PARLIO: Failed to allocate driver (out of memory or invalid width?)");
                return;
            }
            FL_DBG("  Successfully created " << int(optimal_width) << "-lane driver");

            // Configure driver
            fl::ParlioChannelConfig config = {};
            config.clk_gpio = -1;  // Internal clock used (field unused)
            config.num_lanes = optimal_width;  // Must match DATA_WIDTH template parameter
            config.clock_freq_hz = 0;  // Use driver's default clock frequency (chipset-specific)
            config.is_rgbw = is_rgbw;  // Set RGBW mode
            config.auto_clock_adjustment = false;  // Manual clock control

            // Assign active GPIOs
            for (int i = 0; i < num_strips; i++) {
                config.data_gpios[i] = pinList[i];
                FL_DBG("  config.data_gpios[" << i << "] = " << pinList[i]);
            }
            // Pad unused lanes with -1 (GPIO_NUM_NC) to indicate unused
            for (int i = num_strips; i < optimal_width; i++) {
                config.data_gpios[i] = -1;
            }

            // Initialize driver with configuration
            FL_DBG("  Calling mDriver->begin() with " << num_leds_per_strip << " LEDs per strip...");
            bool ok = mDriver->begin(config, static_cast<uint16_t>(num_leds_per_strip));
            if (!ok) {
                FL_WARN("PARLIO: Failed to initialize driver. Possible causes:");
                FL_WARN("  - Invalid GPIO pins for PARLIO peripheral");
                FL_WARN("  - Insufficient DMA memory");
                FL_WARN("  - PARLIO peripheral already in use");
                FL_WARN("  - Exceeded hardware limitations");
                mDriver.reset();  // Clean up failed driver
                return;
            }
            FL_DBG("  Driver initialized successfully");

            // Attach strips to driver channels
            FL_DBG("  Attaching strip buffers to driver channels...");
            for (int i = 0; i < num_strips; i++) {
                fl::span<uint8_t> pin_buffer =
                    mRectDrawBuffer.getLedsBufferBytesForPin(pinList[i], false);

                if (pin_buffer.empty()) {
                    FL_WARN("PARLIO: Strip " << i << " on pin " << pinList[i] << " has empty buffer, skipping");
                    continue;
                }

                CRGB* strip = reinterpret_cast<CRGB*>(pin_buffer.data());
                if (!strip) {
                    FL_WARN("PARLIO: Strip " << i << " on pin " << pinList[i] << " has null pointer, skipping");
                    continue;
                }

                mDriver->set_strip(i, strip);
                FL_DBG("  Attached strip " << i << " (pin " << pinList[i] << ") to driver");
            }
            FL_DBG("  All strips attached successfully");
        }

        // Verify driver is ready before transmission
        if (!mDriver.get()) {
            FL_WARN("PARLIO: Driver not initialized, cannot transmit");
            return;
        }

        // Show and wait (data is already in correct RGB order from PixelController)
        FL_DBG("PARLIO: Starting transmission...");
        mDriver->show();
        mDriver->wait();
        FL_DBG("PARLIO: Transmission complete");

        // Reset state for next frame
        mQueueState = IDLE;
    }

private:
    uint8_t selectOptimalWidth(uint8_t num_strips) {
        if (num_strips <= 1) return 1;
        if (num_strips <= 2) return 2;
        if (num_strips <= 4) return 4;
        if (num_strips <= 8) return 8;
        return 16;
    }
};

} // anonymous namespace

namespace fl {

// ===== ParlioChannelDriver method implementations =====
// Concrete driver helper - uses ParlioGroup for per-chipset management

ParlioChannelDriver::ParlioChannelDriver(int pin, const ChipsetTimingConfig& timing)
    : mPin(pin)
    , mTiming(timing)
{
}

void ParlioChannelDriver::init() {
    // Initialization happens in beginShowLeds()
}

void ParlioChannelDriver::beginShowLeds(int nleds) {
    FASTLED_DBG("PARLIO ParlioChannelDriver::beginShowLeds called with data_pin=" << mPin << ", nleds=" << nleds);

    // Get the chipset-specific group singleton using runtime timing config
    // Each unique ChipsetTimingConfig hash has its own group
    auto& group = IParlioTransmitter::getOrCreate(mTiming);

    // Cross-chipset coordination: Flush any pending groups of DIFFERENT chipset types
    // This ensures that if the user mixes chipsets (e.g., WS2812 + SK6812), the previous
    // chipset's data transmits before we start queuing data for this chipset.
    // Same-chipset strips can batch together for efficiency.
    ParlioHub::getInstance().flushAllExcept(&group);

    // Start queuing mode for this frame (if not already active)
    // Multiple strips of the same chipset will all queue into this group
    if (!group.isQueuing()) {
        group.onQueuingStart();
    }

    // Register this strip with the chipset-specific group
    // All strips in a group must have same LED count (PARLIO hardware limitation)
    group.addStrip(mPin, nleds, false);
    FASTLED_DBG("  Strip registered with " << mTiming.name << " group");
}

void ParlioChannelDriver::showPixels(PixelIterator& pixel_iterator) {
    auto& group = IParlioTransmitter::getOrCreate(mTiming);

    // Mark queuing phase as complete for this group
    group.onQueuingDone();

    // Write pixel data to the DMA buffer for this strip
    // Data is queued but NOT transmitted yet - transmission happens later during flush
    group.writePixels(mPin, pixel_iterator);
}

void ParlioChannelDriver::endShowLeds() {
    // FastLED calls this on EACH controller after pixel data is written
    //
    // For PARLIO, we call showPixelsOnceThisFrame() which has internal logic
    // to only flush once per frame (via mQueueState == FLUSHED check).
    //
    // This means:
    // - First PARLIO strip's endShowLeds(): Flushes and transmits
    // - Subsequent PARLIO strips' endShowLeds(): Skipped (already flushed)
    // - Same pattern works for all chipset types (WS2812, SK6812, etc.)
    //
    // Each chipset has its own group, so WS2812 and SK6812 each flush once.
    auto& group = IParlioTransmitter::getOrCreate(mTiming);
    group.showPixelsOnceThisFrame();
}

// ===== ParlioTransmitterBase factory function =====
// Creates ParlioTransmitterBase instances for IParlioTransmitter

ParlioTransmitterBase* createParlioTransmitterBase(const ChipsetTimingConfig& timing) {
    // Create a stateless lambda (no capture) that can be converted to function pointer
    auto factory = [](uint8_t width, const ChipsetTimingConfig& timing) -> unique_ptr<IParlioChannel> {
        return createParlioChannelRuntime(width, timing);
    };
    return new ParlioTransmitterBase(timing, factory);
}

// ===== ParlioChannel (channel adapter) method implementations =====

template <int DATA_PIN, typename CHIPSET, EOrder RGB_ORDER>
ParlioChannel<DATA_PIN, CHIPSET, RGB_ORDER>::ParlioChannel()
    : mDriver(DATA_PIN, makeTimingConfig<CHIPSET>())
{
    // Template parameters converted to runtime values:
    // - DATA_PIN (compile-time int) → constructor argument (runtime int)
    // - CHIPSET (compile-time type) → makeTimingConfig<CHIPSET>() (runtime ChipsetTimingConfig)
    // - RGB_ORDER handled by CPixelLEDController<RGB_ORDER> base class
}

template <int DATA_PIN, typename CHIPSET, EOrder RGB_ORDER>
void ParlioChannel<DATA_PIN, CHIPSET, RGB_ORDER>::init() {
    mDriver.init();
}

template <int DATA_PIN, typename CHIPSET, EOrder RGB_ORDER>
uint16_t ParlioChannel<DATA_PIN, CHIPSET, RGB_ORDER>::getMaxRefreshRate() const {
    return 800;
}

template <int DATA_PIN, typename CHIPSET, EOrder RGB_ORDER>
void* ParlioChannel<DATA_PIN, CHIPSET, RGB_ORDER>::beginShowLeds(int nleds) {
    void *data = Base::beginShowLeds(nleds);
    mDriver.beginShowLeds(nleds);
    return data;
}

template <int DATA_PIN, typename CHIPSET, EOrder RGB_ORDER>
void ParlioChannel<DATA_PIN, CHIPSET, RGB_ORDER>::showPixels(PixelController<RGB_ORDER> &pixels) {
    auto pixel_iterator = pixels.as_iterator(this->getRgbw());
    mDriver.showPixels(pixel_iterator);
}

template <int DATA_PIN, typename CHIPSET, EOrder RGB_ORDER>
void ParlioChannel<DATA_PIN, CHIPSET, RGB_ORDER>::endShowLeds(void *data) {
    Base::endShowLeds(data);
    mDriver.endShowLeds();
}

// Note: No explicit template instantiations needed for ParlioChannel
// It's a shallow wrapper that converts template parameters to runtime values in the constructor.
// The compiler will instantiate on-demand when used.

} // namespace fl

// ===== C-linkage function for FastLED.cpp integration =====
// See BULK_CLOCKLESS_DESIGN.md §8 - FastLED.cpp must call flushAll() at frame end

namespace fl {
void parlio_flush_all_groups() {
    ParlioHub::getInstance().flushAll();
}
}  // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32P4
#endif // ESP32
