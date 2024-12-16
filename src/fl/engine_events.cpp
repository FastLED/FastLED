#include "fl/engine_events.h"
#include "fl/namespace.h"


using namespace fl;

FASTLED_NAMESPACE_BEGIN



EngineEvents::Listener::Listener() {
}

EngineEvents::Listener::~Listener() {
    #if FASTLED_HAS_ENGINE_EVENTS
    EngineEvents* ptr = EngineEvents::getInstance();
    const bool has_listener = ptr && ptr->_hasListener(this);
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
    for (auto& item : mListeners) {
        auto listener = item.listener;
        listener->onPlatformPreLoop();
    }
    for (auto& item : mListeners) {
        auto listener = item.listener;
        listener->onPlatformPreLoop2();
    }
}

bool EngineEvents::_hasListener(Listener* listener) {
    auto predicate = [listener](const Pair& pair) {
        return pair.listener == listener;
    };
    return mListeners.find_if(predicate) != mListeners.end();
}

void EngineEvents::_addListener(Listener* listener, int priority) {
    if (_hasListener(listener)) {
        return;
    }
    for (auto it = mListeners.begin(); it != mListeners.end(); ++it) {
        if (it->priority < priority) {
            // this is now the highest priority in this spot.
            EngineEvents::Pair pair = EngineEvents::Pair(listener, priority);
            mListeners.insert(it, pair);
            return;
        }
    }
    EngineEvents::Pair pair = EngineEvents::Pair(listener, priority);
    mListeners.push_back(pair);
}

void EngineEvents::_removeListener(Listener* listener) {
    auto predicate = [listener](const Pair& pair) {
        return pair.listener == listener;
    };
    auto it = mListeners.find_if(predicate);
    if (it != mListeners.end()) {
        mListeners.erase(it);
    }
}

void EngineEvents::_onBeginFrame() {
    // Make the copy of the listener list to avoid issues with listeners being added or removed during the loop.
    ListenerList copy = mListeners;
    for (auto& item : copy) {
        auto listener = item.listener;
        listener->onBeginFrame();
    }
}

void EngineEvents::_onEndShowLeds() {
    // Make the copy of the listener list to avoid issues with listeners being added or removed during the loop.
    ListenerList copy = mListeners;
    for (auto& item : copy) {
        auto listener = item.listener;
        listener->onEndShowLeds();
    }
}

void EngineEvents::_onEndFrame() {
    // Make the copy of the listener list to avoid issues with listeners being added or removed during the loop.
    ListenerList copy = mListeners;
    for (auto& item : copy) {
        auto listener = item.listener;
        listener->onEndFrame();
    }
}

void EngineEvents::_onStripAdded(CLEDController* strip, uint32_t num_leds) {
    // Make the copy of the listener list to avoid issues with listeners being added or removed during the loop.
    ListenerList copy = mListeners;
    for (auto& item : copy) {
        auto listener = item.listener;
        listener->onStripAdded(strip, num_leds);
    }
}


void EngineEvents::_onCanvasUiSet(CLEDController *strip, const ScreenMap& screenmap) {
    // Make the copy of the listener list to avoid issues with listeners being added or removed during the loop.
    ListenerList copy = mListeners;
    for (auto& item : copy) {
        auto listener = item.listener;
        listener->onCanvasUiSet(strip, screenmap);
    }
}


#endif // FASTLED_HAS_ENGINE_EVENTS

FASTLED_NAMESPACE_END
