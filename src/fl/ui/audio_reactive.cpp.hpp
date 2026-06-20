#include "fl/ui/audio_reactive.h"
#include "fl/stl/noexcept.h"

namespace fl {

UIAudioReactive::UIAudioReactive(const fl::string& name) FL_NO_EXCEPT
    : mImpl(name) {
    audio::ReactiveConfig config;
    config.sampleRate = AUDIO_DEFAULT_SAMPLE_RATE;
    mReactive.begin(config);
}

UIAudioReactive::UIAudioReactive(const fl::string& name, const fl::url& url) FL_NO_EXCEPT
    : mImpl(name, url) {
    audio::ReactiveConfig config;
    config.sampleRate = AUDIO_DEFAULT_SAMPLE_RATE;
    mReactive.begin(config);
}

UIAudioReactive::UIAudioReactive(const fl::string& name, const fl::asset_ref& asset) FL_NO_EXCEPT
    : mImpl(name, fl::resolve_asset(asset)) {
    audio::ReactiveConfig config;
    config.sampleRate = AUDIO_DEFAULT_SAMPLE_RATE;
    mReactive.begin(config);
}

UIAudioReactive::UIAudioReactive(const fl::string& name, const fl::audio::Config& config) FL_NO_EXCEPT
    : mImpl(name, config), mConfig(config) {
    audio::ReactiveConfig rConfig;
    rConfig.sampleRate = AUDIO_DEFAULT_SAMPLE_RATE;
    if (const audio::ConfigI2S* i2s = config.ptr<audio::ConfigI2S>()) {
        rConfig.sampleRate = i2s->mSampleRate;
    } else if (const audio::ConfigPdm* pdm = config.ptr<audio::ConfigPdm>()) {
        rConfig.sampleRate = pdm->mSampleRate;
    }
    mReactive.begin(rConfig);
}

UIAudioReactive::~UIAudioReactive() FL_NO_EXCEPT {}

void UIAudioReactive::update() FL_NO_EXCEPT {
    while (mImpl.hasNext()) {
        audio::Sample sample = mImpl.next();
        if (!sample.isValid()) {
            break;
        }
        mReactive.processSample(sample);
    }
}

float UIAudioReactive::getVolume() FL_NO_EXCEPT {
    update();
    return mReactive.getVolume();
}

float UIAudioReactive::getBass() FL_NO_EXCEPT {
    update();
    return mReactive.getBass();
}

float UIAudioReactive::getMid() FL_NO_EXCEPT {
    update();
    return mReactive.getMid();
}

float UIAudioReactive::getTreble() FL_NO_EXCEPT {
    update();
    return mReactive.getTreble();
}

float UIAudioReactive::getBand(fl::u8 band) FL_NO_EXCEPT {
    update();
    if (band >= kNumBands) {
        return 0.0f;
    }
    return mReactive.getData().frequencyBins[band];
}

bool UIAudioReactive::isBeat() FL_NO_EXCEPT {
    update();
    return mReactive.isBeat();
}

void UIAudioReactive::setGroup(const fl::string& groupName) FL_NO_EXCEPT {
    UIElement::setGroup(groupName);
    mImpl.setGroup(groupName);
}

}
