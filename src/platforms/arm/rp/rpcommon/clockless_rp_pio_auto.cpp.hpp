// IWYU pragma: private


#include "platforms/arm/rp/is_rp.h"

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

#define FASTLED_INTERNAL
#include "fl/system/fastled.h"

#include "platforms/arm/rp/rpcommon/clockless_rp_pio_auto.h"
#include "platforms/arm/rp/rpcommon/rp_pio_tx_peripheral.h"
#include "platforms/arm/rp/rpcommon/parallel_transpose.h"
#include "fl/gfx/rectangular_draw_buffer.h"
#include "fl/stl/singleton.h"
#include "fl/stl/vector.h"
#include "fl/log/log.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/flat_map.h"
namespace fl {

/// @brief Represents a group of consecutive GPIO pins for parallel output
struct Rp2040PinGroup {
    fl::u8 base_pin;      ///< Starting GPIO pin
    fl::u8 num_pins;      ///< Number of consecutive pins (2, 4, or 8)
    fl::vector<fl::u8> pins;  ///< List of all pins in this group (sorted)

    fl::unique_ptr<fl::u8[]> transpose_buffer;  ///< Bit-transposed output buffer
    fl::u32 buffer_size;  ///< Size of transpose buffer

    Rp2040PinGroup() : base_pin(0), num_pins(0), buffer_size(0) {}
};

/// @brief Singleton group manager for RP2040 parallel output
///
/// This class manages automatic grouping of consecutive pins for parallel output.
/// It uses RectangularDrawBuffer to collect LED data from multiple strips and
/// detects consecutive pin groups for efficient parallel PIO output.
class RP2040ParallelGroup {
  public:
    fl::RectangularDrawBuffer mRectDrawBuffer;
    bool mDrawn = false;

    // Pin groups detected from consecutive pins
    fl::vector<fl::unique_ptr<Rp2040PinGroup>> mPinGroups;

    // Map from pin number to group index
    fl::flat_map<fl::u8, fl::u32> mPinToGroupIndex;

    static RP2040ParallelGroup& getInstance() {
        return fl::Singleton<RP2040ParallelGroup>::instance();
    }

    RP2040ParallelGroup() = default;
    ~RP2040ParallelGroup() {
        mPinGroups.clear();
    }

    void onQueuingStart() {
        mRectDrawBuffer.onQueuingStart();
        mDrawn = false;
    }

    void onQueuingDone() {
        mRectDrawBuffer.onQueuingDone();
    }

    void addObject(fl::u8 pin, fl::u16 numLeds, bool is_rgbw) {
        mRectDrawBuffer.queue(fl::DrawItem(pin, numLeds, is_rgbw));
    }

    /// @brief Detect consecutive pin groups and create parallel output groups
    ///
    /// This function analyzes the pin list and creates groups of 2, 4, or 8
    /// consecutive pins for parallel output. Non-consecutive pins are handled
    /// as single-pin groups (sequential output).
    void detectPinGroups() {
        // Clear existing groups
        mPinGroups.clear();
        mPinToGroupIndex.clear();

        if (mRectDrawBuffer.mDrawList.empty()) {
            return;
        }

        // Extract pins and sort them (simple insertion sort for small N)
        fl::vector<fl::u8> sorted_pins;
        for (auto it = mRectDrawBuffer.mDrawList.begin();
             it != mRectDrawBuffer.mDrawList.end(); ++it) {
            sorted_pins.push_back(it->mPin);
        }

        // Sort pins numerically using insertion sort (efficient for small N)
        for (fl::u32 i = 1; i < sorted_pins.size(); i++) {
            fl::u8 key = sorted_pins[i];
            fl::i32 j = i - 1;
            while (j >= 0 && sorted_pins[j] > key) {
                sorted_pins[j + 1] = sorted_pins[j];
                j--;
            }
            sorted_pins[j + 1] = key;
        }

        FL_DBG_F("Detecting pin groups from %s pins", sorted_pins.size());

        // Detect consecutive runs
        fl::u32 i = 0;
        while (i < sorted_pins.size()) {
            fl::u8 start_pin = sorted_pins[i];
            fl::u32 run_length = 1;

            // Find consecutive run
            while (i + run_length < sorted_pins.size() &&
                   sorted_pins[i + run_length] == start_pin + run_length) {
                run_length++;
            }

            // Determine group size (2, 4, 8, or 1 for fallback)
            fl::u8 group_size = 1;
            if (run_length >= 8) {
                group_size = 8;
            } else if (run_length >= 4) {
                group_size = 4;
            } else if (run_length >= 2) {
                group_size = 2;
            }

            // Create group
            fl::unique_ptr<Rp2040PinGroup> group(new Rp2040PinGroup());  // ok bare allocation
            group->base_pin = start_pin;
            group->num_pins = group_size;

            // Add pins to group
            for (fl::u8 j = 0; j < group_size; j++) {
                group->pins.push_back(start_pin + j);
                mPinToGroupIndex[start_pin + j] = mPinGroups.size();
            }

            if (group_size > 1) {
                FL_DBG_F("Created %s-pin parallel group at GPIO %s", (int)group_size, (int)start_pin);
            } else {
                FL_DBG_F("Created single-pin (sequential) group at GPIO %s", (int)start_pin);
            }

            mPinGroups.push_back(fl::move(group));

            // Move to next group
            i += group_size;
        }

        FL_DBG_F("Total pin groups: %s", mPinGroups.size());
    }

