// IWYU pragma: private

#ifdef FASTLED_STUB_IMPL

#define FASTLED_INTERNAL
#include "fl/system/fastled.h"

#include "platforms/stub/fastspi_stub_generic.h"
#include "fl/stl/noexcept.h"

extern fl::u8 get_brightness() FL_NOEXCEPT;

namespace fl {

StubSPIOutput::StubSPIOutput() FL_NOEXCEPT { EngineEvents::addListener(this); }
StubSPIOutput::~StubSPIOutput() { EngineEvents::removeListener(this); }

void StubSPIOutput::onEndShowLeds() FL_NOEXCEPT {
    // Simply push the captured SPI transmission bytes through the tracker
    // The mBytes buffer is populated by writeByte() calls during LED transmission
    mTracker.update(mBytes);
}

void StubSPIOutput::select() FL_NOEXCEPT { mBytes.clear(); }

void StubSPIOutput::init() FL_NOEXCEPT { mBytes.clear(); }

void StubSPIOutput::waitFully() FL_NOEXCEPT {}

void StubSPIOutput::release() FL_NOEXCEPT {}

void StubSPIOutput::writeByte(u8 byte) FL_NOEXCEPT {
    mBytes.push_back(byte);
}

void StubSPIOutput::writeWord(u16 word) FL_NOEXCEPT {
    writeByte(word >> 8);
    writeByte(word & 0xFF);
}

} // namespace fl

#endif // FASTLED_STUB_IMPL
