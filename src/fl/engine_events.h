#pragma once

#include "fl/namespace.h"
#include "fl/screenmap.h"
#include "fl/singleton.h"
#include "fl/vector.h"
#include "fl/xymap.h"
#include "fl/string.h"
#include "fl/int.h"

#ifndef FASTLED_HAS_ENGINE_EVENTS
#define FASTLED_HAS_ENGINE_EVENTS SKETCH_HAS_LOTS_OF_MEMORY
#endif // FASTLED_HAS_ENGINE_EVENTS

FASTLED_NAMESPACE_BEGIN
class CLEDController;
FASTLED_NAMESPACE_END


namespace fl {

class EngineEvents {
  public:
    class Listener {
      public:
        // Note that the subclass must call EngineEvents::addListener(this) to
        // start listening. In the subclass destructor, the subclass should call
        // EngineEvents::removeListener(this).
        Listener();
        virtual ~Listener();
        virtual void onBeginFrame() {}
        virtual void onEndShowLeds() {}
        virtual void onEndFrame() {}
        virtual void onStripAdded(CLEDController *strip, fl::u32 num_leds) {
            (void)strip;
            (void)num_leds;
        }
        // Called to set the canvas for UI elements for a particular strip.
        virtual void onCanvasUiSet(CLEDController *strip,
                                   const ScreenMap &screenmap) {
            (void)strip;
            (void)screenmap;
        }
        virtual void onPlatformPreLoop() {}
        virtual void onPlatformPreLoop2() {}
    };

    static void addListener(Listener *listener, int priority = 0) {
#if FASTLED_HAS_ENGINE_EVENTS
        EngineEvents::getInstance()->_addListener(listener, priority);
#else
        (void)listener;
        (void)priority;
#endif
    }

    static void removeListener(Listener *listener) {
#if FASTLED_HAS_ENGINE_EVENTS
        EngineEvents::getInstance()->_removeListener(listener);
#else
        (void)listener;
#endif
    }

    static bool hasListener(Listener *listener) {
#if FASTLED_HAS_ENGINE_EVENTS
        return EngineEvents::getInstance()->_hasListener(listener);
#else
        (void)listener;
        return false;
#endif
    }

    static void onBeginFrame() {
#if FASTLED_HAS_ENGINE_EVENTS
        EngineEvents::getInstance()->_onBeginFrame();
#endif
    }

    static void onEndShowLeds() {
#if FASTLED_HAS_ENGINE_EVENTS
        EngineEvents::getInstance()->_onEndShowLeds();
#endif
    }

    static void onEndFrame() {
#if FASTLED_HAS_ENGINE_EVENTS
        EngineEvents::getInstance()->_onEndFrame();
#endif
    }

    static void onStripAdded(CLEDController *strip, fl::u32 num_leds) {
#if FASTLED_HAS_ENGINE_EVENTS
        EngineEvents::getInstance()->_onStripAdded(strip, num_leds);
#else
        (void)strip;
        (void)num_leds;
#endif
    }

    static void onCanvasUiSet(CLEDController *strip, const ScreenMap &xymap) {
#if FASTLED_HAS_ENGINE_EVENTS
        EngineEvents::getInstance()->_onCanvasUiSet(strip, xymap);
#else
        (void)strip;
        (void)xymap;
#endif
    }

    static void onPlatformPreLoop() {
#if FASTLED_HAS_ENGINE_EVENTS
        EngineEvents::getInstance()->_onPlatformPreLoop();
#endif
    }

    // Needed by fl::vector<T>
    EngineEvents() = default;

  private:
    // Safe to add a listeners during a callback.
    void _addListener(Listener *listener, int priority);
    // Safe to remove self during a callback.
    void _removeListener(Listener *listener);
    void _onBeginFrame();
    void _onEndShowLeds();
    void _onEndFrame();
    void _onStripAdded(CLEDController *strip, fl::u32 num_leds);
    void _onCanvasUiSet(CLEDController *strip, const ScreenMap &xymap);
    void _onPlatformPreLoop();
    bool _hasListener(Listener *listener);
#if FASTLED_HAS_ENGINE_EVENTS
    struct Pair {
        Pair() = default;
        Listener *listener = nullptr;
        int priority = 0;
        Pair(Listener *listener, int priority)
            : listener(listener), priority(priority) {}
    };

    typedef fl::vector_inlined<Pair, 16> ListenerList;
    ListenerList mListeners;


    static EngineEvents *getInstance();

    friend class fl::Singleton<EngineEvents>;
#endif  // FASTLED_HAS_ENGINE_EVENTS
};

} // namespace fl
