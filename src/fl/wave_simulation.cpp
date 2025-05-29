// Based on works and code by Shawn Silverman.

#include <stdint.h>

#include "fl/clamp.h"
#include "fl/namespace.h"
#include "fl/wave_simulation.h"

namespace {

uint8_t half_duplex_blend_sqrt_q15(uint16_t x) {
    x = MIN(x, 32767); // Q15
    const int Q = 15;
    uint32_t X = (uint32_t)x << Q; // promote to Q30
    uint32_t y = (1u << Q);        // start at “1.0” in Q15

    // 3–4 iterations is plenty for 15‑bit precision:
    for (int i = 0; i < 4; i++) {
        y = (y + (X / y)) >> 1;
    }
    return static_cast<int16_t>(y) >> 8;
}

uint8_t half_duplex_blend_linear(uint16_t x) {
    x = MIN(x, 32767); // Q15
    x *= 2;
    return x >> 8;
}

} // namespace

namespace fl {

void WaveSimulation2D::setSpeed(float speed) { mSim->setSpeed(speed); }

WaveSimulation2D::WaveSimulation2D(uint32_t W, uint32_t H, SuperSample factor,
                                   float speed, float dampening) {
    init(W, H, factor, speed, dampening);
}

void WaveSimulation2D::init(uint32_t width, uint32_t height, SuperSample factor,
                            float speed, int dampening) {
    mOuterWidth = width;
    mOuterHeight = height;
    mMultiplier = static_cast<uint32_t>(factor);
    mSim.reset(); // clear out memory first.
    uint32_t w = width * mMultiplier;
    uint32_t h = height * mMultiplier;
    mSim.reset(new WaveSimulation2D_Real(w, h, speed, dampening));
    mChangeGrid.reset(w, h);
    // Extra frames are needed because the simulation slows down in
    // proportion to the supersampling factor.
    mExtraFrames = uint8_t(factor) - 1;
}

void WaveSimulation2D::setDampening(int damp) { mSim->setDampening(damp); }

int WaveSimulation2D::getDampenening() const { return mSim->getDampenening(); }

float WaveSimulation2D::getSpeed() const { return mSim->getSpeed(); }

float WaveSimulation2D::getf(size_t x, size_t y) const {
    if (!has(x, y))
        return 0.0f;
    float sum = 0.0f;
    for (uint32_t j = 0; j < mMultiplier; ++j) {
        for (uint32_t i = 0; i < mMultiplier; ++i) {
            sum += mSim->getf(x * mMultiplier + i, y * mMultiplier + j);
        }
    }
    return sum / static_cast<float>(mMultiplier * mMultiplier);
}

int16_t WaveSimulation2D::geti16(size_t x, size_t y) const {
    if (!has(x, y))
        return 0;    
    int32_t sum = 0;
    uint8_t mult = MAX(1, mMultiplier);
    for (uint32_t j = 0; j < mult; ++j) {
        for (uint32_t i = 0; i < mult; ++i) {
            uint32_t xx = x * mult + i;
            uint32_t yy = y * mult + j;
            int32_t pt = mSim->geti16(xx, yy);
            // int32_t ch_pt = mChangeGrid[(yy * mMultiplier) + xx];
            int32_t ch_pt = mChangeGrid(xx, yy);
            if (ch_pt != 0) { // we got a hit.
                sum += ch_pt;
            } else {
                sum += pt;
            }
        }
    }
    int16_t out = static_cast<int16_t>(sum / (mult * mult));
    return out;
}

int16_t WaveSimulation2D::geti16Previous(size_t x, size_t y) const {
    if (!has(x, y))
        return 0;
    int32_t sum = 0;
    uint8_t mult = MAX(1, mMultiplier);
    for (uint32_t j = 0; j < mult; ++j) {
        for (uint32_t i = 0; i < mult; ++i) {
            sum +=
                mSim->geti16Previous(x * mult + i, y * mult + j);
        }
    }
    int16_t out = static_cast<int16_t>(sum / (mult * mult));
    return out;
}

bool WaveSimulation2D::geti16All(size_t x, size_t y, int16_t *curr,
                                 int16_t *prev, int16_t *diff) const {
    if (!has(x, y))
        return false;
    *curr = geti16(x, y);
    *prev = geti16Previous(x, y);
    *diff = *curr - *prev;
    return true;
}

int8_t WaveSimulation2D::geti8(size_t x, size_t y) const {
    return static_cast<int8_t>(geti16(x, y) >> 8);
}

uint8_t WaveSimulation2D::getu8(size_t x, size_t y) const {
    int16_t value = geti16(x, y);
    if (mSim->getHalfDuplex()) {
        uint16_t v2 = static_cast<uint16_t>(value);
        switch (mU8Mode) {
        case WAVE_U8_MODE_LINEAR:
            return half_duplex_blend_linear(v2);
        case WAVE_U8_MODE_SQRT:
            return half_duplex_blend_sqrt_q15(v2);
        }
    }
    return static_cast<uint8_t>(((static_cast<uint16_t>(value) + 32768)) >> 8);
}

bool WaveSimulation2D::has(size_t x, size_t y) const {
    return (x < mOuterWidth) && (y < mOuterHeight);
}

void WaveSimulation2D::seti16(size_t x, size_t y, int16_t v16) {
    if (!has(x, y))
        return;

    uint8_t mult = MAX(1, mMultiplier);

    // radius in pixels of your diamond
    int rad = static_cast<int>(mult) / 2;

    for (size_t j = 0; j < mult; ++j) {
        for (size_t i = 0; i < mult; ++i) {
            // compute offset from the center of this block
            int dx = static_cast<int>(i) - rad;
            int dy = static_cast<int>(j) - rad;
            // keep only those points whose Manhattan distance ≤ rad
            if (ABS(dx) + ABS(dy) > rad) {
                continue;
            }
            size_t xx = x * mult + i;
            size_t yy = y * mult + j;
            if (mSim->has(xx, yy)) {
                int16_t &pt = mChangeGrid.at(xx, yy);
                if (pt == 0) {
                    // not set yet so set unconditionally.
                    pt = v16;
                } else {
                    const bool sign_matches = (pt >= 0) == (v16 >= 0);
                    if (!sign_matches) {
                        // opposite signs, so overwrite
                        pt = v16;
                    } else {
                        // if the magnitude of the new pt is greater than what
                        // was already there, then overwrite.
                        uint16_t abs_pt = static_cast<uint16_t>(ABS(pt));
                        uint16_t abs_v16 = static_cast<uint16_t>(ABS(v16));
                        if (abs_v16 > abs_pt) {
                            pt = v16;
                        }
                    }
                }
            }
        }
    }
}

void WaveSimulation2D::setf(size_t x, size_t y, float value) {
    if (!has(x, y))
        return;

    value = fl::clamp(value, 0.0f, 1.0f);
    int16_t v16 = wave_detail::float_to_fixed(value);
    seti16(x, y, v16);
}

void WaveSimulation2D::update() {
    const vec2<int16_t> min_max = mChangeGrid.minMax();
    const bool has_updates = min_max != vec2<int16_t>(0, 0);
    for (uint8_t i = 0; i < mExtraFrames + 1; ++i) {
        if (has_updates) {
            // apply them
            const uint32_t w = mChangeGrid.width();
            const uint32_t h = mChangeGrid.height();
            for (uint32_t x = 0; x < w; ++x) {
                for (uint32_t y = 0; y < h; ++y) {
                    int16_t v16 = mChangeGrid(x, y);
                    if (v16 != 0) {
                        mSim->seti16(x, y, v16);
                    }
                }
            }
        }
        mSim->update();
    }
    // zero out mChangeGrid
    mChangeGrid.clear();
}

uint32_t WaveSimulation2D::getWidth() const { return mOuterWidth; }
uint32_t WaveSimulation2D::getHeight() const { return mOuterHeight; }

void WaveSimulation2D::setExtraFrames(uint8_t extra) { mExtraFrames = extra; }

WaveSimulation1D::WaveSimulation1D(uint32_t length, SuperSample factor,
                                   float speed, int dampening) {
    init(length, factor, speed, dampening);
}

void WaveSimulation1D::init(uint32_t length, SuperSample factor, float speed,
                            int dampening) {
    mOuterLength = length;
    mMultiplier = static_cast<uint32_t>(factor);
    mSim.reset(); // clear out memory first.
    mSim.reset(
        new WaveSimulation1D_Real(length * mMultiplier, speed, dampening));
    // Extra updates (frames) are applied because the simulation slows down in
    // proportion to the supersampling factor.
    mExtraFrames = static_cast<uint8_t>(factor) - 1;
}

void WaveSimulation1D::setSpeed(float speed) { mSim->setSpeed(speed); }

void WaveSimulation1D::setDampening(int damp) { mSim->setDampening(damp); }

int WaveSimulation1D::getDampenening() const { return mSim->getDampenening(); }

void WaveSimulation1D::setExtraFrames(uint8_t extra) { mExtraFrames = extra; }

float WaveSimulation1D::getSpeed() const { return mSim->getSpeed(); }

float WaveSimulation1D::getf(size_t x) const {
    if (!has(x))
        return 0.0f;
    float sum = 0.0f;
    uint8_t mult = MAX(1, mMultiplier);
    for (uint32_t i = 0; i < mult; ++i) {
        sum += mSim->getf(x * mult + i);
    }
    return sum / static_cast<float>(mult);
}

int16_t WaveSimulation1D::geti16(size_t x) const {
    if (!has(x))
        return 0;
    uint8_t mult = MAX(1, mMultiplier);
    int32_t sum = 0;
    for (uint32_t i = 0; i < mult; ++i) {
        sum += mSim->geti16(x * mult + i);
    }
    return static_cast<int16_t>(sum / mult);
}

int16_t WaveSimulation1D::geti16Previous(size_t x) const {
    if (!has(x))
        return 0;
    uint8_t mult = MAX(1, mMultiplier);
    int32_t sum = 0;
    for (uint32_t i = 0; i < mult; ++i) {
        sum += mSim->geti16Previous(x * mult + i);
    }
    return static_cast<int16_t>(sum / mult);
}

bool WaveSimulation1D::geti16All(size_t x, int16_t *curr, int16_t *prev,
                                 int16_t *diff) const {
    if (!has(x))
        return false;
    *curr = geti16(x);
    *prev = geti16Previous(x);
    *diff = *curr - *prev;
    return true;
}

int8_t WaveSimulation1D::geti8(size_t x) const {
    return static_cast<int8_t>(geti16(x) >> 8);
}

// uint8_t WaveSimulation2D::getu8(size_t x, size_t y) const {
//     int16_t value = geti16(x, y);
//     if (mSim->getHalfDuplex()) {
//         uint16_t v2 = static_cast<uint16_t>(value);
//         switch (mU8Mode) {
//             case WAVE_U8_MODE_LINEAR:
//                 return half_duplex_blend_linear(v2);
//             case WAVE_U8_MODE_SQRT:
//                 return half_duplex_blend_sqrt_q15(v2);
//         }
//     }
//     return static_cast<uint8_t>(((static_cast<uint16_t>(value) + 32768)) >>
//     8);
// }

uint8_t WaveSimulation1D::getu8(size_t x) const {
    int16_t value = geti16(x);
    if (mSim->getHalfDuplex()) {
        uint16_t v2 = static_cast<uint16_t>(value);
        switch (mU8Mode) {
        case WAVE_U8_MODE_LINEAR:
            return half_duplex_blend_linear(v2);
        case WAVE_U8_MODE_SQRT:
            return half_duplex_blend_sqrt_q15(v2);
        }
    }
    return static_cast<uint8_t>(((static_cast<uint16_t>(value) + 32768)) >> 8);
}

bool WaveSimulation1D::has(size_t x) const { return (x < mOuterLength); }

void WaveSimulation1D::setf(size_t x, float value) {
    if (!has(x))
        return;
    value = fl::clamp(value, -1.0f, 1.0f);
    uint8_t mult = MAX(1, mMultiplier);
    for (uint32_t i = 0; i < mult; ++i) {
        mSim->set(x * mult + i, value);
    }
}

void WaveSimulation1D::update() {
    mSim->update();
    for (uint8_t i = 0; i < mExtraFrames; ++i) {
        mSim->update();
    }
}

uint32_t WaveSimulation1D::getLength() const { return mOuterLength; }

} // namespace fl
