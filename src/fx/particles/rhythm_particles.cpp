/*  FastLED - Rhythmic Particle System (Implementation)
    ---------------------------------------------------------
    Audio-reactive particle system for music visualization.

    License: MIT (same spirit as FastLED)
*/

#include "rhythm_particles.h"

#if SKETCH_HAS_LOTS_OF_MEMORY

#include "fl/math.h"
#include "fl/memory.h"
#include "noise.h"
#include <math.h>
#include <stdlib.h>

namespace fl {

// ---------- Constructor / Destructor ----------

RhythmParticles::RhythmParticles(const RhythmParticlesConfig& cfg)
    : _cfg(cfg)
    , _maxParticles(0)
    , _particleCount(0)
    , _x(nullptr)
    , _y(nullptr)
    , _z(nullptr)
    , _vx(nullptr)
    , _vy(nullptr)
    , _vz(nullptr)
    , _h(nullptr)
    , _s(nullptr)
    , _v(nullptr)
    , _life(nullptr)
    , _maxLife(nullptr)
    , _kickDuckTime(0.0f)
    , _kickDuckLevel(0.0f)
    , _inFill(false)
    , _fillDensity(0.0f)
    , _noiseTime(0)
{
    // Initialize emitter configurations with sensible defaults
    _emitterKick.emit_rate = 15.0f;
    _emitterKick.velocity_min = 1.0f;
    _emitterKick.velocity_max = 3.0f;
    _emitterKick.life_min = 0.8f;
    _emitterKick.life_max = 1.5f;
    _emitterKick.color_base = CRGB(255, 50, 0);  // Orange-red
    _emitterKick.hue_variance = 20;
    _emitterKick.x = 0.5f;
    _emitterKick.y = 0.5f;

    _emitterSnare.emit_rate = 12.0f;
    _emitterSnare.velocity_min = 0.8f;
    _emitterSnare.velocity_max = 2.5f;
    _emitterSnare.life_min = 0.5f;
    _emitterSnare.life_max = 1.2f;
    _emitterSnare.color_base = CRGB(0, 150, 255);  // Cyan
    _emitterSnare.hue_variance = 30;
    _emitterSnare.x = 0.3f;
    _emitterSnare.y = 0.6f;

    _emitterHiHat.emit_rate = 8.0f;
    _emitterHiHat.velocity_min = 0.5f;
    _emitterHiHat.velocity_max = 1.8f;
    _emitterHiHat.life_min = 0.3f;
    _emitterHiHat.life_max = 0.8f;
    _emitterHiHat.color_base = CRGB(255, 255, 100);  // Yellow
    _emitterHiHat.hue_variance = 40;
    _emitterHiHat.x = 0.7f;
    _emitterHiHat.y = 0.4f;

    _emitterOverlay.emit_rate = 10.0f;
    _emitterOverlay.velocity_min = 0.7f;
    _emitterOverlay.velocity_max = 2.0f;
    _emitterOverlay.life_min = 0.6f;
    _emitterOverlay.life_max = 1.3f;
    _emitterOverlay.color_base = CRGB(200, 0, 255);  // Purple
    _emitterOverlay.hue_variance = 25;
    _emitterOverlay.x = 0.5f;
    _emitterOverlay.y = 0.8f;

    // Default palette (rainbow)
    _palette = RainbowColors_p;

    // Allocate particle arrays
    allocateParticles(cfg.max_particles);
}

RhythmParticles::~RhythmParticles() {
    deallocateParticles();
}

// ---------- Memory Management ----------

void RhythmParticles::allocateParticles(int max_particles) {
    if (max_particles == _maxParticles && _x != nullptr) {
        return;  // Already allocated
    }

    // Deallocate existing
    deallocateParticles();

    _maxParticles = max_particles;

    // Allocate SoA arrays
    _x = new float[max_particles];
    _y = new float[max_particles];
    _z = new float[max_particles];
    _vx = new float[max_particles];
    _vy = new float[max_particles];
    _vz = new float[max_particles];
    _h = new uint8_t[max_particles];
    _s = new uint8_t[max_particles];
    _v = new uint8_t[max_particles];
    _life = new float[max_particles];
    _maxLife = new float[max_particles];

    // Initialize all particles as dead
    for (int i = 0; i < max_particles; i++) {
        _life[i] = 0.0f;
    }

    _particleCount = 0;
}

void RhythmParticles::deallocateParticles() {
    delete[] _x;
    delete[] _y;
    delete[] _z;
    delete[] _vx;
    delete[] _vy;
    delete[] _vz;
    delete[] _h;
    delete[] _s;
    delete[] _v;
    delete[] _life;
    delete[] _maxLife;

    _x = _y = _z = nullptr;
    _vx = _vy = _vz = nullptr;
    _h = _s = _v = nullptr;
    _life = _maxLife = nullptr;

    _maxParticles = 0;
    _particleCount = 0;
}

// ---------- Event Handlers ----------

void RhythmParticles::onBeat(float /* phase4_4 */, float /* phase7_8 */) {
    // Beat events can trigger overlay emitter
    // For now, just a simple emission
}

void RhythmParticles::onSubdivision(int /* subdivision */, float /* swing_offset */) {
    // Subdivision events can trigger subtle particle releases
}

void RhythmParticles::onOnsetBass(float confidence, float /* timestamp_ms */) {
    // Emit kick particles
    int count = static_cast<int>(_emitterKick.emit_rate * confidence);
    emitParticles(_emitterKick, count, confidence);

    // Trigger kick duck
    _kickDuckTime = _cfg.kick_duck_duration_ms / 1000.0f;
    _kickDuckLevel = _cfg.kick_duck_amount;
}

void RhythmParticles::onOnsetMid(float confidence, float /* timestamp_ms */) {
    // Emit snare particles
    int count = static_cast<int>(_emitterSnare.emit_rate * confidence);
    emitParticles(_emitterSnare, count, confidence);
}

void RhythmParticles::onOnsetHigh(float confidence, float /* timestamp_ms */) {
    // Emit hi-hat particles
    int count = static_cast<int>(_emitterHiHat.emit_rate * confidence);
    emitParticles(_emitterHiHat, count, confidence);
}

void RhythmParticles::onFill(bool starting, float density) {
    _inFill = starting;
    _fillDensity = density;

    if (starting) {
        // Emit overlay particles during fill
        int count = static_cast<int>(_emitterOverlay.emit_rate * density);
        emitParticles(_emitterOverlay, count, density);
    }
}

// ---------- Particle Emission ----------

void RhythmParticles::emitParticles(const ParticleEmitterConfig& emitter, int count, float energy) {
    if (count <= 0) return;

    static uint32_t rng_state = 12345;  // Simple RNG state

    for (int i = 0; i < count; i++) {
        // Find dead particle slot
        int slot = -1;
        for (int j = 0; j < _maxParticles; j++) {
            if (_life[j] <= 0.0f) {
                slot = j;
                break;
            }
        }

        if (slot == -1) break;  // No free slots

        // Generate random values using simple LCG
        rng_state = rng_state * 1103515245 + 12345;
        float rand1 = (rng_state & 0xFFFF) / 65536.0f;
        rng_state = rng_state * 1103515245 + 12345;
        float rand2 = (rng_state & 0xFFFF) / 65536.0f;
        rng_state = rng_state * 1103515245 + 12345;
        float rand3 = (rng_state & 0xFFFF) / 65536.0f;
        rng_state = rng_state * 1103515245 + 12345;
        float rand4 = (rng_state & 0xFFFF) / 65536.0f;

        // Position (from emitter location with slight spread)
        _x[slot] = emitter.x * _cfg.width + (rand1 - 0.5f) * 0.5f;
        _y[slot] = emitter.y * _cfg.height + (rand2 - 0.5f) * 0.5f;
        _z[slot] = emitter.z * (_cfg.enable_3d ? 10.0f : 0.0f);

        // Velocity (random direction within spread angle)
        float angle = rand3 * emitter.spread_angle * (3.14159f / 180.0f);
        float speed = emitter.velocity_min + rand4 * (emitter.velocity_max - emitter.velocity_min);
        speed *= energy;  // Scale by energy

        _vx[slot] = ::cos(angle) * speed;
        _vy[slot] = ::sin(angle) * speed;
        _vz[slot] = (rand1 - 0.5f) * speed * 0.5f;

        // Color
        CRGB color = emitter.color_base;
        int hue_offset = static_cast<int>((rand2 - 0.5f) * emitter.hue_variance * 2.0f);
        CHSV hsv = rgb2hsv_approximate(color);
        uint8_t h = hsv.hue + hue_offset;
        uint8_t s = hsv.sat;
        uint8_t v = hsv.val;

        _h[slot] = h;
        _s[slot] = s;
        _v[slot] = v;

        // Lifetime
        float lifetime = emitter.life_min + rand3 * (emitter.life_max - emitter.life_min);
        _life[slot] = lifetime;
        _maxLife[slot] = lifetime;

        _particleCount++;
    }
}

// ---------- Physics Update ----------

void RhythmParticles::update(float dt) {
    if (dt <= 0.0f) {
        dt = _cfg.dt;
    }

    // Update noise field time
    _noiseTime += static_cast<uint32_t>(dt * 1000.0f);

    // Apply forces
    applyForces(dt);

    // Update lifetime
    updateLifetime(dt);

    // Cull dead particles
    cullDead();

    // Apply kick duck decay
    applyKickDuck(dt);
}

void RhythmParticles::applyForces(float dt) {
    float centerX = _cfg.width * 0.5f;
    float centerY = _cfg.height * 0.5f;

    for (int i = 0; i < _maxParticles; i++) {
        if (_life[i] <= 0.0f) continue;

        // Radial gravity (attract to or repel from center)
        if (_cfg.radial_gravity != 0.0f) {
            float dx = centerX - _x[i];
            float dy = centerY - _y[i];
            float dist = ::sqrt(dx * dx + dy * dy);
            if (dist > 0.001f) {
                float force = _cfg.radial_gravity / dist;
                _vx[i] += dx * force * dt;
                _vy[i] += dy * force * dt;
            }
        }

        // Curl noise flow field
        if (_cfg.curl_strength > 0.0f) {
            float curlX = getCurlNoiseX(_x[i], _y[i], _z[i]);
            float curlY = getCurlNoiseY(_x[i], _y[i], _z[i]);
            float curlZ = getCurlNoiseZ(_x[i], _y[i], _z[i]);

            _vx[i] += curlX * _cfg.curl_strength * dt;
            _vy[i] += curlY * _cfg.curl_strength * dt;
            if (_cfg.enable_3d) {
                _vz[i] += curlZ * _cfg.curl_strength * dt;
            }
        }

        // Apply velocity decay
        _vx[i] *= _cfg.velocity_decay;
        _vy[i] *= _cfg.velocity_decay;
        _vz[i] *= _cfg.velocity_decay;

        // Update position
        _x[i] += _vx[i] * dt;
        _y[i] += _vy[i] * dt;
        if (_cfg.enable_3d) {
            _z[i] += _vz[i] * dt;
        }

        // Wrap around boundaries
        while (_x[i] < 0) _x[i] += _cfg.width;
        while (_x[i] >= _cfg.width) _x[i] -= _cfg.width;
        while (_y[i] < 0) _y[i] += _cfg.height;
        while (_y[i] >= _cfg.height) _y[i] -= _cfg.height;
    }
}

void RhythmParticles::updateLifetime(float dt) {
    for (int i = 0; i < _maxParticles; i++) {
        if (_life[i] > 0.0f) {
            _life[i] -= dt;

            // Fade brightness based on remaining life
            if (_maxLife[i] > 0.0f) {
                float life_fraction = _life[i] / _maxLife[i];
                // Keep full brightness until 50% life, then fade
                if (life_fraction < 0.5f) {
                    _v[i] = static_cast<uint8_t>(_v[i] * (life_fraction * 2.0f));
                }
            }
        }
    }
}

void RhythmParticles::cullDead() {
    int active = 0;
    for (int i = 0; i < _maxParticles; i++) {
        if (_life[i] > 0.0f) {
            active++;
        }
    }
    _particleCount = active;
}

void RhythmParticles::applyKickDuck(float dt) {
    if (_kickDuckTime > 0.0f) {
        _kickDuckTime -= dt;
        if (_kickDuckTime < 0.0f) {
            _kickDuckTime = 0.0f;
            _kickDuckLevel = 0.0f;
        }
    }
}

// ---------- Curl Noise ----------

float RhythmParticles::getCurlNoiseX(float x, float y, float z) const {
    // Curl = (dNz/dy - dNy/dz, dNx/dz - dNz/dx, dNy/dx - dNx/dy)
    // For 2D, we simplify: curlX = dNz/dy
    float epsilon = 0.01f;
    uint32_t scale = 100;  // Noise scale

    int16_t n1 = inoise16_raw(static_cast<uint32_t>(x * scale),
                               static_cast<uint32_t>((y + epsilon) * scale),
                               static_cast<uint32_t>(z * scale),
                               _noiseTime);
    int16_t n2 = inoise16_raw(static_cast<uint32_t>(x * scale),
                               static_cast<uint32_t>((y - epsilon) * scale),
                               static_cast<uint32_t>(z * scale),
                               _noiseTime);

    return static_cast<float>(n1 - n2) / 32768.0f;
}

float RhythmParticles::getCurlNoiseY(float x, float y, float z) const {
    // curlY = -dNz/dx
    float epsilon = 0.01f;
    uint32_t scale = 100;

    int16_t n1 = inoise16_raw(static_cast<uint32_t>((x + epsilon) * scale),
                               static_cast<uint32_t>(y * scale),
                               static_cast<uint32_t>(z * scale),
                               _noiseTime);
    int16_t n2 = inoise16_raw(static_cast<uint32_t>((x - epsilon) * scale),
                               static_cast<uint32_t>(y * scale),
                               static_cast<uint32_t>(z * scale),
                               _noiseTime);

    return -static_cast<float>(n1 - n2) / 32768.0f;
}

float RhythmParticles::getCurlNoiseZ(float x, float y, float /* z */) const {
    // curlZ = dNy/dx - dNx/dy
    float epsilon = 0.01f;
    uint32_t scale = 100;

    int16_t ny1 = inoise16_raw(static_cast<uint32_t>((x + epsilon) * scale),
                                static_cast<uint32_t>(y * scale),
                                _noiseTime, 1000);
    int16_t ny2 = inoise16_raw(static_cast<uint32_t>((x - epsilon) * scale),
                                static_cast<uint32_t>(y * scale),
                                _noiseTime, 1000);

    int16_t nx1 = inoise16_raw(static_cast<uint32_t>(x * scale),
                                static_cast<uint32_t>((y + epsilon) * scale),
                                _noiseTime, 2000);
    int16_t nx2 = inoise16_raw(static_cast<uint32_t>(x * scale),
                                static_cast<uint32_t>((y - epsilon) * scale),
                                _noiseTime, 2000);

    float dNy_dx = static_cast<float>(ny1 - ny2) / 32768.0f;
    float dNx_dy = static_cast<float>(nx1 - nx2) / 32768.0f;

    return dNy_dx - dNx_dy;
}

// ---------- Rendering ----------

void RhythmParticles::render(CRGB* leds, int num_leds) {
    if (!leds || num_leds <= 0) return;

    // Map particles to LED strip
    for (int i = 0; i < _maxParticles; i++) {
        if (_life[i] <= 0.0f) continue;

        // Map 2D position to 1D LED index
        // Simple mapping: row-major order
        int px = static_cast<int>(_x[i]);
        int py = static_cast<int>(_y[i]);

        // Clamp to bounds
        if (px < 0 || px >= _cfg.width || py < 0 || py >= _cfg.height) continue;

        int led_index = py * _cfg.width + px;
        if (led_index >= num_leds) continue;

        // Convert HSV to RGB
        CRGB color = CHSV(_h[i], _s[i], _v[i]);

        // Apply kick duck
        if (_kickDuckLevel > 0.0f) {
            color.nscale8(static_cast<uint8_t>(255.0f * (1.0f - _kickDuckLevel)));
        }

        // Additive blend
        leds[led_index] += color;
    }

    // Apply bloom if enabled
    if (_cfg.bloom_threshold > 0) {
        for (int i = 0; i < num_leds; i++) {
            uint8_t maxComponent = MAX(leds[i].r, MAX(leds[i].g, leds[i].b));
            if (maxComponent > _cfg.bloom_threshold) {
                // Simple 1-tap bloom: brighten neighbors
                if (i > 0) {
                    leds[i - 1] += leds[i].scale8(static_cast<uint8_t>(_cfg.bloom_strength * 255));
                }
                if (i < num_leds - 1) {
                    leds[i + 1] += leds[i].scale8(static_cast<uint8_t>(_cfg.bloom_strength * 255));
                }
            }
        }
    }
}

// ---------- Configuration ----------

void RhythmParticles::setEmitterConfig(EmitterType type, const ParticleEmitterConfig& cfg) {
    switch (type) {
        case EmitterType::Kick:
            _emitterKick = cfg;
            break;
        case EmitterType::Snare:
            _emitterSnare = cfg;
            break;
        case EmitterType::HiHat:
            _emitterHiHat = cfg;
            break;
        case EmitterType::Overlay:
            _emitterOverlay = cfg;
            break;
        case EmitterType::Custom:
            // Custom emitter type not yet implemented
            break;
    }
}

const ParticleEmitterConfig& RhythmParticles::getEmitterConfig(EmitterType type) const {
    switch (type) {
        case EmitterType::Kick:
            return _emitterKick;
        case EmitterType::Snare:
            return _emitterSnare;
        case EmitterType::HiHat:
            return _emitterHiHat;
        case EmitterType::Overlay:
            return _emitterOverlay;
        case EmitterType::Custom:
            // Custom emitter type not yet implemented, return kick as fallback
            return _emitterKick;
    }
}

void RhythmParticles::setPalette(const CRGBPalette16& palette) {
    _palette = palette;
}

void RhythmParticles::setConfig(const RhythmParticlesConfig& cfg) {
    bool need_realloc = (cfg.max_particles != _cfg.max_particles);
    _cfg = cfg;

    if (need_realloc) {
        allocateParticles(cfg.max_particles);
    }
}

void RhythmParticles::reset() {
    // Kill all particles
    for (int i = 0; i < _maxParticles; i++) {
        _life[i] = 0.0f;
    }
    _particleCount = 0;
    _kickDuckTime = 0.0f;
    _kickDuckLevel = 0.0f;
    _inFill = false;
    _fillDensity = 0.0f;
}

} // namespace fl

#endif // SKETCH_HAS_LOTS_OF_MEMORY
