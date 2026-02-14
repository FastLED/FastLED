// IWYU pragma: private

#ifdef FASTLED_STUB_IMPL

#define FASTLED_INTERNAL
#include "fl/fastled.h"

#include "platforms/stub/fastspi_stub_generic.h"

extern fl::u8 get_brightness();

namespace fl {

StubSPIOutput::StubSPIOutput() { EngineEvents::addListener(this); }
StubSPIOutput::~StubSPIOutput() { EngineEvents::removeListener(this); }

void StubSPIOutput::onEndShowLeds() {
    // Simply push the captured SPI transmission bytes through the tracker
    // The mBytes buffer is populated by writeByte() calls during LED transmission
    mTracker.update(fl::span<const u8>(mBytes.data(), mBytes.size()));
}

void StubSPIOutput::select() { mBytes.clear(); }

void StubSPIOutput::init() { mBytes.clear(); }

void StubSPIOutput::waitFully() {}

void StubSPIOutput::release() {}

void StubSPIOutput::writeByte(u8 byte) {
    mBytes.push_back(byte);
}

void StubSPIOutput::writeWord(u16 word) {
    writeByte(word >> 8);
    writeByte(word & 0xFF);
}

} // namespace fl

#endif // FASTLED_STUB_IMPL
