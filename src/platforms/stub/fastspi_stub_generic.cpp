#ifdef FASTLED_STUB_IMPL

#define FASTLED_INTERNAL
#include "fl/fastled.h"

#include "platforms/stub/fastspi_stub_generic.h"

extern uint8_t get_brightness();

namespace fl {

CLEDController *StubSPIOutput::tryFindOwner() {
    if (mId == -1) {
        mId = StripIdMap::getOrFindByAddress(reinterpret_cast<uintptr_t>(this));
    }
    if (mId == -1) {
        return nullptr;
    }
    return StripIdMap::getOwner(mId);
}

StubSPIOutput::StubSPIOutput() { EngineEvents::addListener(this); }
StubSPIOutput::~StubSPIOutput() { EngineEvents::removeListener(this); }

void StubSPIOutput::onEndShowLeds() {
    // Get the led data and send it to the ActiveStripData system. This is tricky
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
    ActiveStripData &active_strips = ActiveStripData::Instance();
    active_strips.update(mId, millis(), fl::span<const uint8_t>(mRgb.data(), mRgb.size()));
}

void StubSPIOutput::select() { mRgb.clear(); }

void StubSPIOutput::init() { mRgb.clear(); }

void StubSPIOutput::waitFully() {}

void StubSPIOutput::release() {}

void StubSPIOutput::writeByte(uint8_t byte) {
    mRgb.push_back(byte);
}

void StubSPIOutput::writeWord(uint16_t word) {
    writeByte(word >> 8);
    writeByte(word & 0xFF);
}

} // namespace fl

#endif // FASTLED_STUB_IMPL
