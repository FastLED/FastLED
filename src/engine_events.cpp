#include "engine_events.h"
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN



EngineEvents::Listener::Listener(bool auto_attach) {
    if (auto_attach) {
        EngineEvents* ptr = EngineEvents::getInstance();
        if (ptr) {
            ptr->addListener(this);
        }
    }
}

EngineEvents::Listener::~Listener() {
    EngineEvents* ptr = EngineEvents::getInstance();
    if (ptr) {
        ptr->removeListener(this);
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

void EngineEvents::onBeginFrame() {
    if (auto ptr = EngineEvents::getInstance()) {
        ptr->_onBeginFrame();
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
