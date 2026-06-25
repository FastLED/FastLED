/// FastLED mapping of the ObjectFLED driver for Teensy 4.0/4.1.
///
/// This driver supports up to 42 parallel strips of ANY clockless LED chipsets!
/// ~7x that of OctoWS2811!
///
/// NEW: Generic proxy architecture supporting ALL clockless chipsets:
/// - WS2812, WS2811, WS2813, WS2815
/// - SK6812, SK6822
/// - And any other clockless chipset with timing definitions
///
/// The theoretical limit of Teensy 4.0, if frames per second is not a concern, is
/// more than 200k pixels. However, realistically, to run 42 strips at 550 pixels
/// each at 60fps, is 23k pixels.
///
/// @author Kurt Funderburg
/// @reddit: reddit.com/u/Tiny_Structure_7
/// The FastLED code was written by Zach Vorhies
/// @author Kurt Funderburg
/// @reddit: reddit.com/u/Tiny_Structure_7
/// @author: Zach Vorhies (FastLED code)
/// @reddit: reddit.com/u/ZachVorhies


#pragma once

// IWYU pragma: private

#include "cpixel_ledcontroller.h"
#include "pixel_iterator.h"
#include "fl/gfx/rectangular_draw_buffer.h"
#include "fl/stl/vector.h"
#include "fl/stl/singleton.h"
#include "fl/stl/map.h"
#include "fl/stl/noexcept.h"
#include "fl/system/engine_events.h"

#ifndef FASTLED_OBJECTFLED_LATCH_DELAY
#define FASTLED_OBJECTFLED_LATCH_DELAY 300  // WS2812-5VB
#endif

namespace fl {

// Forward declarations
template <typename TIMING> class ObjectFLEDGroup;

// ============================================================================
// ObjectFLED Registry (tracks all active chipset groups)
// ============================================================================

/// Track all active chipset groups across all chipset types
class ObjectFLEDRegistry : public EngineEvents::Listener {
public:
    ObjectFLEDRegistry() FL_NO_EXCEPT;
    ~ObjectFLEDRegistry() FL_NO_EXCEPT override;

    static ObjectFLEDRegistry& getInstance() FL_NO_EXCEPT {
        return fl::Singleton<ObjectFLEDRegistry>::instance();
    }

    // Register a group for tracking
    void registerGroup(void* groupPtr, void (*flushFunc)(void*)) FL_NO_EXCEPT;

    // Flush all pending groups
    void flushAll() FL_NO_EXCEPT;

    // Flush all groups except the specified one
    void flushAllExcept(void* exceptPtr) FL_NO_EXCEPT;

    void onEndFrame() FL_NO_EXCEPT override;

private:
    struct GroupEntry {
        void* groupPtr;
        void (*flushFunc)(void*);

        bool operator==(const GroupEntry& other) const {
            return groupPtr == other.groupPtr;
        }
    };

    fl::vector<GroupEntry> mGroups;

    bool contains(const GroupEntry& entry) FL_NO_EXCEPT;
};

// ============================================================================
// Concrete ObjectFLED Group (runtime timing configuration)
// ============================================================================

// Timing configuration passed at runtime instead of compile-time
struct ObjectFLEDTimingConfig {
    u32 T1;     // High time (ns)
    u32 T2;     // Low time (ns)
    u32 T3;     // Total bit period (ns)
    u32 RESET;  // Reset/latch time (ns)
};

/// Concrete (non-template) group that manages ObjectFLED for a specific timing
/// This does all the real work - templates just delegate to this
class ObjectFLEDGroupBase {
public:
    ObjectFLEDGroupBase(const ObjectFLEDTimingConfig& timing) FL_NO_EXCEPT;
    ~ObjectFLEDGroupBase();

    // Called by proxy in beginShowLeds()
    void onQueuingStart() FL_NO_EXCEPT;

    // Called by proxy in showPixels()
    void addStrip(u8 pin, PixelIterator& pixel_iterator) FL_NO_EXCEPT;

    // Called by registry when chipset changes or frame ends
    void flush() FL_NO_EXCEPT;

private:
    void rebuildObjectFLED() FL_NO_EXCEPT;

    ObjectFLEDTimingConfig mTiming;
    RectangularDrawBuffer mRectDrawBuffer;
    struct PendingStrip {
        u8 pin = 0;
        fl::vector<u8> bytes;
    };
    fl::vector<PendingStrip> mPendingStrips;
    void* mObjectFLED;  // Opaque pointer to ObjectFLED
    bool mDrawn;
};

/// Templated singleton wrapper - one instance per chipset type
/// This is just a thin wrapper that converts compile-time TIMING to runtime config
template <typename TIMING>
class ObjectFLEDGroup {
public:
    static ObjectFLEDGroup& getInstance() FL_NO_EXCEPT {
        return fl::Singleton<ObjectFLEDGroup<TIMING>>::instance();
    }

