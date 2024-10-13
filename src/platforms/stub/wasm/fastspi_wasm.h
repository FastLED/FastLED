#pragma once

#ifndef __EMSCRIPTEN__
#error "This file should only be included in an Emscripten build"
#endif

#include <stdint.h>

#include "namespace.h"
#include "channel_data.h"
#include "singleton.h"
#include <vector>

FASTLED_NAMESPACE_BEGIN

#define FASTLED_ALL_PINS_HARDWARE_SPI

class WasmSpiInstanceCounter {
public:
    static WasmSpiInstanceCounter& getInstance();

    uint32_t increment() {
        return mCount++;
    }
private:
    uint32_t mCount = 0;
};

inline WasmSpiInstanceCounter& WasmSpiInstanceCounter::getInstance() {
    WasmSpiInstanceCounter& out = Singleton<WasmSpiInstanceCounter>::instance();
    return out;
}

class WasmSpiOutput: public EndFrameListener {
public:
    WasmSpiOutput() {
        mId = WasmSpiInstanceCounter::getInstance().increment();
        if (EndFrame* ef = EndFrame::getInstance()) {
            ef->addListener(this);
        }
    }

    ~WasmSpiOutput() {
        if (EndFrame* ef = EndFrame::getInstance()) {
            ef->removeListener(this);
        }
    }

    void onBeginFrame () override {
        mBuffer.clear();
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
    int mId = 0;
    std::vector<uint8_t> mBuffer;
};

// Compatibility alias
typedef WasmSpiOutput StubSPIOutput;

FASTLED_NAMESPACE_END
