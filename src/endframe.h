#pragma once

#include "singleton.h"
#include "fixed_vector.h"

class Listener;



class EndFrame {
public:

    class Listener {
    public:
        Listener(bool auto_attach = true) {
            if (auto_attach) {
                EndFrame* ptr = EndFrame::getInstance();
                if (ptr) {
                    ptr->addListener(this);
                }
            }
        }
        virtual ~Listener() {
            EndFrame* ptr = EndFrame::getInstance();
            if (ptr) {
                ptr->removeListener(this);
            }
        }
        virtual void onBeginFrame() {}
        virtual void onEndFrame() {}
    };

    static EndFrame* getInstance();

    void addListener(Listener* listener) {
        #ifdef __AVR__
        (void)listener;
        #else
        if (mListeners.has(listener)) {
            return;
        }
        mListeners.push_back(listener);
        #endif
    }

    void removeListener(Listener* listener) {
        #ifdef __AVR__
        (void)listener;
        #else
        mListeners.erase(listener);
        #endif
    }

    // Called right before endFrame. Gives listeners a chance to prepare data for the call to endFrame.
    void onBeginFrame() {
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

    // Called when the frame ends.
    void onEndFrame() {
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


private:
    #ifndef __AVR__
    typedef FixedVector<Listener*, 8> ListenerList;
    ListenerList mListeners;
    #endif

    // Make constructor private to enforce singleton pattern
    EndFrame() = default;
    friend class Singleton<EndFrame>;
};



inline EndFrame* EndFrame::getInstance() {
    #ifdef __AVR__
    return nullptr;  // strip out for avr.
    #else
    return &Singleton<EndFrame>::instance();
    #endif
}
