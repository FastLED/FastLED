#pragma once

#include "singleton.h"
#include "fixed_vector.h"

class EndFrameListener {
public:
    virtual void onEndFrame() = 0;
};

class EndFrame {
public:
    static EndFrame* getInstance();

    void addListener(EndFrameListener* listener) {
        #ifdef __AVR__
        (void)listener;
        #else
        mListeners.push_back(listener);
        #endif
    }

    void endFrame() {
        #ifdef __AVR__
        return;
        #else
        for (auto listener : mListeners) {
            listener->onEndFrame();
        }
        #endif
    }

private:
    #ifndef __AVR__
    typedef FixedVector<EndFrameListener*, 8> ListenerList;
    ListenerList mListeners;
    #endif

    // Make constructor private to enforce singleton pattern
    EndFrame() = default;
    friend class Singleton<EndFrame>;
};

EndFrame* EndFrame::getInstance() {
    #ifdef __AVR__
    return nullptr;  // strip out for avr.
    #else
    return &Singleton<EndFrame>::instance();
    #endif
}
