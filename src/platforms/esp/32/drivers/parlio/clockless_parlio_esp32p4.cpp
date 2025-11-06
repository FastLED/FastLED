#if defined(ESP32)
#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32P4)

#define FASTLED_INTERNAL
#include "fl/fastled.h"

#include "crgb.h"
#include "eorder.h"
#include "fl/map.h"
#include "fl/singleton.h"
#include "fl/vector.h"
#include "fl/warn.h"
#include "fl/math_macros.h"
#include "pixel_iterator.h"
#include "fl/allocator.h"
#include "fl/unique_ptr.h"
#include "fl/assert.h"
#include "fl/rectangular_draw_buffer.h"
#include "cpixel_ledcontroller.h"

#include "clockless_parlio_esp32p4.h"
#include "parlio_driver.h"

namespace { // anonymous namespace

typedef uint8_t ParlioPin;

/// @brief Manages all PARLIO strips and bulk driver
class ParlioEsp32P4_Group {
public:
    fl::unique_ptr<fl::ParlioLedDriverBase> mDriver;
    fl::RectangularDrawBuffer mRectDrawBuffer;
    bool mDrawn = false;
    uint8_t mClkPin = 9;  // Default clock pin (GPIO 9)

    static ParlioEsp32P4_Group& getInstance() {
        return fl::Singleton<ParlioEsp32P4_Group>::instance();
    }

    ParlioEsp32P4_Group() = default;
    ~ParlioEsp32P4_Group() { mDriver.reset(); }

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
        mRectDrawBuffer.onQueuingStart();
        mDrawn = false;
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

    void showPixelsOnceThisFrame() {
        if (mDrawn) {
            FL_DBG("PARLIO: Already drawn this frame, skipping");
            return;
        }
        mDrawn = true;

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

            // Instantiate driver based on width (optimal memory usage)
            switch (optimal_width) {
                case 1:
                    mDriver.reset(new fl::ParlioLedDriver<1, fl::WS2812ChipsetTiming>());
                    FL_DBG("  Created 1-lane driver");
                    break;
                case 2:
                    mDriver.reset(new fl::ParlioLedDriver<2, fl::WS2812ChipsetTiming>());
                    FL_DBG("  Created 2-lane driver");
                    break;
                case 4:
                    mDriver.reset(new fl::ParlioLedDriver<4, fl::WS2812ChipsetTiming>());
                    FL_DBG("  Created 4-lane driver");
                    break;
                case 8:
                    mDriver.reset(new fl::ParlioLedDriver<8, fl::WS2812ChipsetTiming>());
                    FL_DBG("  Created 8-lane driver");
                    break;
                case 16:
                    mDriver.reset(new fl::ParlioLedDriver<16, fl::WS2812ChipsetTiming>());
                    FL_DBG("  Created 16-lane driver");
                    break;
                default:
                    FL_WARN("PARLIO: Invalid optimal width " << int(optimal_width)
                            << ". This is a bug in selectOptimalWidth().");
                    FL_ASSERT(false, "Invalid PARLIO width: " << int(optimal_width));
                    return;
            }

            if (!mDriver.get()) {
                FL_WARN("PARLIO: Failed to allocate driver (out of memory?)");
                return;
            }

            // Configure driver
            fl::ParlioDriverConfig config = {};
            config.clk_gpio = -1;  // Internal clock used (field unused)
            config.num_lanes = optimal_width;  // Must match DATA_WIDTH template parameter
            config.clock_freq_hz = 0;  // Use default 3.2 MHz for WS2812
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

void Parlio_Esp32P4::beginShowLeds(int data_pin, int nleds) {
    FASTLED_DBG("PARLIO Parlio_Esp32P4::beginShowLeds called with data_pin=" << data_pin << ", nleds=" << nleds);
    ParlioEsp32P4_Group& group = ParlioEsp32P4_Group::getInstance();
    group.onQueuingStart();
    group.addObject(data_pin, nleds, false);
    FASTLED_DBG("  After addObject, drawList.size()=" << (int)group.mRectDrawBuffer.mDrawList.size());
}

void Parlio_Esp32P4::showPixels(uint8_t data_pin, PixelIterator& pixel_iterator) {
    ParlioEsp32P4_Group& group = ParlioEsp32P4_Group::getInstance();
    group.onQueuingDone();

    const Rgbw rgbw = pixel_iterator.get_rgbw();
    int numLeds = pixel_iterator.size();
    span<uint8_t> strip_bytes = group.mRectDrawBuffer.getLedsBufferBytesForPin(data_pin, true);

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

void Parlio_Esp32P4::endShowLeds() {
    ParlioEsp32P4_Group::getInstance().showPixelsOnceThisFrame();
}

} // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32P4
#endif // ESP32
