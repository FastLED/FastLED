#pragma once

#include "fl/math/screenmap.h"  // IWYU pragma: keep
#include "fl/stl/singleton.h"
#include "fl/stl/vector.h"
#include "fl/math/xymap.h"  // IWYU pragma: keep
#include "fl/stl/string.h"  // IWYU pragma: keep
#include "fl/stl/int.h"
#include "fl/system/sketch_macros.h"
#include "fl/stl/noexcept.h"

#ifndef FASTLED_HAS_ENGINE_EVENTS
#define FASTLED_HAS_ENGINE_EVENTS SKETCH_HAS_LARGE_MEMORY
#endif // FASTLED_HAS_ENGINE_EVENTS


namespace fl {

class CLEDController;  // forward declaration

class EngineEvents {
  public:
    class Listener {
      public:
        // Note that the subclass must call EngineEvents::addListener(this) to
        // start listening. In the subclass destructor, the subclass should call
        // EngineEvents::removeListener(this).
        Listener() FL_NOEXCEPT;
        virtual ~Listener() FL_NOEXCEPT;
        virtual void onBeginFrame() FL_NOEXCEPT {}
        virtual void onEndShowLeds() FL_NOEXCEPT {}
        virtual void onEndFrame() FL_NOEXCEPT {}
        virtual void onStripAdded(CLEDController *strip, fl::u32 num_leds) FL_NOEXCEPT {
            (void)strip;
            (void)num_leds;
        }
        // Called to set the canvas for UI elements for a particular strip.
        virtual void onCanvasUiSet(CLEDController *strip,
                                   const ScreenMap &screenmap) FL_NOEXCEPT {
            (void)strip;
            (void)screenmap;
        }
        virtual void onPlatformPreLoop() FL_NOEXCEPT {}
        virtual void onPlatformPreLoop2() FL_NOEXCEPT {}
        virtual void onExit() FL_NOEXCEPT {}  // Called before engine shutdown
    };

    static void addListener(Listener *listener, int priority = 0) FL_NOEXCEPT {
#if FASTLED_HAS_ENGINE_EVENTS
        EngineEvents::getInstance()->_addListener(listener, priority);
#else
        (void)listener;
        (void)priority;
#endif
    }

    static void removeListener(Listener *listener) FL_NOEXCEPT {
#if FASTLED_HAS_ENGINE_EVENTS
        EngineEvents::getInstance()->_removeListener(listener);
#else
        (void)listener;
#endif
    }

    static bool hasListener(Listener *listener) FL_NOEXCEPT {
#if FASTLED_HAS_ENGINE_EVENTS
        return EngineEvents::getInstance()->_hasListener(listener);
#else
        (void)listener;
        return false;
#endif
    }

    static void onBeginFrame() FL_NOEXCEPT {
#if FASTLED_HAS_ENGINE_EVENTS
        EngineEvents::getInstance()->_onBeginFrame();
#endif
    }

    static void onEndShowLeds() FL_NOEXCEPT {
#if FASTLED_HAS_ENGINE_EVENTS
        EngineEvents::getInstance()->_onEndShowLeds();
#endif
    }

    static void onEndFrame() FL_NOEXCEPT {
#if FASTLED_HAS_ENGINE_EVENTS
        EngineEvents::getInstance()->_onEndFrame();
#endif
    }

    static void onStripAdded(CLEDController *strip, fl::u32 num_leds) FL_NOEXCEPT {
#if FASTLED_HAS_ENGINE_EVENTS
        EngineEvents::getInstance()->_onStripAdded(strip, num_leds);
#else
        (void)strip;
        (void)num_leds;
#endif
    }

    static void onCanvasUiSet(CLEDController *strip, const ScreenMap &xymap) FL_NOEXCEPT {
#if FASTLED_HAS_ENGINE_EVENTS
        EngineEvents::getInstance()->_onCanvasUiSet(strip, xymap);
#else
        (void)strip;
        (void)xymap;
#endif
    }

    static void onPlatformPreLoop() FL_NOEXCEPT {
#if FASTLED_HAS_ENGINE_EVENTS
        EngineEvents::getInstance()->_onPlatformPreLoop();
#endif
    }

    static void onExit() FL_NOEXCEPT {
#if FASTLED_HAS_ENGINE_EVENTS
        EngineEvents::getInstance()->_onExit();
#endif
    }

    // Needed by fl::vector<T>
    EngineEvents() FL_NOEXCEPT;
    ~EngineEvents() FL_NOEXCEPT;

  private:
    // Safe to add a listeners during a callback.
    void _addListener(Listener *listener, int priority) FL_NOEXCEPT;
    // Safe to remove self during a callback.
    void _removeListener(Listener *listener) FL_NOEXCEPT;
    void _onBeginFrame() FL_NOEXCEPT;
    void _onEndShowLeds() FL_NOEXCEPT;
    void _onEndFrame() FL_NOEXCEPT;
    void _onStripAdded(CLEDController *strip, fl::u32 num_leds) FL_NOEXCEPT;
    void _onCanvasUiSet(CLEDController *strip, const ScreenMap &xymap) FL_NOEXCEPT;
    void _onPlatformPreLoop() FL_NOEXCEPT;
    void _onExit() FL_NOEXCEPT;
    bool _hasListener(Listener *listener) FL_NOEXCEPT;
#if FASTLED_HAS_ENGINE_EVENTS
    struct Pair {
        Pair() FL_NOEXCEPT = default;
        Listener *listener = nullptr;
        int priority = 0;
        Pair(Listener *listener, int priority) FL_NOEXCEPT
            : listener(listener), priority(priority) {}
    };

    typedef fl::vector_inlined<Pair, 16> ListenerList;
    ListenerList mListeners;


    static EngineEvents *getInstance() FL_NOEXCEPT;

    friend class fl::Singleton<EngineEvents>;
#endif  // FASTLED_HAS_ENGINE_EVENTS
};

} // namespace fl
