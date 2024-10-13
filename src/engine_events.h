#pragma once

#include "singleton.h"
#include "fixed_vector.h"

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
        Listener(bool auto_attach = true);
        virtual ~Listener();
        virtual void onBeginFrame() {}
        virtual void onEndFrame() {}
        virtual void onStripAdded(CLEDController* strip, uint32_t num_leds) {}
    };

    static void addListener(Listener* listener);
    static void removeListener(Listener* listener);
    static void onBeginFrame();
    static void onEndFrame();
    static void onStripAdded(CLEDController* strip, uint32_t num_leds);
private:
    // Safe to add a listeners during a callback.
    void _addListener(Listener* listener);
    // Safe to remove self during a callback.
    void _removeListener(Listener* listener);
    void _onBeginFrame();
    void _onEndFrame();
    void _onStripAdded(CLEDController* strip, uint32_t num_leds);
    #ifndef __AVR__
    typedef FixedVector<Listener*, FASTLED_ENGINE_EVENTS_MAX_LISTENERS> ListenerList;
    ListenerList mListeners;
    #endif

    static EngineEvents* getInstance();

    EngineEvents() = default;
    friend class Singleton<EngineEvents>;
};

FASTLED_NAMESPACE_END
