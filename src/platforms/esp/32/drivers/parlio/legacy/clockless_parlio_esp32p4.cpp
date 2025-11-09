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
#include "../bulk/parlio_channel.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "../bulk/parlio_group.h"
#include "../bulk/parlio_manager.h"

// Runtime driver creation with chipset timing config
namespace fl {
    fl::unique_ptr<fl::IParlioChannel> createParlioChannelRuntime(uint8_t width, const ChipsetTimingConfig& timing) {
        return fl::IParlioChannel::create(timing, width);
    }
}

// Legacy ParlioTransmitterBase class removed - replaced by ParlioGroup in bulk/ architecture
// See Task 5.1 in LOOP.md for migration details

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
    auto& group = ParlioGroup::getOrCreate(mTiming);

    // Cross-chipset coordination: Flush any pending groups of DIFFERENT chipset types
    // This ensures that if the user mixes chipsets (e.g., WS2812 + SK6812), the previous
    // chipset's data transmits before we start queuing data for this chipset.
    // Same-chipset strips can batch together for efficiency.
    IParlioManager::getInstance().flushAllExcept(&group);

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
    auto& group = ParlioGroup::getOrCreate(mTiming);

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
    auto& group = ParlioGroup::getOrCreate(mTiming);
    group.showPixelsOnceThisFrame();
}

// createParlioTransmitterBase() factory function removed - no longer used
// ParlioGroup is now created by ParlioGroup::getOrCreate() factory in parlio_group.cpp

// Note: Template method implementations have been moved to the header file.
// ClocklessController_Parlio_Esp32P4 is a thin template wrapper that converts
// compile-time template parameters to runtime values, then delegates all work
// to the non-template ParlioChannelDriver class. This follows the same pattern
// as RMT5 (ClocklessRMT → RmtController5).

} // namespace fl

// ===== C-linkage function for FastLED.cpp integration =====
// See BULK_CLOCKLESS_DESIGN.md §8 - FastLED.cpp must call flushAll() at frame end

namespace fl {
void parlio_flush_all_groups() {
    IParlioManager::getInstance().flushAll();
}
}  // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32P4
#endif // ESP32
