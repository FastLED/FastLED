#include "engine_events.h"
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN



EngineEvents::Listener::Listener() {
}

EngineEvents::Listener::~Listener() {
    #if FASTLED_HAS_ENGINE_EVENTS
    EngineEvents* ptr = EngineEvents::getInstance();
    const bool has_listener = ptr && ptr->mListeners.has(this);
    if (has_listener) {
        // Warning, the listener should be removed by the subclass. If we are here
        // then the subclass did not remove the listener and we are now in a partial
        // state of destruction and the results may be undefined for multithreaded
        // applications. However, for single threaded (the only option as of 2024, October)
        // this will be ok.
        ptr->_removeListener(this);
    }
    #endif
}

EngineEvents* EngineEvents::getInstance() {
    #if FASTLED_HAS_ENGINE_EVENTS
    return &Singleton<EngineEvents>::instance();
    #else
    return nullptr;  // strip out when engine events are disabled.
    #endif
}


#if FASTLED_HAS_ENGINE_EVENTS
void EngineEvents::_onPlatformPreLoop() {
    for (auto listener : mListeners) {
        listener->onPlatformPreLoop();
    }
    for (auto listener : mListeners) {
        listener->onPlatformPreLoop2();
    }
}

void EngineEvents::_addListener(Listener* listener) {
    if (mListeners.has(listener)) {
        return;
    }
    mListeners.push_back(listener);
}

void EngineEvents::_removeListener(Listener* listener) {
    mListeners.erase(listener);
}

void EngineEvents::_onBeginFrame() {
    // Make the copy of the listener list to avoid issues with listeners being added or removed during the loop.
    ListenerList copy = mListeners;
    for (auto listener : copy) {
        listener->onBeginFrame();
    }
}

void EngineEvents::_onEndShowLeds() {
    // Make the copy of the listener list to avoid issues with listeners being added or removed during the loop.
    ListenerList copy = mListeners;
    for (auto listener : copy) {
        listener->onEndShowLeds();
    }
}

void EngineEvents::_onEndFrame() {
    // Make the copy of the listener list to avoid issues with listeners being added or removed during the loop.
    ListenerList copy = mListeners;
    for (auto listener : copy) {
        listener->onEndFrame();
    }
}

void EngineEvents::_onStripAdded(CLEDController* strip, uint32_t num_leds) {
    // Make the copy of the listener list to avoid issues with listeners being added or removed during the loop.
    ListenerList copy = mListeners;
    for (auto listener : copy) {
        listener->onStripAdded(strip, num_leds);
    }
}

void EngineEvents::_onCanvasUiSet(CLEDController* strip, const XYMap& map) {
    // Make the copy of the listener list to avoid issues with listeners being added or removed during the loop.
    ListenerList copy = mListeners;
    for (auto listener : copy) {
        listener->onCanvasUiSet(strip, map);
    }
}


void EngineEvents::_onCanvasUiSet(CLEDController *strip, const ScreenMap& screenmap) {
    // Make the copy of the listener list to avoid issues with listeners being added or removed during the loop.
    ListenerList copy = mListeners;
    for (auto listener : copy) {
        listener->onCanvasUiSet(strip, screenmap);
    }
}


#endif // FASTLED_HAS_ENGINE_EVENTS

FASTLED_NAMESPACE_END
