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
#include <vector>

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

    void onEndShowLeds() override {
        if (mId == -1) {
            mId = StripIdMap::getOrFindByAddress(reinterpret_cast<uint32_t>(this));
        }
		ActiveStripData& ch_data = Singleton<ActiveStripData>::instance();
		ch_data.update(mId, millis(), mBuffer.data(), mBuffer.size());
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