    /// @brief Show all pixels - called once per frame
    ///
    /// This function performs the actual output for all pin groups.
    /// It allocates resources if needed, transposes data for parallel groups,
    /// and triggers DMA transfers to PIO state machines.
    void showPixelsOnceThisFrame() {
        if (mDrawn) {
            return;  // Already drawn this frame
        }
        mDrawn = true;

        if (!mRectDrawBuffer.mAllLedsBufferUint8Size) {
            return;  // No data to show
        }

        if (mRectDrawBuffer.mDrawListChangedThisFrame) {
            // Pin configuration changed - rebuild the software grouping. PIO,
            // DMA, and GPIO claims are made by RpPioTxPeripheral for the
            // duration of each transfer, so this path cannot retain stale
            // claims when a sketch changes its LED topology.
            FL_DBG_F("Pin configuration changed, rebuilding groups");
            detectPinGroups();
        }

        // Output each group
        for (auto it = mPinGroups.begin(); it != mPinGroups.end(); ++it) {
            Rp2040PinGroup* group = it->get();

            if (group->num_pins == 1) {
                // Single pin - sequential output (fallback)
                outputSinglePin(group);
            } else {
                // Parallel group - transpose and output
                outputParallelGroup(group);
            }
        }
    }

  private:
    /// Transfer one already-transposed frame through the shared RP PIO/DMA
    /// implementation. The words are kept alive until the PIO reaches its
    /// blocking OUT instruction, not merely until DMA reports idle.
    bool transmitWords(fl::u8 pin, fl::u8 lanes,
                       const fl::vector<fl::u32>& words) {
        RpPioTxPeripheral peripheral(0);
        RpPioTxConfig config;
        config.tx_pin = pin;
        config.lane_count = lanes;
        config.timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
        if (!peripheral.configure(config) ||
            !peripheral.startTxDma(words.data(), words.size())) {
            FL_WARN_F("Unable to start %s-lane PIO transfer at GPIO %s",
                      (int)lanes, (int)pin);
            return false;
        }

        const u32 start_us = fl::micros();
        while (peripheral.isDmaBusy()) {
            if (static_cast<u32>(fl::micros() - start_us) > 1000000u) {
                peripheral.abort();
                FL_WARN_F("Timed out waiting for DMA at GPIO %s", (int)pin);
                return false;
            }
        }
        while (!peripheral.isTerminalComplete()) {
            if (static_cast<u32>(fl::micros() - start_us) > 1000000u) {
                peripheral.abort();
                FL_WARN_F("Timed out waiting for PIO completion at GPIO %s", (int)pin);
                return false;
            }
        }
        return true;
    }

    /// @brief Output a single pin (non-parallel fallback)
    void outputSinglePin(Rp2040PinGroup* group) {
        fl::u8 pin = group->base_pin;

        // Get LED data for this pin from RectangularDrawBuffer
        fl::span<fl::u8> led_data = mRectDrawBuffer.getLedsBufferBytesForPin(pin, false);

        fl::vector<fl::u32> words;
        words.reserve(led_data.size() * 8);
        for (u8 byte : led_data) {
            for (int bit = 7; bit >= 0; --bit) {
                words.push_back(((static_cast<u32>(byte >> bit) & 1u) << 31));
            }
        }
        if (transmitWords(pin, 1, words)) {
            FL_DBG_F("Sequential output for GPIO %s (%s bytes)",
                     (int)pin, led_data.size());
        }
    }

