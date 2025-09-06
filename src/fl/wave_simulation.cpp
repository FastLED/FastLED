// Based on works and code by Shawn Silverman.

#include "fl/stdint.h"

#include "fl/clamp.h"
#include "fl/namespace.h"
#include "fl/wave_simulation.h"
#include "fl/int.h"

namespace {

fl::u8 half_duplex_blend_sqrt_q15(fl::u16 x) {
    x = MIN(x, 32767); // Q15
    const int Q = 15;
    fl::u32 X = (fl::u32)x << Q; // promote to Q30
    fl::u32 y = (1u << Q);        // start at “1.0” in Q15

    // 3–4 iterations is plenty for 15‑bit precision:
    for (int i = 0; i < 4; i++) {
        y = (y + (X / y)) >> 1;
    }
    return static_cast<fl::i16>(y) >> 8;
}

fl::u8 half_duplex_blend_linear(fl::u16 x) {
    x = MIN(x, 32767); // Q15
    x *= 2;
    return x >> 8;
}

} // namespace

namespace fl {

void WaveSimulation2D::setSpeed(float speed) { mSim->setSpeed(speed); }

WaveSimulation2D::WaveSimulation2D(u32 W, u32 H, SuperSample factor,
                                   float speed, float dampening) {
    init(W, H, factor, speed, dampening);
}

void WaveSimulation2D::init(u32 width, u32 height, SuperSample factor,
                            float speed, int dampening) {
    mOuterWidth = width;
    mOuterHeight = height;
    mMultiplier = static_cast<u32>(factor);
    mSim.reset(); // clear out memory first.
    u32 w = width * mMultiplier;
    u32 h = height * mMultiplier;
    mSim.reset(new WaveSimulation2D_Real(w, h, speed, dampening));
    // Only allocate change grid if it's enabled (saves memory when disabled)
    if (mUseChangeGrid) {
        mChangeGrid.reset(w, h);
    }
    // Extra frames are needed because the simulation slows down in
    // proportion to the supersampling factor.
    mExtraFrames = u8(factor) - 1;
}

void WaveSimulation2D::setDampening(int damp) { mSim->setDampening(damp); }

int WaveSimulation2D::getDampenening() const { return mSim->getDampenening(); }

float WaveSimulation2D::getSpeed() const { return mSim->getSpeed(); }

float WaveSimulation2D::getf(fl::size x, fl::size y) const {
    if (!has(x, y))
        return 0.0f;
    float sum = 0.0f;
    for (u32 j = 0; j < mMultiplier; ++j) {
        for (u32 i = 0; i < mMultiplier; ++i) {
            sum += mSim->getf(x * mMultiplier + i, y * mMultiplier + j);
        }
    }
    return sum / static_cast<float>(mMultiplier * mMultiplier);
}

i16 WaveSimulation2D::geti16(fl::size x, fl::size y) const {
    if (!has(x, y))
        return 0;    
    i32 sum = 0;
    u8 mult = MAX(1, mMultiplier);
    for (u32 j = 0; j < mult; ++j) {
        for (u32 i = 0; i < mult; ++i) {
            u32 xx = x * mult + i;
            u32 yy = y * mult + j;
            i32 pt = mSim->geti16(xx, yy);
            if (mUseChangeGrid) {
                // i32 ch_pt = mChangeGrid[(yy * mMultiplier) + xx];
                i32 ch_pt = mChangeGrid(xx, yy);
                if (ch_pt != 0) { // we got a hit.
                    sum += ch_pt;
                } else {
                    sum += pt;
                }
            } else {
                sum += pt;
            }
        }
    }
    i16 out = static_cast<i16>(sum / (mult * mult));
    return out;
}

i16 WaveSimulation2D::geti16Previous(fl::size x, fl::size y) const {
    if (!has(x, y))
        return 0;
    i32 sum = 0;
    u8 mult = MAX(1, mMultiplier);
    for (u32 j = 0; j < mult; ++j) {
        for (u32 i = 0; i < mult; ++i) {
            sum +=
                mSim->geti16Previous(x * mult + i, y * mult + j);
        }
    }
    i16 out = static_cast<i16>(sum / (mult * mult));
    return out;
}

bool WaveSimulation2D::geti16All(fl::size x, fl::size y, i16 *curr,
                                 i16 *prev, i16 *diff) const {
    if (!has(x, y))
        return false;
    *curr = geti16(x, y);
    *prev = geti16Previous(x, y);
    *diff = *curr - *prev;
    return true;
}

i8 WaveSimulation2D::geti8(fl::size x, fl::size y) const {
    return static_cast<i8>(geti16(x, y) >> 8);
}

u8 WaveSimulation2D::getu8(fl::size x, fl::size y) const {
    i16 value = geti16(x, y);
    if (mSim->getHalfDuplex()) {
        u16 v2 = static_cast<u16>(value);
        switch (mU8Mode) {
        case WAVE_U8_MODE_LINEAR:
            return half_duplex_blend_linear(v2);
        case WAVE_U8_MODE_SQRT:
            return half_duplex_blend_sqrt_q15(v2);
        }
    }
    return static_cast<u8>(((static_cast<u16>(value) + 32768)) >> 8);
}

bool WaveSimulation2D::has(fl::size x, fl::size y) const {
    return (x < mOuterWidth) && (y < mOuterHeight);
}

void WaveSimulation2D::seti16(fl::size x, fl::size y, i16 v16) {
    if (!has(x, y))
        return;

    u8 mult = MAX(1, mMultiplier);

    // radius in pixels of your diamond
    int rad = static_cast<int>(mult) / 2;

    for (fl::size j = 0; j < mult; ++j) {
        for (fl::size i = 0; i < mult; ++i) {
            // compute offset from the center of this block
            int dx = static_cast<int>(i) - rad;
            int dy = static_cast<int>(j) - rad;
            // keep only those points whose Manhattan distance ≤ rad
            if (ABS(dx) + ABS(dy) > rad) {
                continue;
            }
            fl::size xx = x * mult + i;
            fl::size yy = y * mult + j;
            if (mSim->has(xx, yy)) {
                if (mUseChangeGrid) {
                    i16 &pt = mChangeGrid.at(xx, yy);
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
                            u16 abs_pt = static_cast<u16>(ABS(pt));
                            u16 abs_v16 = static_cast<u16>(ABS(v16));
                            if (abs_v16 > abs_pt) {
                                pt = v16;
                            }
                        }
                    }
                } else {
                    // Directly set the value in the simulation when change grid is disabled
                    mSim->seti16(xx, yy, v16);
                }
            }
        }
    }
}

