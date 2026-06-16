/*
Wave-PDE-specific performance bench helpers. The wave simulator owns
this code; AutoResearch (or any other RPC layer) is an *external
binder* that calls into these helpers and translates results into its
preferred wire format. The simulator side does not know about RPC
topology, JSON, or any specific transport.

Used by meta #3113 — see `agents/docs/research/wave_pde_alternatives.md`
for the broader motivation. The RPC binder for these helpers lives in
`examples/AutoResearch/AutoResearchRemote.cpp`.
*/

#pragma once

#include "fl/math/wave/wave_simulation_real.h"
#include "fl/stl/chrono.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"
#include "fl/stl/unique_ptr.h"

namespace fl {

namespace wave_perf {

// Result of a single-shot wave-PDE perf measurement.
struct WavePerfResult {
    bool success = false;
    u32 total_us = 0;
    double us_per_update = 0.0;
    double us_per_cell_per_update = 0.0;
    double fps_at_one_update_per_frame = 0.0;
};

// Result of a multi-repeat stability check.
struct WavePerfRepeatResult {
    bool success = false;
    u32 repeats = 0;
    double mean_us_per_update = 0.0;
    double std_dev_us_per_update = 0.0;
    double std_dev_pct = 0.0;
};

// Bench config range gates. Tight enough to refuse pathological RPC
// payloads (e.g. accidentally requesting a 256-MB grid) without
// constraining real experiments.
inline bool isValidGrid(u32 W, u32 H) FL_NOEXCEPT {
    return W >= 4 && H >= 4 && W <= 1024 && H <= 1024;
}
inline bool isValidIterations(u32 iterations) FL_NOEXCEPT {
    return iterations >= 1 && iterations <= 100000;
}
inline bool isValidRepeats(u32 repeats) FL_NOEXCEPT {
    return repeats >= 2 && repeats <= 64;
}

// Build a wave simulator seeded with a deterministic +/-0.5 checkerboard.
// Bench helpers below use this so every run starts from the same state.
// The +/-0.5 amplitude is deliberate — avoids the +/-1.0 float_to_fixed
// asymmetry noted in #3083's audit.
inline fl::unique_ptr<WaveSimulation2D_Real> makeBenchSim(
    u32 W, u32 H, LaplacianStencil stencil) FL_NOEXCEPT {
    auto sim = fl::make_unique<WaveSimulation2D_Real>(W, H, 0.16f, 6.0f);
    sim->setHalfDuplex(false);
    sim->setStencil(stencil);
    for (u32 y = 0; y < H; ++y) {
        for (u32 x = 0; x < W; ++x) {
            sim->setf(static_cast<fl::size>(x),
                      static_cast<fl::size>(y),
                      ((x + y) & 1u) ? -0.5f : 0.5f);
        }
    }
    sim->update();  // warm-up: prime caches, get past init quirks
    return sim;
}

// Single-shot: time `iterations` calls to update() and report
// per-cell-per-update cost. The corresponding `loads_only=true` mode
// (memory-bound baseline) lives in `runWavePerfLoadsOnly` so the
// signatures stay simple — the gap between the two reveals memory
// vs compute cost (critical for the ESP32-S3 PSRAM analysis from
// #3114).
inline WavePerfResult runWavePerf(u32 W, u32 H, u32 iterations,
                                  LaplacianStencil stencil) FL_NOEXCEPT {
    WavePerfResult r;
    if (!isValidGrid(W, H) || !isValidIterations(iterations)) {
        return r;
    }
    auto sim = makeBenchSim(W, H, stencil);
    const u32 t0 = fl::micros();
    for (u32 i = 0; i < iterations; ++i) {
        sim->update();
    }
    const u32 t1 = fl::micros();
    r.success = true;
    r.total_us = t1 - t0;
    const double cells_per_update = static_cast<double>(W) * H;
    r.us_per_update = static_cast<double>(r.total_us)
                       / static_cast<double>(iterations);
    r.us_per_cell_per_update = r.us_per_update / cells_per_update;
    r.fps_at_one_update_per_frame = (r.us_per_update > 0.0)
        ? (1.0e6 / r.us_per_update) : 0.0;
    return r;
}

// Memory-bound baseline: read every inner cell `iterations` times into
// a volatile sink. Touches the same memory pattern as update() with
// none of the kernel arithmetic. The gap between this and runWavePerf
// is the memory-vs-compute breakdown.
inline WavePerfResult runWavePerfLoadsOnly(u32 W, u32 H, u32 iterations,
                                           LaplacianStencil stencil) FL_NOEXCEPT {
    WavePerfResult r;
    if (!isValidGrid(W, H) || !isValidIterations(iterations)) {
        return r;
    }
    auto sim = makeBenchSim(W, H, stencil);
    volatile fl::i32 sink = 0;
    const u32 t0 = fl::micros();
    for (u32 i = 0; i < iterations; ++i) {
        fl::i32 acc = 0;
        for (u32 y = 0; y < H; ++y) {
            for (u32 x = 0; x < W; ++x) {
                acc += sim->geti16(static_cast<fl::size>(x),
                                   static_cast<fl::size>(y));
            }
        }
        sink += acc;
    }
    const u32 t1 = fl::micros();
    (void)sink;
    r.success = true;
    r.total_us = t1 - t0;
    const double cells_per_update = static_cast<double>(W) * H;
    r.us_per_update = static_cast<double>(r.total_us)
                       / static_cast<double>(iterations);
    r.us_per_cell_per_update = r.us_per_update / cells_per_update;
    r.fps_at_one_update_per_frame = (r.us_per_update > 0.0)
        ? (1.0e6 / r.us_per_update) : 0.0;
    return r;
}

// Repeat-stability: run `repeats` measurements of `runWavePerf` and
// report mean + std_dev_pct of us_per_update. Caller checks
// std_dev_pct < 5%; higher → IRQ noise or RPC framing jitter is
// contaminating the data, mark UNTRUSTED.
inline WavePerfRepeatResult runWavePerfRepeat(
    u32 W, u32 H, u32 iterations, u32 repeats,
    LaplacianStencil stencil) FL_NOEXCEPT {
    WavePerfRepeatResult r;
    if (!isValidGrid(W, H) || !isValidIterations(iterations)
        || !isValidRepeats(repeats)) {
        return r;
    }
    // Single sim reused across repeats — the warm-up cost is paid once
    // and each repeat measures steady-state cost.
    auto sim = makeBenchSim(W, H, stencil);
    double sum = 0.0;
    double sum_sq = 0.0;
    for (u32 j = 0; j < repeats; ++j) {
        const u32 t0 = fl::micros();
        for (u32 i = 0; i < iterations; ++i) {
            sim->update();
        }
        const u32 t1 = fl::micros();
        const double us_per_update =
            static_cast<double>(t1 - t0) / static_cast<double>(iterations);
        sum += us_per_update;
        sum_sq += us_per_update * us_per_update;
    }
    const double mean = sum / static_cast<double>(repeats);
    const double variance = (sum_sq / static_cast<double>(repeats))
                          - (mean * mean);
    double std_dev = 0.0;
    if (variance > 0.0) {
        // Newton-Raphson sqrt — avoids pulling in <cmath> on platforms
        // where it's costly. 6 iterations gives ~14 significant digits.
        double s = variance;
        for (int k = 0; k < 6; ++k) {
            s = 0.5 * (s + variance / s);
        }
        std_dev = s;
    }
    r.success = true;
    r.repeats = repeats;
    r.mean_us_per_update = mean;
    r.std_dev_us_per_update = std_dev;
    r.std_dev_pct = (mean > 0.0) ? (100.0 * std_dev / mean) : 0.0;
    return r;
}

}  // namespace wave_perf
}  // namespace fl
