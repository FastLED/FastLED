#ifdef __EMSCRIPTEN__


#define FASTLED_INTERNAL
#include "FastLED.h"

#include "platforms/wasm/fastspi_wasm.h"

namespace fl {

extern uint8_t get_brightness();

CLEDController *WasmSpiOutput::tryFindOwner() {
    if (mId == -1) {
        mId = StripIdMap::getOrFindByAddress(reinterpret_cast<uint32_t>(this));
    }
    if (mId == -1) {
        return nullptr;
    }
    return StripIdMap::getOwner(mId);
}

WasmSpiOutput::WasmSpiOutput() { EngineEvents::addListener(this); }
WasmSpiOutput::~WasmSpiOutput() { EngineEvents::removeListener(this); }

void WasmSpiOutput::onEndShowLeds() {
    // Get the led data and send it to the JavaScript side. This is tricky
    // because we have to find the owner of this pointer, which will be
    // inlined in a CLEDController subclass. Therefore we are going to do
    // address lookup to get the CLEDController for all CLEDController
    // instances that exist and select the one in which this SpiOutput class
    // would be inlined into.
    CLEDController *owner = tryFindOwner();
    if (owner == nullptr) {
        return;
    }
    if (mId == -1) {
        int new_id = StripIdMap::getId(owner);
        if (new_id != -1) {
            mId = new_id;
        }
    }
    ColorAdjustment color_adjustment =
        owner->getAdjustmentData(get_brightness());
    PixelController<RGB> pixels(owner->leds(), owner->size(), color_adjustment,
                                DISABLE_DITHER);
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
    ActiveStripData &active_strips = Singleton<ActiveStripData>::instance();
    active_strips.update(mId, millis(), mRgb.data(), mRgb.size());
}

void WasmSpiOutput::select() { mRgb.clear(); }

void WasmSpiOutput::init() { mRgb.clear(); }

void WasmSpiOutput::waitFully() {}

void WasmSpiOutput::release() {}

void WasmSpiOutput::writeByte(uint8_t byte) {
    // FASTLED_WARN("writeByte %d\n", byte);
    mRgb.push_back(byte);
}

void WasmSpiOutput::writeWord(uint16_t word) {
    writeByte(word >> 8);
    writeByte(word & 0xFF);
}

} // namespace fl

#endif // __EMSCRIPTEN__
