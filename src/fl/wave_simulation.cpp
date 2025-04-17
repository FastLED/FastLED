// Based on works and code by Shawn Silverman.

#include <stdint.h>

#include "fl/namespace.h"
#include "fl/wave_simulation.h"


namespace {

uint8_t half_duplex_blend_sqrt_q15(uint16_t x) {
    x = MIN(x, 32767);  // Q15
    const int   Q = 15;
    uint32_t    X = (uint32_t)x << Q;   // promote to Q30
    uint32_t    y = (1u << Q);          // start at “1.0” in Q15

    // 3–4 iterations is plenty for 15‑bit precision:
    for (int i = 0; i < 4; i++) {
        y = ( y + (X / y) ) >> 1;
    }
    return static_cast<int16_t>(y) >> 8;
}

uint8_t half_duplex_blend_linear(uint16_t x) {
    x = MIN(x, 32767);  // Q15
    x *= 2;
    return x >> 8;
}

}  // namespace


namespace fl {


void WaveSimulation2D::setSpeed(float speed) { sim->setSpeed(speed); }

WaveSimulation2D::WaveSimulation2D(uint32_t W, uint32_t H, SuperSample factor,
                                   float speed, float dampening) {
    init(W, H, factor, speed, dampening);
}

void WaveSimulation2D::init(uint32_t width, uint32_t height, SuperSample factor, float speed, int dampening) {
    outerWidth = width;
    outerHeight = height;
    multiplier = static_cast<uint32_t>(factor);
    sim.reset(new WaveSimulation2D_Real(width * multiplier, height * multiplier, speed, dampening));
    // Extra frames are needed because the simulation slows down in
    // proportion to the supersampling factor.
    extraFrames = uint8_t(factor) - 1;
}

void WaveSimulation2D::setDampening(int damp) { sim->setDampening(damp); }

int WaveSimulation2D::getDampenening() const { return sim->getDampenening(); }

float WaveSimulation2D::getSpeed() const { return sim->getSpeed(); }

float WaveSimulation2D::getf(size_t x, size_t y) const {
    if (!has(x, y))
        return 0.0f;
    float sum = 0.0f;
    for (uint32_t j = 0; j < multiplier; ++j) {
        for (uint32_t i = 0; i < multiplier; ++i) {
            sum += sim->getf(x * multiplier + i, y * multiplier + j);
        }
    }
    return sum / static_cast<float>(multiplier * multiplier);
}

int16_t WaveSimulation2D::geti16(size_t x, size_t y) const {
    if (!has(x, y))
        return 0;
    int32_t sum = 0;
    for (uint32_t j = 0; j < multiplier; ++j) {
        for (uint32_t i = 0; i < multiplier; ++i) {
            sum += sim->geti16(x * multiplier + i, y * multiplier + j);
        }
    }
    return static_cast<int16_t>(sum / (multiplier * multiplier));
}

int16_t WaveSimulation2D::geti16Previous(size_t x, size_t y) const {
    if (!has(x, y))
        return 0;
    int32_t sum = 0;
    for (uint32_t j = 0; j < multiplier; ++j) {
        for (uint32_t i = 0; i < multiplier; ++i) {
            sum += sim->geti16Previous(x * multiplier + i, y * multiplier + j);
        }
    }
    return static_cast<int16_t>(sum / (multiplier * multiplier));
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
    if (sim->getHalfDuplex()) {
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
    return (x < outerWidth) && (y < outerHeight);
}

void WaveSimulation2D::seti16(size_t x, size_t y, int16_t v16) {
    if (!has(x, y))
        return;

    // radius in pixels of your diamond
    int rad = static_cast<int>(multiplier) / 2;

    for (size_t j = 0; j < multiplier; ++j) {
        for (size_t i = 0; i < multiplier; ++i) {
            // compute offset from the center of this block
            int dx = static_cast<int>(i) - rad;
            int dy = static_cast<int>(j) - rad;

            // keep only those points whose Manhattan distance ≤ rad
            if (ABS(dx) + ABS(dy) > rad)
                continue;

            size_t xx = x * multiplier + i;
            size_t yy = y * multiplier + j;
            if (sim->has(xx, yy)) {
                sim->seti16(xx, yy, v16);
            }
        }
    }
}

void WaveSimulation2D::setf(size_t x, size_t y, float value) {
    if (!has(x, y))
        return;

    int16_t v16 = wave_detail::float_to_fixed(value);
    seti16(x, y, v16);
}

void WaveSimulation2D::update() {
    sim->update();
    for (uint8_t i = 0; i < extraFrames; ++i) {
        sim->update();
    }
}

uint32_t WaveSimulation2D::getWidth() const { return outerWidth; }
uint32_t WaveSimulation2D::getHeight() const { return outerHeight; }

void WaveSimulation2D::setExtraFrames(uint8_t extra) { extraFrames = extra; }

WaveSimulation1D::WaveSimulation1D(uint32_t length, SuperSample factor,
                                   float speed, int dampening) {
    init(length, factor, speed, dampening);
}

void WaveSimulation1D::init(uint32_t length, SuperSample factor,
                            float speed, int dampening) {
    outerLength = length;
    multiplier = static_cast<uint32_t>(factor);
    sim.reset(new WaveSimulation1D_Real(length * multiplier, speed, dampening));
    // Extra updates (frames) are applied because the simulation slows down in
    // proportion to the supersampling factor.
    extraFrames = static_cast<uint8_t>(factor) - 1;
}

void WaveSimulation1D::setSpeed(float speed) { sim->setSpeed(speed); }

void WaveSimulation1D::setDampening(int damp) { sim->setDampening(damp); }

int WaveSimulation1D::getDampenening() const { return sim->getDampenening(); }

void WaveSimulation1D::setExtraFrames(uint8_t extra) { extraFrames = extra; }

float WaveSimulation1D::getSpeed() const { return sim->getSpeed(); }

float WaveSimulation1D::getf(size_t x) const {
    if (!has(x))
        return 0.0f;
    float sum = 0.0f;
    for (uint32_t i = 0; i < multiplier; ++i) {
        sum += sim->getf(x * multiplier + i);
    }
    return sum / static_cast<float>(multiplier);
}

int16_t WaveSimulation1D::geti16(size_t x) const {
    if (!has(x))
        return 0;
    int32_t sum = 0;
    for (uint32_t i = 0; i < multiplier; ++i) {
        sum += sim->geti16(x * multiplier + i);
    }
    return static_cast<int16_t>(sum / multiplier);
}

int16_t WaveSimulation1D::geti16Previous(size_t x) const {
    if (!has(x))
        return 0;
    int32_t sum = 0;
    for (uint32_t i = 0; i < multiplier; ++i) {
        sum += sim->geti16Previous(x * multiplier + i);
    }
    return static_cast<int16_t>(sum / multiplier);
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
//     if (sim->getHalfDuplex()) {
//         uint16_t v2 = static_cast<uint16_t>(value);
//         switch (mU8Mode) {
//             case WAVE_U8_MODE_LINEAR:
//                 return half_duplex_blend_linear(v2);
//             case WAVE_U8_MODE_SQRT:
//                 return half_duplex_blend_sqrt_q15(v2);
//         }
//     }
//     return static_cast<uint8_t>(((static_cast<uint16_t>(value) + 32768)) >> 8);
// }

uint8_t WaveSimulation1D::getu8(size_t x) const {
    int16_t value = geti16(x);
    if (sim->getHalfDuplex()) {
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

bool WaveSimulation1D::has(size_t x) const { return (x < outerLength); }

void WaveSimulation1D::setf(size_t x, float value) {
    if (!has(x))
        return;
    for (uint32_t i = 0; i < multiplier; ++i) {
        sim->set(x * multiplier + i, value);
    }
}

void WaveSimulation1D::update() {
    sim->update();
    for (uint8_t i = 0; i < extraFrames; ++i) {
        sim->update();
    }
}

uint32_t WaveSimulation1D::getLength() const { return outerLength; }

} // namespace fl
