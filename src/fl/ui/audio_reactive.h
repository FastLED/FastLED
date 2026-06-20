#pragma once

#include "fl/asset/asset.h"
#include "fl/audio/audio.h"
#include "fl/audio/audio_input.h"
#include "fl/audio/audio_reactive.h"
#include "fl/stl/optional.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"
#include "fl/stl/url.h"
#include "fl/ui/element.h"
#include "fl/ui/audio.h"
#include "fl/stl/noexcept.h"

namespace fl {

class UIAudioReactive : public UIElement {
  public:
    FL_NO_COPY(UIAudioReactive)
    UIAudioReactive(const fl::string& name) FL_NO_EXCEPT;
    UIAudioReactive(const fl::string& name, const fl::url& url) FL_NO_EXCEPT;
    UIAudioReactive(const fl::string& name, const fl::asset_ref& asset) FL_NO_EXCEPT;
    UIAudioReactive(const fl::string& name, const fl::audio::Config& config) FL_NO_EXCEPT;
    ~UIAudioReactive() FL_NO_EXCEPT;

    float getVolume() FL_NO_EXCEPT;
    float getBass() FL_NO_EXCEPT;
    float getMid() FL_NO_EXCEPT;
    float getTreble() FL_NO_EXCEPT;
    float getBand(fl::u8 band) FL_NO_EXCEPT;
    bool isBeat() FL_NO_EXCEPT;

    static constexpr fl::u8 kNumBands = 16;

    void setGroup(const fl::string& groupName) FL_NO_EXCEPT override;

  protected:
    UIAudioImpl mImpl;
    fl::audio::Reactive mReactive;
    fl::optional<audio::Config> mConfig;

  private:
    void update() FL_NO_EXCEPT;
};

}