    ObjectFLEDGroup()
        : mBase(ObjectFLEDTimingConfig{TIMING::T1, TIMING::T2, TIMING::T3, TIMING::RESET}) {
        registerWithRegistry();
    }

    void onQueuingStart() {
        registerWithRegistry();
        mBase.onQueuingStart();
    }
    void addStrip(u8 pin, PixelIterator& pixel_iterator) FL_NO_EXCEPT {
        mBase.addStrip(pin, pixel_iterator);
    }
    void flush() { mBase.flush(); }

private:
    static void flushRegisteredGroup(void* ptr) FL_NO_EXCEPT {
        static_cast<ObjectFLEDGroup*>(ptr)->flush();
    }

    void registerWithRegistry() FL_NO_EXCEPT {
        ObjectFLEDRegistry::getInstance().registerGroup(
            this,
            &ObjectFLEDGroup<TIMING>::flushRegisteredGroup
        );
    }

    ObjectFLEDGroupBase mBase;  // Delegate all work to concrete base
};

/// Generic proxy controller for ObjectFLED on Teensy 4.x
/// Works with ANY clockless chipset timing (WS2812, SK6812, WS2811, etc.)
///
/// Automatically routes to the correct ObjectFLEDGroup<TIMING> singleton
/// based on the TIMING template parameter.
template <typename TIMING, int DATA_PIN, EOrder RGB_ORDER = RGB>
class ClocklessController_ObjectFLED_Proxy
    : public CPixelLEDController<RGB_ORDER> {
  private:
    typedef CPixelLEDController<RGB_ORDER> Base;

  public:
    ClocklessController_ObjectFLED_Proxy() FL_NO_EXCEPT;

    void init() override {}

    virtual u16 getMaxRefreshRate() const FL_NO_EXCEPT override {
        // Calculate based on timing: if total period > 2000ns, it's 400kHz, else 800kHz
        u32 total_ns = TIMING::T1 + TIMING::T2 + TIMING::T3;
        return (total_ns > 2000) ? 400 : 800;
    }

  protected:
    virtual void *beginShowLeds(int nleds) override;
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) FL_NO_EXCEPT override;
    virtual void endShowLeds(void *data) FL_NO_EXCEPT override;
};

// ============================================================================
// Template Member Function Implementations (must be in header for visibility)
// ============================================================================

template <typename TIMING, int DATA_PIN, EOrder RGB_ORDER>
ClocklessController_ObjectFLED_Proxy<TIMING, DATA_PIN, RGB_ORDER>::ClocklessController_ObjectFLED_Proxy()
    : Base() {
    // Latch delay is automatically determined from TIMING::RESET
}

template <typename TIMING, int DATA_PIN, EOrder RGB_ORDER>
void *ClocklessController_ObjectFLED_Proxy<TIMING, DATA_PIN, RGB_ORDER>::beginShowLeds(int nleds) {
    void *data = Base::beginShowLeds(nleds);

    // Auto-grab the singleton for THIS chipset type
    ObjectFLEDGroup<TIMING>& group = ObjectFLEDGroup<TIMING>::getInstance();

    // Flush any pending groups of DIFFERENT chipset types
    ObjectFLEDRegistry::getInstance().flushAllExcept(&group);

    // Start queuing for this group
    group.onQueuingStart();

    return data;
}

template <typename TIMING, int DATA_PIN, EOrder RGB_ORDER>
void ClocklessController_ObjectFLED_Proxy<TIMING, DATA_PIN, RGB_ORDER>::showPixels(
    PixelController<RGB_ORDER> &pixels) {
    // Auto-grab the singleton for THIS chipset type
    ObjectFLEDGroup<TIMING>& group = ObjectFLEDGroup<TIMING>::getInstance();

    // Add this strip to the group
    auto pixel_iterator = pixels.as_iterator(this->getRgbw());
    group.addStrip(DATA_PIN, pixel_iterator);
}

template <typename TIMING, int DATA_PIN, EOrder RGB_ORDER>
void ClocklessController_ObjectFLED_Proxy<TIMING, DATA_PIN, RGB_ORDER>::endShowLeds(void *data) {
    Base::endShowLeds(data);

    // DON'T flush here - let chipset change detection or frame end handle it
    // This is handled by the next controller's beginShowLeds() or FastLED.show() end
}

// Legacy WS2812-specific controller (for backward compatibility)
// Now just a typedef to the proxy with WS2812 timing
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
using ClocklessController_ObjectFLED_WS2812 = ClocklessController_ObjectFLED_Proxy<
    struct TIMING_WS2812_800KHZ,
    DATA_PIN,
    RGB_ORDER>;

}  // namespace fl
