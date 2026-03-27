#include "fl/fx/fx_engine.h"
#include "fl/audio/audio_batch.h"
#include "fl/fx/video.h"
#include "fl/audio/audio_processor.h"
#include "fl/task/scheduler.h"
#include "fl/stl/noexcept.h"

namespace fl {

FxEngine::FxEngine(u16 numLeds, bool interpolate)
    : mTimeFunction(0), mCompositor(numLeds), mCurrId(0),
      mInterpolate(interpolate) {
    mEffects.reserve(FASTLED_FX_ENGINE_MAX_FX);
}

FxEngine::~FxEngine() FL_NOEXCEPT {}

void FxEngine::pushAudioFrame(const AudioFrame &frame) {
    mAudioBack.push_back(frame);
}

void FxEngine::setAudio(fl::shared_ptr<fl::audio::Processor> proc) {
    mAudioProcessor = fl::move(proc);
}

int FxEngine::addFx(FxPtr effect) {
    float fps = 0;
    if (mInterpolate && effect->hasFixedFrameRate(&fps)) {
        // Wrap the effect in a VideoFxWrapper so that we can get
        // interpolation.
        VideoFxWrapperPtr vid_fx = fl::make_shared<VideoFxWrapper>(effect);
        vid_fx->setFade(0, 0); // No fade for interpolated effects
        effect = vid_fx;
    }
    bool auto_set = mEffects.empty();
    bool ok = mEffects.insert(mCounter, effect);
    if (!ok) {
        return -1;
    }
    if (auto_set) {
        mCurrId = mCounter;
        mCompositor.startTransition(0, 0, effect);
    }
    return mCounter++;
}

bool FxEngine::nextFx(u16 duration) {
    bool ok = mEffects.next(mCurrId, &mCurrId, true);
    if (!ok) {
        return false;
    }
    setNextFx(mCurrId, duration);
    return true;
}

bool FxEngine::setNextFx(int index, u16 duration) {
    if (!mEffects.has(index)) {
        return false;
    }
    mCurrId = index;
    mDuration = duration;
    mDurationSet = true;
    return true;
}

FxPtr FxEngine::removeFx(int index) {
    if (!mEffects.has(index)) {
        return FxPtr();
    }

    FxPtr removedFx;
    bool ok = mEffects.get(index, &removedFx);
    if (!ok) {
        return FxPtr();
    }

    if (mCurrId == index) {
        // If we're removing the current effect, switch to the next one
        mEffects.next(mCurrId, &mCurrId, true);
        mDurationSet = true;
        mDuration = 0; // Instant transition
    }

    return removedFx;
}

FxPtr FxEngine::getFx(int id) {
    if (mEffects.has(id)) {
        FxPtr fx;
        mEffects.get(id, &fx);
        return fx;
    }
    return FxPtr();
}

bool FxEngine::draw(fl::u32 now, fl::span<CRGB> finalBuffer) {
    mTimeFunction.update(now);
    fl::u32 warpedTime = mTimeFunction.time();

    // Swap audio double-buffers: back → front, then clear back for next frame.
    mAudioFront.swap(mAudioBack);
    mAudioBack.clear();

    // Bump the task scheduler so the audio auto-pump task runs NOW,
    // feeding any pending I2S samples into the Processor before we read levels.
    // Without this, draw() would read stale levels from the previous frame.
    if (mAudioProcessor) {
        fl::task::Scheduler::instance().update();
    }

    // If an audio processor is wired, poll it for one frame per draw cycle.
    if (mAudioProcessor) {
        AudioFrame af;
        af.bass = mAudioProcessor->getBassLevel();
        af.mid = mAudioProcessor->getMidLevel();
        af.treble = mAudioProcessor->getTrebleLevel();
        af.volume = mAudioProcessor->getEnergy();
        af.beat = mAudioProcessor->isVibeBassSpike();
        af.timestamp = now;
        mAudioFront.push_back(af);
    }

    // Build AudioBatch on the stack — shared across both compositor layers.
    // Passes Processor pointer for lazy access to vibe/equalizer/percussion.
    fl::span<const AudioFrame> audioSpan(mAudioFront);
    AudioBatch audioBatch(audioSpan, mAudioProcessor.get());
    const AudioBatch *audioPtr =
        (mAudioFront.empty() && !mAudioProcessor) ? nullptr : &audioBatch;

    if (mEffects.empty()) {
        return false;
    }
    if (mDurationSet) {
        FxPtr fx;
        bool ok = mEffects.get(mCurrId, &fx);
        if (!ok) {
            // something went wrong.
            return false;
        }
        mCompositor.startTransition(now, mDuration, fx);
        mDurationSet = false;
    }
    if (!mEffects.empty()) {
        float speed = mTimeFunction.scale();
        mCompositor.draw(now, warpedTime, finalBuffer, speed, audioPtr);
    }
    return true;
}

} // namespace fl
