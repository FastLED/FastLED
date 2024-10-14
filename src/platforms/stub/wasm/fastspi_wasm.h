#pragma once

#ifndef __EMSCRIPTEN__
#error "This file should only be included in an Emscripten build"
#endif

#include <stdint.h>

#include "namespace.h"
#include "active_strip_data.h"
#include "singleton.h"
#include "engine_events.h"
#include "strip_id_map.h"
#include "crgb.h"
#include <vector>
#include <stdio.h>

FASTLED_NAMESPACE_BEGIN

#define FASTLED_ALL_PINS_HARDWARE_SPI


class WasmSpiOutput: public EngineEvents::Listener {
public:
    WasmSpiOutput() {
    }

    ~WasmSpiOutput() {
    }

    void onBeginFrame () override {
        mBuffer.clear();
    }

    CLEDController* tryFindOwner() {
        if (mId == -1) {
            mId = StripIdMap::getOrFindByAddress(reinterpret_cast<uint32_t>(this));
        }
        if (mId == -1) {
            return nullptr;
        }
        return StripIdMap::getOwner(mId);
    }

    void onEndShowLeds() override {
        // Get the led data and send it to the JavaScript side. This is tricky because we
        // have to find the owner of this pointer, which will be inlined in a CLEDController
        // subclass. Therefore we are going to do address lookup to get the CLEDController
        // for all CLEDController instances that exist and select the one in which this SpiOutput
        // class would be inlined into.
        CLEDController* owner = tryFindOwner();
        if (owner == nullptr) {
            return;
        }
        // Okay we got the owner, now we can get the data and send it to the JavaScript side.
        const uint8_t *data = reinterpret_cast<const uint8_t*>(owner->leds());
        const size_t data_size = owner->size() * 3;
		ActiveStripData& active_strips = Singleton<ActiveStripData>::instance();
		active_strips.update(mId, millis(), data, data_size);
    }

    void select() {}
    void init() {}
    void waitFully() {}
    void release() {}

    void writeByte(uint8_t byte) {
        mBuffer.push_back(byte);
    }

    void writeWord(uint16_t word) {
        writeByte(word >> 8);
        writeByte(word & 0xFF);
    }

private:
    int mId = -1;  // Deferred initialization
    std::vector<uint8_t> mBuffer;
};

// Compatibility alias
typedef WasmSpiOutput StubSPIOutput;

FASTLED_NAMESPACE_END
