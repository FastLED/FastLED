// ok no header - CFastLED is declared in FastLED.h
#define FASTLED_INTERNAL
// IWYU pragma: private

#include "FastLED.h"  // ok include: implements CFastLED audio methods
#include "fl/audio/audio_manager.h"
#include "fl/audio/fft/fft.h"
#include "fl/stl/string.h"
#include "fl/stl/strstream.h"
#include "fl/ui/audio.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/move.h"
#include "fl/stl/noexcept.h"

// ============================================================================
// CFastLED audio method implementations - thin trampolines to AudioManager
// ============================================================================

FL_MAYBE_UNUSED
fl::shared_ptr<fl::audio::Processor> CFastLED::add(const fl::audio::Config& config) FL_NO_EXCEPT {
	return fl::audio::AudioManager::instance().add(config);
}

FL_MAYBE_UNUSED
fl::shared_ptr<fl::audio::Processor> CFastLED::add(fl::shared_ptr<fl::audio::IInput> input) FL_NO_EXCEPT {
	return fl::audio::AudioManager::instance().add(fl::move(input));
}

FL_MAYBE_UNUSED
fl::shared_ptr<fl::audio::Processor> CFastLED::add(fl::UIAudio& uiAudio) FL_NO_EXCEPT {
	return fl::audio::AudioManager::instance().add(uiAudio);
}

FL_MAYBE_UNUSED
void CFastLED::remove(fl::shared_ptr<fl::audio::Processor> processor) FL_NO_EXCEPT {
	fl::audio::AudioManager::instance().remove(fl::move(processor));
}

namespace fl {

string &string::append(const audio::fft::Bins &str) FL_NO_EXCEPT {
    append("\n Impl Bins:\n  ");
    append(str.raw());
    append("\n");
    append(" Impl Bins DB:\n  ");
    append(str.db());
    append("\n");
    return *this;
}

sstream &sstream::operator<<(const audio::fft::Bins &bins) FL_NO_EXCEPT {
    mStr.append("Bins(bands=");
    mStr.append(bins.bands());
    mStr.append(", raw=");
    (*this) << bins.raw();
    mStr.append(", db=");
    (*this) << bins.db();
    mStr.append(")");
    return *this;
}

} // namespace fl
