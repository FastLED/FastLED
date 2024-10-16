#pragma once

#include "fixed_vector.h"
#include "singleton.h"

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

#ifndef FASTLED_ENGINE_EVENTS_MAX_LISTENERS
#define FASTLED_ENGINE_EVENTS_MAX_LISTENERS 8
#endif

class CLEDController;

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
        virtual void onStripAdded(CLEDController *strip, uint32_t num_leds) {}
        virtual void onPlatformPreLoop() {}  
        virtual void onPlatformPreLoop2() {}
    };
    

    static void addListener(Listener *listener);
    static void removeListener(Listener *listener);
    static bool hasListener(Listener *listener);
    static void onBeginFrame();
    static void onEndShowLeds();
    static void onEndFrame();
    static void onStripAdded(CLEDController *strip, uint32_t num_leds);

    static void onPlatformPreLoop() {
        if (auto ptr = EngineEvents::getInstance()) {
            for (auto listener : ptr->mListeners) {
                listener->onPlatformPreLoop();
            }
            for (auto listener : ptr->mListeners) {
                listener->onPlatformPreLoop2();
            }
        }
    }

  private:
    // Safe to add a listeners during a callback.
    void _addListener(Listener *listener);
    // Safe to remove self during a callback.
    void _removeListener(Listener *listener);
    void _onBeginFrame();
    void _onEndShowLeds();
    void _onEndFrame();
    void _onStripAdded(CLEDController *strip, uint32_t num_leds);
#ifndef __AVR__
    typedef FixedVector<Listener *, FASTLED_ENGINE_EVENTS_MAX_LISTENERS>
        ListenerList;
    ListenerList mListeners;
#endif

    static EngineEvents *getInstance();

    EngineEvents() = default;
    friend class Singleton<EngineEvents>;
};

FASTLED_NAMESPACE_END
