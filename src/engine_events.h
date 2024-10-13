#pragma once

#include "singleton.h"
#include "fixed_vector.h"

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

class EngineEvents {
public:
    class Listener {
    public:
        Listener(bool auto_attach = true);
        virtual ~Listener();
        virtual void onBeginFrame() {}
        virtual void onEndFrame() {}
    };

    static void addListener(Listener* listener);
    static void removeListener(Listener* listener);
    static void onBeginFrame();
    static void onEndFrame();

    // Safe to add a listeners during a callback.
    void _addListener(Listener* listener);
    // Safe to remove self during a callback.
    void _removeListener(Listener* listener);
    void _onBeginFrame();
    void _onEndFrame();

private:
    #ifndef __AVR__
    typedef FixedVector<Listener*, 8> ListenerList;
    ListenerList mListeners;
    #endif

    static EngineEvents* getInstance();

    EngineEvents() = default;
    friend class Singleton<EngineEvents>;
};

FASTLED_NAMESPACE_END
