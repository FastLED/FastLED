#pragma once

#ifndef __EMSCRIPTEN__
#error "This file should only be included in an Emscripten build"
#endif

#include <stdint.h>

#include <vector>
#include <stdio.h>


#include "namespace.h"
#include "active_strip_data.h"
#include "singleton.h"
#include "ui/events.h"
#include "strip_id_map.h"
#include "crgb.h"
#include "pixel_controller.h"
#include "dither_mode.h"

FASTLED_NAMESPACE_BEGIN

extern uint8_t get_brightness();

#define FASTLED_ALL_PINS_HARDWARE_SPI


class WasmSpiOutput: public EngineEvents::Listener {
public:
    WasmSpiOutput() {
        EngineEvents::addListener(this);
    }

    ~WasmSpiOutput() {
        EngineEvents::removeListener(this);
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
        if (mId == -1) {
            int new_id = StripIdMap::getId(owner);
            if (new_id != -1) {
                mId = new_id;
            }
        }
        ColorAdjustment color_adjustment = owner->getAdjustmentData(get_brightness());
        PixelController<RGB> pixels(owner->leds(), owner->size(), color_adjustment, DISABLE_DITHER);
        pixels.disableColorAdjustment();
        mRgb.clear();
        while (pixels.has(1)) {
            uint8_t r, g, b;
            pixels.loadAndScaleRGB(&r, &g, &b);
            mRgb.push_back(r);
            mRgb.push_back(g);
            mRgb.push_back(b);
            pixels.advanceData();
        }
		ActiveStripData& active_strips = Singleton<ActiveStripData>::instance();
		active_strips.update(mId, millis(), mRgb.data(), mRgb.size());
    }

    void select() {}
    void init() {}
    void waitFully() {}
    void release() {}

    void writeByte(uint8_t byte) {
    }

    void writeWord(uint16_t word) {
        writeByte(word >> 8);
        writeByte(word & 0xFF);
    }

private:
    int mId = -1;  // Deferred initialization
    std::vector<uint8_t> mRgb;
};

// Compatibility alias
typedef WasmSpiOutput StubSPIOutput;

FASTLED_NAMESPACE_END
