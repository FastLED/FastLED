#pragma once

#include <stdint.h>
#include "namespace.h"
#include "crgb.h"
#include "fx/fx.h"
#include "ptr.h"
#include "fixed_vector.h"
#include <string.h>

#ifndef FASTLED_FX_ENGINE_MAX_FX
#define FASTLED_FX_ENGINE_MAX_FX 64
#endif

FASTLED_NAMESPACE_BEGIN

class Transition {
public:
    Transition() : mStart(0), mDuration(0), mNotStarted(true) {}
    ~Transition() {}

    uint8_t getProgress(uint32_t now) {
        if (mNotStarted) {
            return 0;
        }
        if (now < mStart) {
            return 0;
        } else if (now >= mStart + mDuration) {
            return 255;
        } else {
            return ((now - mStart) * 255) / mDuration;
        }
    }

    void start(uint32_t now, uint32_t duration) {
        mNotStarted = false;
        mStart = now;
        mDuration = duration;
    }

    bool isTransitioning(uint32_t now) {
        if (mNotStarted) {
            return false;
        }   
        return now >= mStart && now < mStart + mDuration;
    }

private:
    uint32_t mStart;
    uint32_t mDuration;
    bool mNotStarted;
};

class FxEngine {
public:
    FxEngine(uint16_t numLeds);
    ~FxEngine();

    void addFx(Fx* effect);
    //void startTransition(uint32_t now, uint32_t duration);
    void draw(uint32_t now, CRGB* outputBuffer);
    void nextFx(uint32_t now, uint32_t duration) {
        if (mIsTransitioning) {
            
            mCurrentIndex = mNextIndex;
            mIsTransitioning = false;
        }
        mNextIndex = (mCurrentIndex + 1) % mEffects.size();
        startTransition(now, duration);
    }

private:
    uint16_t mNumLeds;
    FixedVector<Fx*, FASTLED_FX_ENGINE_MAX_FX> mEffects;
    scoped_array<CRGB> mLayer1;
    scoped_array<CRGB> mLayer2;
    bool mIsTransitioning;
    uint16_t mCurrentIndex;
    uint16_t mNextIndex;
    Transition mTransition;
    void startTransition(uint32_t now, uint32_t duration);

};

inline FxEngine::FxEngine(uint16_t numLeds) 
    : mNumLeds(numLeds), mIsTransitioning(false), mCurrentIndex(0), mNextIndex(0) {
    mLayer1.reset(new CRGB[numLeds]);
    mLayer2.reset(new CRGB[numLeds]);
}

inline FxEngine::~FxEngine() {}

inline void FxEngine::addFx(Fx* effect) {
    mEffects.push_back(effect);
}

inline void FxEngine::startTransition(uint32_t now, uint32_t duration) {
    mIsTransitioning = true;
    mTransition.start(now, duration);
}

inline void FxEngine::draw(uint32_t now, CRGB* finalBuffer) {
    if (!mEffects.empty()) {
        mEffects[mCurrentIndex]->draw(now, mLayer1.get());
        
        if (mIsTransitioning && mEffects.size() > 1) {
            mEffects[mNextIndex]->draw(now, mLayer2.get());
            
            uint8_t progress = mTransition.getProgress(now);
            uint8_t inverse_progress = 255 - progress;

            for (uint16_t i = 0; i < mNumLeds; i++) {
                finalBuffer[i] = mLayer1[i].nscale8(inverse_progress) + mLayer2[i].nscale8(progress);
            }

            if (progress == 255) {
                // Transition complete, update current index
                mCurrentIndex = mNextIndex;
                mIsTransitioning = false;
            }
        } else {
            memcpy(finalBuffer, mLayer1.get(), sizeof(CRGB) * mNumLeds);
        }
    }
}

FASTLED_NAMESPACE_END