void WaveSimulation2D::setf(fl::size x, fl::size y, float value) {
    if (!has(x, y))
        return;

    value = fl::clamp(value, 0.0f, 1.0f);
    i16 v16 = wave_detail::float_to_fixed(value);
    seti16(x, y, v16);
}

void WaveSimulation2D::update() {
    if (mUseChangeGrid) {
        const vec2<i16> min_max = mChangeGrid.minMax();
        const bool has_updates = min_max != vec2<i16>(0, 0);
        for (u8 i = 0; i < mExtraFrames + 1; ++i) {
            if (has_updates) {
                // apply them
                const u32 w = mChangeGrid.width();
                const u32 h = mChangeGrid.height();
                for (u32 x = 0; x < w; ++x) {
                    for (u32 y = 0; y < h; ++y) {
                        i16 v16 = mChangeGrid(x, y);
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
    } else {
        // When change grid is disabled, just run the simulation updates
        for (u8 i = 0; i < mExtraFrames + 1; ++i) {
            mSim->update();
        }
    }
}

u32 WaveSimulation2D::getWidth() const { return mOuterWidth; }
u32 WaveSimulation2D::getHeight() const { return mOuterHeight; }

void WaveSimulation2D::setExtraFrames(u8 extra) { mExtraFrames = extra; }

void WaveSimulation2D::setUseChangeGrid(bool enabled) {
    if (mUseChangeGrid == enabled) {
        return; // No change needed
    }
    
    mUseChangeGrid = enabled;
    
    if (mUseChangeGrid) {
        // Allocate change grid if enabling
        u32 w = mOuterWidth * mMultiplier;
        u32 h = mOuterHeight * mMultiplier;
        mChangeGrid.reset(w, h);
    } else {
        // Deallocate change grid if disabling (saves memory)
        mChangeGrid.reset(0, 0);
    }
}

WaveSimulation1D::WaveSimulation1D(u32 length, SuperSample factor,
                                   float speed, int dampening) {
    init(length, factor, speed, dampening);
}

void WaveSimulation1D::init(u32 length, SuperSample factor, float speed,
                            int dampening) {
    mOuterLength = length;
    mMultiplier = static_cast<u32>(factor);
    mSim.reset(); // clear out memory first.
    mSim.reset(
        new WaveSimulation1D_Real(length * mMultiplier, speed, dampening));
    // Extra updates (frames) are applied because the simulation slows down in
    // proportion to the supersampling factor.
    mExtraFrames = static_cast<u8>(factor) - 1;
}

void WaveSimulation1D::setSpeed(float speed) { mSim->setSpeed(speed); }

void WaveSimulation1D::setDampening(int damp) { mSim->setDampening(damp); }

int WaveSimulation1D::getDampenening() const { return mSim->getDampenening(); }

void WaveSimulation1D::setExtraFrames(u8 extra) { mExtraFrames = extra; }

float WaveSimulation1D::getSpeed() const { return mSim->getSpeed(); }

float WaveSimulation1D::getf(fl::size x) const {
    if (!has(x))
        return 0.0f;
    float sum = 0.0f;
    u8 mult = MAX(1, mMultiplier);
    for (u32 i = 0; i < mult; ++i) {
        sum += mSim->getf(x * mult + i);
    }
    return sum / static_cast<float>(mult);
}

i16 WaveSimulation1D::geti16(fl::size x) const {
    if (!has(x))
        return 0;
    u8 mult = MAX(1, mMultiplier);
    i32 sum = 0;
    for (u32 i = 0; i < mult; ++i) {
        sum += mSim->geti16(x * mult + i);
    }
    return static_cast<i16>(sum / mult);
}

i16 WaveSimulation1D::geti16Previous(fl::size x) const {
    if (!has(x))
        return 0;
    u8 mult = MAX(1, mMultiplier);
    i32 sum = 0;
    for (u32 i = 0; i < mult; ++i) {
        sum += mSim->geti16Previous(x * mult + i);
    }
    return static_cast<i16>(sum / mult);
}

bool WaveSimulation1D::geti16All(fl::size x, i16 *curr, i16 *prev,
                                 i16 *diff) const {
    if (!has(x))
        return false;
    *curr = geti16(x);
    *prev = geti16Previous(x);
    *diff = *curr - *prev;
    return true;
}

i8 WaveSimulation1D::geti8(fl::size x) const {
    return static_cast<i8>(geti16(x) >> 8);
}

// u8 WaveSimulation2D::getu8(fl::size x, fl::size y) const {
//     i16 value = geti16(x, y);
//     if (mSim->getHalfDuplex()) {
//         u16 v2 = static_cast<u16>(value);
//         switch (mU8Mode) {
//             case WAVE_U8_MODE_LINEAR:
//                 return half_duplex_blend_linear(v2);
//             case WAVE_U8_MODE_SQRT:
//                 return half_duplex_blend_sqrt_q15(v2);
//         }
//     }
//     return static_cast<u8>(((static_cast<u16>(value) + 32768)) >>
//     8);
// }

u8 WaveSimulation1D::getu8(fl::size x) const {
    i16 value = geti16(x);
    if (mSim->getHalfDuplex()) {
        u16 v2 = static_cast<u16>(value);
        switch (mU8Mode) {
        case WAVE_U8_MODE_LINEAR:
            return half_duplex_blend_linear(v2);
        case WAVE_U8_MODE_SQRT:
            return half_duplex_blend_sqrt_q15(v2);
        }
    }
    return static_cast<u8>(((static_cast<u16>(value) + 32768)) >> 8);
}

bool WaveSimulation1D::has(fl::size x) const { return (x < mOuterLength); }

void WaveSimulation1D::setf(fl::size x, float value) {
    if (!has(x))
        return;
    value = fl::clamp(value, -1.0f, 1.0f);
    u8 mult = MAX(1, mMultiplier);
    for (u32 i = 0; i < mult; ++i) {
        mSim->set(x * mult + i, value);
    }
}

void WaveSimulation1D::update() {
    mSim->update();
    for (u8 i = 0; i < mExtraFrames; ++i) {
        mSim->update();
    }
}

u32 WaveSimulation1D::getLength() const { return mOuterLength; }

} // namespace fl
