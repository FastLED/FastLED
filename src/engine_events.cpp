#include "engine_events.h"
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN



EngineEvents::Listener::Listener() {
}

EngineEvents::Listener::~Listener() {
    EngineEvents* ptr = EngineEvents::getInstance();
    const bool has_listener = ptr && ptr->mListeners.has(this);
    if (has_listener) {
        // Warning, the listener should be removed by the subclass. If we are here
        // then the subclass did not remove the listener and we are now in a partial
        // state of destruction and the results may be undefined.
        ptr->_removeListener(this);
    }
}

EngineEvents* EngineEvents::getInstance() {
    #ifdef __AVR__
    return nullptr;  // strip out for avr.
    #else
    return &Singleton<EngineEvents>::instance();
    #endif
}

void EngineEvents::addListener(Listener* listener) {
    if (auto ptr = EngineEvents::getInstance()) {
        ptr->_addListener(listener);
    }
}

void EngineEvents::removeListener(Listener* listener) {
    if (auto ptr = EngineEvents::getInstance()) {
        ptr->_removeListener(listener);
    }
}

bool EngineEvents::hasListener(Listener* listener) {
    #ifdef __AVR__
    (void)listener;
    return false;
    #else
    if (auto ptr = EngineEvents::getInstance()) {
        if (ptr->mListeners.has(listener)) {
            return true;
        }
    }
    return false;
    #endif
}

void EngineEvents::onBeginFrame() {
    if (auto ptr = EngineEvents::getInstance()) {
        ptr->_onBeginFrame();
    }
}

void EngineEvents::onEndShowLeds() {
    if (auto ptr = EngineEvents::getInstance()) {
        ptr->_onEndShowLeds();
    }
}

void EngineEvents::onEndFrame() {
    if (auto ptr = EngineEvents::getInstance()) {
        ptr->_onEndFrame();
    }
}

void EngineEvents::onStripAdded(CLEDController* strip, uint32_t num_leds) {
    if (auto ptr = EngineEvents::getInstance()) {
        ptr->_onStripAdded(strip, num_leds);
    }
}

void EngineEvents::_addListener(Listener* listener) {
    #ifdef __AVR__
    (void)listener;
    #else
    if (mListeners.has(listener)) {
        return;
    }
    mListeners.push_back(listener);
    #endif
}

void EngineEvents::_removeListener(Listener* listener) {
    #ifdef __AVR__
    (void)listener;
    #else
    mListeners.erase(listener);
    #endif
}

void EngineEvents::_onBeginFrame() {
    #ifdef __AVR__
    return;
    #else
    // Make the copy of the listener list to avoid issues with listeners being added or removed during the loop.
    ListenerList copy = mListeners;
    for (auto listener : copy) {
        listener->onBeginFrame();
    }
    #endif
}

void EngineEvents::_onEndShowLeds() {
    #ifdef __AVR__
    return;
    #else
    // Make the copy of the listener list to avoid issues with listeners being added or removed during the loop.
    ListenerList copy = mListeners;
    for (auto listener : copy) {
        listener->onEndShowLeds();
    }
    #endif
}

void EngineEvents::_onEndFrame() {
    #ifdef __AVR__
    return;
    #else
    // Make the copy of the listener list to avoid issues with listeners being added or removed during the loop.
    ListenerList copy = mListeners;
    for (auto listener : copy) {
        listener->onEndFrame();
    }
    #endif
}

void EngineEvents::_onStripAdded(CLEDController* strip, uint32_t num_leds) {
    #ifdef __AVR__
    return;
    #else
    // Make the copy of the listener list to avoid issues with listeners being added or removed during the loop.
    ListenerList copy = mListeners;
    for (auto listener : copy) {
        listener->onStripAdded(strip, num_leds);
    }
    #endif
}

FASTLED_NAMESPACE_END
