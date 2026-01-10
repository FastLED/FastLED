
#if defined(PICO_RP2040) || defined(PICO_RP2350) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)

#define FASTLED_INTERNAL
#include "fl/fastled.h"

#include "clockless_rp_pio_auto.h"
#include "parallel_transpose.h"
#include "fl/rectangular_draw_buffer.h"
#include "fl/singleton.h"
#include "fl/stl/vector.h"
#include "fl/warn.h"
#include "fl/math_macros.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/assert.h"
#include "fl/stl/cstring.h"
#include "fl/stl/allocator.h"
#include "fl/stl/map.h"
#include "fl/dbg.h"

// Hardware headers for RP2040/RP2350
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"

namespace fl {

/// @brief Represents a group of consecutive GPIO pins for parallel output
struct PinGroup {
    fl::u8 base_pin;      ///< Starting GPIO pin
    fl::u8 num_pins;      ///< Number of consecutive pins (2, 4, or 8)
    fl::vector<fl::u8> pins;  ///< List of all pins in this group (sorted)

    PIO pio;          ///< PIO instance (pio0 or pio1)
    fl::i32 sm;           ///< State machine index (-1 if not allocated)
    fl::i32 dma_chan;     ///< DMA channel (-1 if not allocated)

    fl::scoped_array<fl::u8> transpose_buffer;  ///< Bit-transposed output buffer
    fl::u32 buffer_size;  ///< Size of transpose buffer

    PinGroup() : base_pin(0), num_pins(0), pio(nullptr), sm(-1), dma_chan(-1), buffer_size(0) {}

    ~PinGroup() {
        cleanup();
    }

    void cleanup() {
        if (dma_chan != -1) {
            dma_channel_unclaim(dma_chan);
            dma_chan = -1;
        }
        if (sm != -1 && pio != nullptr) {
            pio_sm_set_enabled(pio, sm, false);
            pio_sm_unclaim(pio, sm);
            sm = -1;
        }
        transpose_buffer.reset();
        buffer_size = 0;
    }

    bool allocateResources() {
        // Try to claim PIO and DMA resources
        pio = pio0;  // Start with pio0
        sm = pio_claim_unused_sm(pio, false);
        if (sm == -1) {
            // Try pio1
            pio = pio1;
            sm = pio_claim_unused_sm(pio, false);
            if (sm == -1) {
                FL_WARN("Failed to claim PIO state machine for pin group starting at GPIO " << (int)base_pin);
                return false;
            }
        }

        dma_chan = dma_claim_unused_channel(false);
        if (dma_chan == -1) {
            pio_sm_unclaim(pio, sm);
            sm = -1;
            FL_WARN("Failed to claim DMA channel for pin group starting at GPIO " << (int)base_pin);
            return false;
        }

        // Initialize GPIO pins for output
        for (fl::u8 i = 0; i < num_pins; i++) {
            gpio_init(base_pin + i);
            gpio_set_dir(base_pin + i, GPIO_OUT);
        }

        FL_DBG("Allocated resources for " << (int)num_pins << "-pin parallel group at GPIO "
               << (int)base_pin << " (PIO" << (pio == pio0 ? 0 : 1) << ", SM" << sm << ", DMA" << dma_chan << ")");

        return true;
    }
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
    fl::vector<fl::unique_ptr<PinGroup>> mPinGroups;

    // Map from pin number to group index
    fl::SortedHeapMap<fl::u8, fl::u32> mPinToGroupIndex;

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

        FL_DBG("Detecting pin groups from " << sorted_pins.size() << " pins");

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
            fl::unique_ptr<PinGroup> group(new PinGroup());
            group->base_pin = start_pin;
            group->num_pins = group_size;

            // Add pins to group
            for (fl::u8 j = 0; j < group_size; j++) {
                group->pins.push_back(start_pin + j);
                mPinToGroupIndex[start_pin + j] = mPinGroups.size();
            }

            if (group_size > 1) {
                FL_DBG("Created " << (int)group_size << "-pin parallel group at GPIO " << (int)start_pin);
            } else {
                FL_DBG("Created single-pin (sequential) group at GPIO " << (int)start_pin);
            }

            mPinGroups.push_back(fl::move(group));

            // Move to next group
            i += group_size;
        }

        FL_DBG("Total pin groups: " << mPinGroups.size());
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

        bool drawlist_changed = mRectDrawBuffer.mDrawListChangedThisFrame;

        if (drawlist_changed) {
            // Pin configuration changed - rebuild groups
            FL_DBG("Pin configuration changed, rebuilding groups");
            detectPinGroups();

            // Allocate resources for each group
            for (auto it = mPinGroups.begin(); it != mPinGroups.end(); ++it) {
                PinGroup* group = it->get();
                if (group->num_pins > 1) {
                    // Parallel group - allocate PIO/DMA
                    if (!group->allocateResources()) {
                        // Allocation failed - skip this group
                        continue;
                    }
                }
            }
        }

        // Output each group
        for (auto it = mPinGroups.begin(); it != mPinGroups.end(); ++it) {
            PinGroup* group = it->get();

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
    /// @brief Output a single pin (non-parallel fallback)
    void outputSinglePin(PinGroup* group) {
        fl::u8 pin = group->base_pin;

        // Get LED data for this pin from RectangularDrawBuffer
        fl::span<fl::u8> led_data = mRectDrawBuffer.getLedsBufferBytesForPin(pin, false);

        // TODO: Implement sequential PIO output for single pins
        // For now, just log that we would output this data
        FL_DBG("Sequential output for GPIO " << (int)pin << " (" << led_data.size() << " bytes)");

        // In a full implementation, this would use the regular non-parallel
        // clockless driver from clockless_rp_pio.h
    }

    /// @brief Output a parallel group with bit transposition
    void outputParallelGroup(PinGroup* group) {
        if (group->sm == -1 || group->dma_chan == -1) {
            FL_WARN("Parallel group at GPIO " << (int)group->base_pin
                    << " has no allocated resources, skipping");
            return;
        }

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
            group->transpose_buffer.reset(new fl::u8[needed_buffer_size]);
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
                FL_WARN("Invalid parallel group size: " << (int)group->num_pins);
                return;
        }

        FL_DBG("Transposed " << group->num_pins << "-pin group at GPIO " << (int)group->base_pin
               << " (" << max_leds << " LEDs, " << needed_buffer_size << " bytes)");

        // TODO: Configure PIO program and start DMA transfer
        // For now, just log that we would output this data
        FL_DBG("Parallel output for " << group->num_pins << " pins starting at GPIO "
               << (int)group->base_pin << " (" << needed_buffer_size << " bytes)");

        // In a full implementation, this would:
        // 1. Configure PIO program with WS2812 timing
        // 2. Start DMA transfer of transpose_buffer to PIO
        // 3. Wait for DMA completion
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

#endif  // defined(PICO_RP2040) || defined(PICO_RP2350) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