    /// @brief Output a parallel group with bit transposition
    void outputParallelGroup(Rp2040PinGroup* group) {
        // Check if any strip in this parallel group is RGBW
        // If any strip is RGBW, we treat the entire group as RGBW (pad RGB strips with W=0)
        bool any_rgbw = false;
        for (fl::u8 i = 0; i < group->num_pins; i++) {
            fl::u8 pin = group->base_pin + i;
            // Search for this pin in the draw list
            for (auto it = mRectDrawBuffer.mDrawList.begin();
                 it != mRectDrawBuffer.mDrawList.end(); ++it) {
                if (it->mPin == pin && it->mIsRgbw) {
                    any_rgbw = true;
                    break;
                }
            }
            if (any_rgbw) break;
        }

        // Calculate bytes per pixel based on whether any strip is RGBW
        const int bytes_per_pixel = any_rgbw ? 4 : 3;

        // Get LED data for each pin in the group
        fl::u32 max_bytes = mRectDrawBuffer.getMaxBytesInStrip();
        fl::u32 max_leds = max_bytes / bytes_per_pixel;

        // Allocate transpose buffer if needed
        // RGB: 24 bytes per LED (8 bits * 3 colors)
        // RGBW: 32 bytes per LED (8 bits * 4 channels)
        fl::u32 needed_buffer_size = max_leds * (bytes_per_pixel * 8);
        if (group->buffer_size < needed_buffer_size) {
            group->transpose_buffer.reset(new fl::u8[needed_buffer_size]);  // ok bare allocation
            group->buffer_size = needed_buffer_size;
            fl::memset(group->transpose_buffer.get(), 0, needed_buffer_size);
        }

        // Build array of pointers to LED data for each pin in the group
        const fl::u8* strip_ptrs[8];  // Max 8 strips
        for (fl::u8 i = 0; i < group->num_pins; i++) {
            fl::u8 pin = group->base_pin + i;
            fl::span<fl::u8> led_data = mRectDrawBuffer.getLedsBufferBytesForPin(pin, false);
            strip_ptrs[i] = led_data.data();
        }

        // Transpose based on group size and pixel format (RGB vs RGBW)
        fl::u8 bytes_per_led = any_rgbw ? 4 : 3;
        switch (group->num_pins) {
            case 8:
                fl::transpose_8strips(strip_ptrs, group->transpose_buffer.get(), max_leds, bytes_per_led);
                break;
            case 4:
                fl::transpose_4strips(strip_ptrs, group->transpose_buffer.get(), max_leds, bytes_per_led);
                break;
            case 2:
                fl::transpose_2strips(strip_ptrs, group->transpose_buffer.get(), max_leds, bytes_per_led);
                break;
            default:
                FL_WARN_F("Invalid parallel group size: %s", (int)group->num_pins);
                return;
        }

        FL_DBG_F("Transposed %s-pin group at GPIO %s (%s LEDs, %s bytes)", group->num_pins, (int)group->base_pin, max_leds, needed_buffer_size);

        // transpose_* emits one byte per bit plane. RpPioTxPeripheral consumes
        // one 32-bit DMA word per plane, with the active lane bits in the MSB.
        fl::vector<fl::u32> words;
        words.reserve(needed_buffer_size);
        for (fl::u32 i = 0; i < needed_buffer_size; ++i) {
            const u32 plane = group->transpose_buffer[i] &
                              static_cast<u32>((1u << group->num_pins) - 1u);
            words.push_back(plane << (32u - group->num_pins));
        }
        if (transmitWords(group->base_pin, group->num_pins, words)) {
            FL_DBG_F("Parallel output for %s pins starting at GPIO %s (%s bytes)",
                     group->num_pins, (int)group->base_pin, needed_buffer_size);
        }
    }
};



void RP2040_PIO_Parallel::beginShowLeds(int data_pin, int nleds, bool is_rgbw) {
    RP2040ParallelGroup& group = RP2040ParallelGroup::getInstance();
    group.onQueuingStart();
    group.addObject(data_pin, nleds, is_rgbw);
}

void RP2040_PIO_Parallel::showPixels(u8 data_pin, PixelIterator& pixel_iterator) {
    RP2040ParallelGroup& group = RP2040ParallelGroup::getInstance();
    group.onQueuingDone();

    const Rgbw rgbw = pixel_iterator.get_rgbw();

    // Get buffer slice for this pin
    span<u8> strip_bytes = group.mRectDrawBuffer.getLedsBufferBytesForPin(data_pin, true);

    if (rgbw.active()) {
        // RGBW mode
        u8 r, g, b, w;
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
        // RGB mode
        u8 r, g, b;
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

void RP2040_PIO_Parallel::endShowLeds() {
    // First controller to call this draws everything, others are ignored
    RP2040ParallelGroup::getInstance().showPixelsOnceThisFrame();
}

}  // namespace fl

#endif  // defined(FL_IS_RP2040) || defined(FL_IS_RP2350)
