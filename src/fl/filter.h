#pragma once

// General-purpose signal smoothing filters.
//
// IIR (stateful, no buffer):
//   ExponentialSmoother<T>       — first-order exponential (IIR/RC) smoother
//   AttackDecayFilter<T>        — asymmetric EMA: separate attack/decay taus
//   CascadedEMA<T, Stages>      — N stages of EMA in series (Gaussian-like)
//   LeakyIntegrator<T, K>       — shift-based EMA, cheap for integer/fixed-point
//   DCBlocker<T>                — single-pole DC offset removal (audio input)
//   BiquadFilter<T>             — second-order IIR (lowpass/highpass/bandpass/notch)
//   KalmanFilter<T>             — 1D scalar Kalman filter
//   OneEuroFilter<T>            — adaptive velocity-based smoothing (VR/graphics)
//
// Multi-channel:
//   SpectralVariance<T>         — per-bin EMA with relative deviation (audio/sensors)
//
// FIR (windowed, buffer-backed):
//   MovingAverage<T, N>          — simple moving average (O(1) running sum)
//   WeightedMovingAverage<T, N>  — linearly weighted moving average
//   TriangularFilter<T, N>       — triangular (tent) weighted average
//   GaussianFilter<T, N>         — binomial-coefficient Gaussian approximation
//   MedianFilter<T, N>           — sliding-window median filter
//   AlphaTrimmedMean<T, N>       — sorted window, trim extremes, average middle
//   HampelFilter<T, N>           — median + deviation threshold outlier rejection
//   SavitzkyGolayFilter<T, N>    — polynomial-fit smoothing (preserves peaks)
//   BilateralFilter<T, N>        — edge-preserving value-similarity weighting
//
// Each FIR filter has a sensible default N (window size). You can also set
// N = 0 for a runtime-sized buffer that takes capacity at construction:
//   MedianFilter<float> mf;          // static, default N = 5
//   MedianFilter<float, 0> mf(10);   // dynamic, capacity set at runtime
//
// ============================================================================
// Filter Reference Table
// ============================================================================
//
// IIR filters (stateful, no buffer, constant memory):
//
//   Filter                       | update()  | Memory | Notes
//   -----------------------------|-----------|--------|-------------------------------
//   ExponentialSmoother<T>       | O(1)      | 2T     | exp() per call, time-aware
//   AttackDecayFilter<T>         | O(1)      | 3T     | exp() per call, asymmetric tau
//   DCBlocker<T>                 | O(1)      | 2T     | 1 mul + 2 adds, no exp()
//   LeakyIntegrator<T, K>        | O(1)      | 1T     | bit-shift only, very cheap
//   CascadedEMA<T, S>            | O(S)      | S*T    | S stages, exp() per call
//   BiquadFilter<T>              | O(1)      | 5T     | 5 muls + 4 adds, LP/HP/BP/notch
//   KalmanFilter<T>              | O(1)      | 4T     | 1 div + few muls
//   OneEuroFilter<T>             | O(1)      | 5T     | 2 EMA steps + derivative
//
// FIR filters (windowed, buffer-backed, N = window size):
//
//   Filter                       | update()  | Memory | Odd N? | Notes
//   -----------------------------|-----------|--------|--------|--------------------
//   MovingAverage<T, N>          | O(1)      | N*T    | no     | running sum trick
//   WeightedMovingAverage<T, N>  | O(N)      | N*T    | no     | linear weights [1..N]
//   TriangularFilter<T, N>       | O(N)      | N*T    | yes    | tent weights, symmetric
//   GaussianFilter<T, N>         | O(N)      | N*T    | no     | binomial coefficients
//   MedianFilter<T, N>           | O(N)      | 2*N*T  | yes    | sorted insert + shift
//   AlphaTrimmedMean<T, N>       | O(N)      | 2*N*T  | no     | sorted + trim + avg
//   HampelFilter<T, N>           | O(N)      | 2*N*T  | yes    | sorted + MAD outlier
//   SavitzkyGolayFilter<T, N>    | O(N)      | N*T    | yes    | polynomial fit, peaks
//   BilateralFilter<T, N>        | O(N)      | N*T    | no     | exp() per sample
//
// "Odd N?" = static_assert enforces odd N (even N auto-corrected at runtime).
//
// ============================================================================
// Choosing a Filter — Common Scenarios
// ============================================================================
//
// Fast attack, slow decay (instant response to rising input, slow fade-out):
//   AttackDecayFilter<float> env(0.01f, 0.5f);  // attack=10ms, decay=500ms
//   float v = env.update(input, dt);
//
// Slow attack, fast decay (sluggish ramp-up, snappy drop-off):
//   AttackDecayFilter<float> env(0.5f, 0.01f);  // attack=500ms, decay=10ms
//   float v = env.update(input, dt);
//   Useful for peak-hold displays or "gravity" effects on VU meters.
//
// Preserve square waves / sharp edges (smooth noise but keep transitions):
//   MedianFilter — rejects impulse noise without smearing edges. A spike
//     gets replaced by the middle value; a genuine step persists once half
//     the window has crossed the threshold.
//   BilateralFilter — weights by value similarity, so samples on the other
//     side of an edge contribute almost nothing. Best edge preservation of
//     the averaging filters.
//   SavitzkyGolayFilter — polynomial fit preserves peaks and step shapes
//     better than any averaging filter, though it does smooth slightly.
//
// Reject random spikes / outliers (sensor glitches, bad readings):
//   MedianFilter — the classic choice. Completely ignores isolated spikes.
//   HampelFilter — like MedianFilter but replaces only statistical outliers
//     (beyond threshold * MAD), passing normal variation through unchanged.
//   AlphaTrimmedMean — trims extremes then averages the rest. Smoother
//     output than pure median, still robust to outliers.
//
// Smooth noisy sensor data (general-purpose, low latency):
//   LeakyIntegrator — cheapest option: one shift + one add. Good enough
//     for most ADC smoothing on microcontrollers.
//   MovingAverage — O(1) update, easy to reason about. N=8 or N=16 are
//     common choices for ADC readings.
//   ExponentialSmoother — time-aware, handles variable sample rates cleanly.
//
// Gaussian-quality smoothing (best frequency response, minimal ringing):
//   GaussianFilter — binomial weights approximate a Gaussian kernel. Best
//     stopband attenuation of the simple FIR filters.
//   CascadedEMA — IIR approximation of Gaussian with no buffer. 3-4 stages
//     gives near-Gaussian impulse response.
//
// Pointer / VR / motion tracking (low lag on fast gestures, smooth at rest):
//   OneEuroFilter — designed specifically for this. Adaptive cutoff based
//     on velocity: low jitter when still, low lag when moving fast.
//
// Audio input cleanup (DC removal, hum rejection):
//   DCBlocker — removes DC offset from microphone / line-in / ADC bias.
//   BiquadFilter::notch(60.0f, sr, 30.0f) — kills 50/60 Hz mains hum.
//   BiquadFilter::highpass(20.0f, sr) — rumble filter, removes sub-bass.
//
// Audio envelope / VU meter:
//   AttackDecayFilter<float> vu(0.001f, 0.3f);  // fast attack, slow decay
//   Or BiquadFilter::butterworth() for a clean low-pass at a specific cutoff.
//
// Frequency isolation (audio band selection):
//   BiquadFilter::bandpass(440.0f, sr, 2.0f) — isolate a frequency band.
//
// Noisy signal with known process model:
//   KalmanFilter — optimal when you can estimate process noise (Q) and
//     measurement noise (R). Converges to the true value over time.
//
// Cheapest possible filter (minimal CPU, integer-only MCU):
//   LeakyIntegrator<int, K> — one right-shift and one add. No division,
//     no multiplication, no buffer. K=2 (alpha=1/4) or K=3 (alpha=1/8).

#include "fl/stl/compiler_control.h"
#include "fl/stl/span.h"

// Detail impl headers — IIR
#include "fl/detail/filter/attack_decay_filter_impl.h"
#include "fl/detail/filter/dc_blocker_impl.h"
#include "fl/detail/filter/exponential_smoother_impl.h"
#include "fl/detail/filter/leaky_integrator_impl.h"
#include "fl/detail/filter/cascaded_ema_impl.h"
#include "fl/detail/filter/biquad_filter_impl.h"
#include "fl/detail/filter/kalman_filter_impl.h"
#include "fl/detail/filter/one_euro_filter_impl.h"

// Detail impl headers — Multi-channel
#include "fl/detail/filter/spectral_variance_impl.h"

// Detail impl headers — FIR
#include "fl/detail/filter/moving_average_impl.h"
#include "fl/detail/filter/median_filter_impl.h"
#include "fl/detail/filter/weighted_moving_average_impl.h"
#include "fl/detail/filter/triangular_filter_impl.h"
#include "fl/detail/filter/gaussian_filter_impl.h"
#include "fl/detail/filter/alpha_trimmed_mean_impl.h"
#include "fl/detail/filter/hampel_filter_impl.h"
#include "fl/detail/filter/savitzky_golay_filter_impl.h"
#include "fl/detail/filter/bilateral_filter_impl.h"

namespace fl {

// ============================================================================
// IIR filters (no buffer, live in fl:: namespace directly)
// ============================================================================

// First-order exponential (RC low-pass) smoother. Time-aware: takes dt so
// it works at any sample rate. Requires one exp() call per update.
//
// update(): O(1)   Memory: 2T
//
//   ExponentialSmoother<float> ema(0.1f);   // tau = 100 ms
//   void loop() {
//       float dt = millis_since_last / 1000.0f;
//       float smoothed = ema.update(analogRead(A0), dt);
//   }
//
// Larger tau = slower response. setTau() lets you change it at runtime.
//
// Fixed-point:
//   using FP = fl::fixed_point<16,16>;
//   ExponentialSmoother<FP> ema(FP(0.5f), FP(0.0f));
//   FP v = ema.update(FP(1.0f), FP(0.05f));
template <typename T = float>
class ExponentialSmoother {
  public:
    explicit ExponentialSmoother(T tau_seconds, T initial = T(0))
        : mImpl(tau_seconds, initial) {}
    FASTLED_FORCE_INLINE T update(T input, T dt_seconds) { return mImpl.update(input, dt_seconds); }
    FASTLED_FORCE_INLINE T value() const { return mImpl.value(); }
    FASTLED_FORCE_INLINE void reset(T initial = T(0)) { mImpl.reset(initial); }
    FASTLED_FORCE_INLINE void setTau(T tau_seconds) { mImpl.setTau(tau_seconds); }
  private:
    detail::ExponentialSmootherImpl<T> mImpl;
};

// Asymmetric exponential smoother with separate time constants for rising
// (attack) and falling (decay) signals. Picks the appropriate tau on each
// update based on whether the input is above or below the current value.
// Same exponential math as ExponentialSmoother, one exp() call per update.
//
// update(): O(1)   Memory: 3T
//
//   AttackDecayFilter<float> env(0.01f, 0.5f);  // attack=10ms, decay=500ms
//   void loop() {
//       float dt = millis_since_last / 1000.0f;
//       float v = env.update(analogRead(A0), dt);
//   }
//
// Swap the tau values for slow-attack / fast-decay (e.g., peak-hold, gravity).
template <typename T = float>
class AttackDecayFilter {
  public:
    AttackDecayFilter(T attack_tau, T decay_tau, T initial = T(0))
        : mImpl(attack_tau, decay_tau, initial) {}
    FASTLED_FORCE_INLINE T update(T input, T dt_seconds) { return mImpl.update(input, dt_seconds); }
    FASTLED_FORCE_INLINE T value() const { return mImpl.value(); }
    FASTLED_FORCE_INLINE void reset(T initial = T(0)) { mImpl.reset(initial); }
    FASTLED_FORCE_INLINE void setAttackTau(T tau_seconds) { mImpl.setAttackTau(tau_seconds); }
    FASTLED_FORCE_INLINE void setDecayTau(T tau_seconds) { mImpl.setDecayTau(tau_seconds); }
  private:
    detail::AttackDecayFilterImpl<T> mImpl;
};

// Single-pole DC offset removal filter. Essential for audio input processing
// (microphone, line-in) where hardware bias or ADC offset adds a constant DC
// component. One multiply and two adds per sample, no exp().
//
// update(): O(1)   Memory: 2T
//
//   DCBlocker<float> dc;             // default R = 0.995
//   void loop() {
//       float clean = dc.update(micSample);  // DC removed
//   }
//
// R controls cutoff: closer to 1.0 = lower cutoff. R=0.995 gives ~1.6 Hz
// at 1 kHz sample rate. R=0.99 gives ~3.2 Hz. R=0.9 is aggressive.
template <typename T = float>
class DCBlocker {
  public:
    explicit DCBlocker(T r = T(0.995f)) : mImpl(r) {}
    FASTLED_FORCE_INLINE T update(T input) { return mImpl.update(input); }
    FASTLED_FORCE_INLINE T value() const { return mImpl.value(); }
    FASTLED_FORCE_INLINE void reset() { mImpl.reset(); }
    FASTLED_FORCE_INLINE void setR(T r) { mImpl.setR(r); }
  private:
    detail::DCBlockerImpl<T> mImpl;
};

// Shift-based EMA: alpha = 1/(2^K). The cheapest filter here — one
// bit-shift and one add for integer types, no exp() or division.
//
// update(): O(1)   Memory: 1T
//
//   LeakyIntegrator<int, 3> li;   // alpha = 1/8
//   void loop() {
//       int smoothed = li.update(rawADC);
//   }
//
// For floats it divides by 2^K instead. K=2 (alpha=0.25) is a good default.
template <typename T = float, int K = 2>
class LeakyIntegrator {
  public:
    LeakyIntegrator() = default;
    explicit LeakyIntegrator(T initial) : mImpl(initial) {}
    FASTLED_FORCE_INLINE T update(T input) { return mImpl.update(input); }
    FASTLED_FORCE_INLINE T value() const { return mImpl.value(); }
    FASTLED_FORCE_INLINE void reset(T initial = T(0)) { mImpl.reset(initial); }
  private:
    detail::LeakyIntegratorImpl<T, K> mImpl;
};

// S stages of EMA in series for a near-Gaussian impulse response without a
// buffer. One exp() call shared across all stages. More stages = smoother
// (but slower to respond).
//
// update(): O(S)   Memory: S*T   (S = Stages, typically 2-4)
//
//   CascadedEMA<float, 3> smooth(0.05f);  // 3-stage, tau = 50 ms
//   void loop() {
//       float v = smooth.update(sensorValue, dt);
//   }
//
// Fixed-point:
//   using FP = fl::fixed_point<16,16>;
//   CascadedEMA<FP, 2> smooth(FP(0.1f), FP(0.0f));
//   FP v = smooth.update(FP(reading), FP(dt));
template <typename T = float, int Stages = 2>
class CascadedEMA {
  public:
    explicit CascadedEMA(T tau_seconds, T initial = T(0))
        : mImpl(tau_seconds, initial) {}
    FASTLED_FORCE_INLINE T update(T input, T dt_seconds) { return mImpl.update(input, dt_seconds); }
    FASTLED_FORCE_INLINE T value() const { return mImpl.value(); }
    FASTLED_FORCE_INLINE void reset(T initial = T(0)) { mImpl.reset(initial); }
    FASTLED_FORCE_INLINE void setTau(T tau_seconds) { mImpl.setTau(tau_seconds); }
  private:
    detail::CascadedEMAImpl<T, Stages> mImpl;
};

// Second-order IIR filter (5 multiplies + 4 adds per sample, no exp()).
// Factory methods for common filter types:
//   butterworth() — low-pass, -12 dB/octave above cutoff
//   highpass()    — high-pass, -12 dB/octave below cutoff
//   bandpass()    — band-pass, passes center frequency, Q controls width
//   notch()       — band-reject, removes center frequency (50/60 Hz hum)
//
// update(): O(1)   Memory: 5T (coefficients) + 4T (state)
//
//   auto lpf = BiquadFilter<float>::butterworth(100.0f, 1000.0f);
//   auto hpf = BiquadFilter<float>::highpass(20.0f, 44100.0f);
//   auto bpf = BiquadFilter<float>::bandpass(440.0f, 44100.0f, 2.0f);
//   auto hum = BiquadFilter<float>::notch(60.0f, 44100.0f, 30.0f);
//
// You can also construct directly with custom coefficients (b0,b1,b2,a1,a2).
template <typename T = float>
class BiquadFilter {
    using Impl = detail::BiquadFilterImpl<T>;
    BiquadFilter(const Impl& impl) : mImpl(impl) {}
  public:
    BiquadFilter(T b0, T b1, T b2, T a1, T a2)
        : mImpl(b0, b1, b2, a1, a2) {}
    static BiquadFilter butterworth(float cutoff_hz, float sample_rate) {
        return BiquadFilter(Impl::butterworth(cutoff_hz, sample_rate));
    }
    static BiquadFilter highpass(float cutoff_hz, float sample_rate) {
        return BiquadFilter(Impl::highpass(cutoff_hz, sample_rate));
    }
    static BiquadFilter bandpass(float center_hz, float sample_rate,
                                  float q = 1.0f) {
        return BiquadFilter(Impl::bandpass(center_hz, sample_rate, q));
    }
    static BiquadFilter notch(float center_hz, float sample_rate,
                               float q = 1.0f) {
        return BiquadFilter(Impl::notch(center_hz, sample_rate, q));
    }
    FASTLED_FORCE_INLINE T update(T input) { return mImpl.update(input); }
    FASTLED_FORCE_INLINE T value() const { return mImpl.value(); }
    FASTLED_FORCE_INLINE void reset() { mImpl.reset(); }
  private:
    Impl mImpl;
};

// 1-D scalar Kalman filter. Balances process noise (Q) against measurement
// noise (R) to produce an optimal estimate. One division per update.
//
// update(): O(1)   Memory: 4T
//
//   KalmanFilter<float> kf(0.01f, 0.1f);  // Q = 0.01, R = 0.1
//   void loop() {
//       float estimate = kf.update(noisyMeasurement);
//   }
//
// Fixed-point:
//   using FP = fl::fixed_point<16,16>;
//   KalmanFilter<FP> kf(FP(0.01f), FP(0.1f));
//   FP estimate = kf.update(FP(measurement));
template <typename T = float>
class KalmanFilter {
  public:
    KalmanFilter(T process_noise, T measurement_noise, T initial = T(0))
        : mImpl(process_noise, measurement_noise, initial) {}
    FASTLED_FORCE_INLINE T update(T measurement) { return mImpl.update(measurement); }
    FASTLED_FORCE_INLINE T value() const { return mImpl.value(); }
    FASTLED_FORCE_INLINE void reset(T initial = T(0)) { mImpl.reset(initial); }
  private:
    detail::KalmanFilterImpl<T> mImpl;
};

// Adaptive velocity-based smoother (1-Euro filter). Two EMA steps plus a
// derivative estimate per update. Smooths slow movements heavily but lets
// fast movements through with minimal lag. Ideal for VR/graphics/pointer.
//
// update(): O(1)   Memory: 5T
//
//   OneEuroFilter<float> oef(1.0f, 0.5f);  // min_cutoff=1 Hz, beta=0.5
//   void loop() {
//       float dt = millis_since_last / 1000.0f;
//       float smoothed = oef.update(pointerX, dt);
//   }
//
// Higher beta = less lag on fast motions but more jitter at rest.
template <typename T = float>
class OneEuroFilter {
  public:
    OneEuroFilter(T min_cutoff, T beta, T d_cutoff = T(1.0f))
        : mImpl(min_cutoff, beta, d_cutoff) {}
    FASTLED_FORCE_INLINE T update(T input, T dt) { return mImpl.update(input, dt); }
    FASTLED_FORCE_INLINE T value() const { return mImpl.value(); }
    FASTLED_FORCE_INLINE void reset(T initial = T(0)) { mImpl.reset(initial); }
  private:
    detail::OneEuroFilterImpl<T> mImpl;
};

// ============================================================================
// FIR filter public API — composition with impl
// N > 0: static (inlined) buffer, default-constructible
// N == 0: dynamic buffer, requires capacity at construction
// ============================================================================

// Simple moving average using a running sum: subtracts the oldest sample
// and adds the newest, so each update is O(1) regardless of window size.
// Good general-purpose smoother with equal weight on all samples.
//
// update(): O(1)   Memory: N*T
//
//   MovingAverage<float> ma;               // default N = 8
//   MovingAverage<float, 16> ma16;         // explicit 16-sample window
//   MovingAverage<float, 0> dyn(32);       // dynamic: capacity set at runtime
//   void loop() {
//       ma.update(analogRead(A0));
//       float smoothed = ma.value();       // average of last 8 readings
//   }
//
// Fixed-point:
//   using FP = fl::fixed_point<16,16>;
//   MovingAverage<FP, 4> ma;
//   ma.update(FP(reading));
//   float result = ma.value().to_float();
template <typename T = float, fl::size N = 8>
class MovingAverage {
  public:
    MovingAverage() = default;
    explicit MovingAverage(fl::size capacity) : mImpl(capacity) {}
    FASTLED_FORCE_INLINE T update(T input) { return mImpl.update(input); }
    FASTLED_FORCE_INLINE T value() const { return mImpl.value(); }
    FASTLED_FORCE_INLINE void reset() { mImpl.reset(); }
    FASTLED_FORCE_INLINE bool full() const { return mImpl.full(); }
    FASTLED_FORCE_INLINE fl::size size() const { return mImpl.size(); }
    FASTLED_FORCE_INLINE fl::size capacity() const { return mImpl.capacity(); }
    FASTLED_FORCE_INLINE void resize(fl::size new_capacity) { mImpl.resize(new_capacity); }
  private:
    detail::MovingAverageImpl<T, N> mImpl;
};

// Sliding-window median: maintains a sorted copy of the window via binary
// search + shift. Rejects outliers by returning the middle value. Best for
// impulse/spike noise (e.g., bad sensor readings, random spikes).
// N must be odd (static_assert for compile-time, auto-corrected at runtime).
//
// update(): O(N)   Memory: 2*N*T (ring + sorted copy)
//
//   MedianFilter<float> mf;                // default N = 5
//   MedianFilter<float, 0> dyn(11);        // dynamic, odd N required
//   void loop() {
//       mf.update(distanceSensor());
//       float clean = mf.value();          // spike-free output
//   }
//
// Fixed-point:
//   using FP = fl::fixed_point<16,16>;
//   MedianFilter<FP, 5> mf;
//   mf.update(FP(reading));
//   float result = mf.value().to_float();
template <typename T = float, fl::size N = 5>
class MedianFilter {
  public:
    MedianFilter() = default;
    explicit MedianFilter(fl::size capacity) : mImpl(capacity) {}
    FASTLED_FORCE_INLINE T update(T input) { return mImpl.update(input); }
    FASTLED_FORCE_INLINE T update(fl::span<const T> values) { return mImpl.update(values); }
    FASTLED_FORCE_INLINE T value() const { return mImpl.value(); }
    FASTLED_FORCE_INLINE void reset() { mImpl.reset(); }
    FASTLED_FORCE_INLINE fl::size size() const { return mImpl.size(); }
    FASTLED_FORCE_INLINE fl::size capacity() const { return mImpl.capacity(); }
    FASTLED_FORCE_INLINE void resize(fl::size new_capacity) { mImpl.resize(new_capacity); }
  private:
    detail::MedianFilterImpl<T, N> mImpl;
};

// Linearly weighted moving average: recomputes weighted sum each update.
// Weights are [1, 2, 3, ..., N] so newer samples dominate. Use when recent
// data matters more than older.
//
// update(): O(N)   Memory: N*T
//
//   WeightedMovingAverage<float> wma;       // default N = 8
//   void loop() {
//       wma.update(sensorValue);
//       float smoothed = wma.value();       // recent-biased average
//   }
//
// Fixed-point:
//   using FP = fl::fixed_point<16,16>;
//   WeightedMovingAverage<FP, 3> wma;
//   wma.update(FP(reading));
//   float result = wma.value().to_float();
template <typename T = float, fl::size N = 8>
class WeightedMovingAverage {
  public:
    WeightedMovingAverage() = default;
    explicit WeightedMovingAverage(fl::size capacity) : mImpl(capacity) {}
    FASTLED_FORCE_INLINE T update(T input) { return mImpl.update(input); }
    FASTLED_FORCE_INLINE T update(fl::span<const T> values) { return mImpl.update(values); }
    FASTLED_FORCE_INLINE T value() const { return mImpl.value(); }
    FASTLED_FORCE_INLINE void reset() { mImpl.reset(); }
    FASTLED_FORCE_INLINE bool full() const { return mImpl.full(); }
    FASTLED_FORCE_INLINE fl::size size() const { return mImpl.size(); }
    FASTLED_FORCE_INLINE fl::size capacity() const { return mImpl.capacity(); }
    FASTLED_FORCE_INLINE void resize(fl::size new_capacity) { mImpl.resize(new_capacity); }
  private:
    detail::WeightedMovingAverageImpl<T, N> mImpl;
};

// Triangular (tent) weighted average: recomputes tent-shaped weights each
// update. Weights for N=5: [1, 2, 3, 2, 1]. Smoother than MovingAverage,
// cheaper than Gaussian. N must be odd for a symmetric peak
// (static_assert for compile-time, auto-corrected at runtime).
//
// update(): O(N)   Memory: N*T
//
//   TriangularFilter<float> tf;             // default N = 7
//   void loop() {
//       tf.update(sensorValue);
//       float smoothed = tf.value();
//   }
//
// Fixed-point:
//   using FP = fl::fixed_point<16,16>;
//   TriangularFilter<FP, 5> tf;
//   tf.update(FP(reading));
//   float result = tf.value().to_float();
template <typename T = float, fl::size N = 7>
class TriangularFilter {
  public:
    TriangularFilter() = default;
    explicit TriangularFilter(fl::size capacity) : mImpl(capacity) {}
    FASTLED_FORCE_INLINE T update(T input) { return mImpl.update(input); }
    FASTLED_FORCE_INLINE T update(fl::span<const T> values) { return mImpl.update(values); }
    FASTLED_FORCE_INLINE T value() const { return mImpl.value(); }
    FASTLED_FORCE_INLINE void reset() { mImpl.reset(); }
    FASTLED_FORCE_INLINE bool full() const { return mImpl.full(); }
    FASTLED_FORCE_INLINE fl::size size() const { return mImpl.size(); }
    FASTLED_FORCE_INLINE fl::size capacity() const { return mImpl.capacity(); }
    FASTLED_FORCE_INLINE void resize(fl::size new_capacity) { mImpl.resize(new_capacity); }
  private:
    detail::TriangularFilterImpl<T, N> mImpl;
};

// Binomial-coefficient Gaussian approximation: recomputes Pascal's-triangle
// weights each update. Weights for N=5: [1, 4, 6, 4, 1]. Best
// frequency-domain response of the simple FIR filters.
//
// update(): O(N)   Memory: N*T
//
//   GaussianFilter<float> gf;               // default N = 5
//   void loop() {
//       gf.update(sensorValue);
//       float smoothed = gf.value();
//   }
//
// Fixed-point:
//   using FP = fl::fixed_point<16,16>;
//   GaussianFilter<FP, 5> gf;
//   for (int i = 0; i < 5; ++i) gf.update(FP(7.0f));
//   float result = gf.value().to_float();  // ≈ 7.0
template <typename T = float, fl::size N = 5>
class GaussianFilter {
  public:
    GaussianFilter() = default;
    explicit GaussianFilter(fl::size capacity) : mImpl(capacity) {}
    FASTLED_FORCE_INLINE T update(T input) { return mImpl.update(input); }
    FASTLED_FORCE_INLINE T update(fl::span<const T> values) { return mImpl.update(values); }
    FASTLED_FORCE_INLINE T value() const { return mImpl.value(); }
    FASTLED_FORCE_INLINE void reset() { mImpl.reset(); }
    FASTLED_FORCE_INLINE bool full() const { return mImpl.full(); }
    FASTLED_FORCE_INLINE fl::size size() const { return mImpl.size(); }
    FASTLED_FORCE_INLINE fl::size capacity() const { return mImpl.capacity(); }
    FASTLED_FORCE_INLINE void resize(fl::size new_capacity) { mImpl.resize(new_capacity); }
  private:
    detail::GaussianFilterImpl<T, N> mImpl;
};

// Alpha-trimmed mean: maintains a sorted window via binary search + shift,
// trims extreme values from both ends, and averages the middle. With N=7
// and trim_count=1, drops min and max, averages the remaining 5. Robust
// to outliers while still averaging.
//
// update(): O(N)   Memory: 2*N*T (ring + sorted copy)
//
//   AlphaTrimmedMean<float> atm(1);         // default N=7, trim 1 each end
//   AlphaTrimmedMean<float, 5> atm5(2);     // N=5, trim 2 each end (= median)
//   void loop() {
//       atm.update(sensorValue);
//       float robust = atm.value();
//   }
//
// Fixed-point:
//   using FP = fl::fixed_point<16,16>;
//   AlphaTrimmedMean<FP, 5> atm(1);
//   atm.update(FP(reading));
//   float result = atm.value().to_float();
template <typename T = float, fl::size N = 7>
class AlphaTrimmedMean {
  public:
    explicit AlphaTrimmedMean(fl::size trim_count = 1) : mImpl(trim_count) {}
    AlphaTrimmedMean(fl::size capacity, fl::size trim_count) : mImpl(capacity, trim_count) {}
    FASTLED_FORCE_INLINE T update(T input) { return mImpl.update(input); }
    FASTLED_FORCE_INLINE T update(fl::span<const T> values) { return mImpl.update(values); }
    FASTLED_FORCE_INLINE T value() const { return mImpl.value(); }
    FASTLED_FORCE_INLINE void reset() { mImpl.reset(); }
    FASTLED_FORCE_INLINE fl::size size() const { return mImpl.size(); }
    FASTLED_FORCE_INLINE fl::size capacity() const { return mImpl.capacity(); }
    FASTLED_FORCE_INLINE void resize(fl::size new_capacity, fl::size trim_count) { mImpl.resize(new_capacity, trim_count); }
  private:
    detail::AlphaTrimmedMeanImpl<T, N> mImpl;
};

// Hampel filter: maintains a sorted window, computes the median and median
// absolute deviation (MAD), then replaces values that deviate beyond
// threshold * MAD with the median. N must be odd (static_assert for
// compile-time, auto-corrected at runtime).
//
// update(): O(N)   Memory: 2*N*T (ring + sorted copy)
//
//   HampelFilter<float> hf(3.0f);           // default N=5, threshold=3
//   HampelFilter<float, 7> hf7(2.0f);       // 7-sample window, tighter threshold
//   void loop() {
//       float clean = hf.update(noisyValue); // outliers replaced with median
//   }
template <typename T = float, fl::size N = 5>
class HampelFilter {
  public:
    explicit HampelFilter(T threshold = T(3.0f)) : mImpl(threshold) {}
    HampelFilter(fl::size capacity, T threshold) : mImpl(capacity, threshold) {}
    FASTLED_FORCE_INLINE T update(T input) { return mImpl.update(input); }
    FASTLED_FORCE_INLINE T update(fl::span<const T> values) { return mImpl.update(values); }
    FASTLED_FORCE_INLINE T value() const { return mImpl.value(); }
    FASTLED_FORCE_INLINE void reset() { mImpl.reset(); }
    FASTLED_FORCE_INLINE fl::size size() const { return mImpl.size(); }
    FASTLED_FORCE_INLINE fl::size capacity() const { return mImpl.capacity(); }
    FASTLED_FORCE_INLINE void resize(fl::size new_capacity) { mImpl.resize(new_capacity); }
  private:
    detail::HampelFilterImpl<T, N> mImpl;
};

// Savitzky-Golay: fits a local quadratic polynomial to the window and returns
// the center value. Recomputes polynomial weights each update. Smooths noise
// while preserving peaks, edges, and signal shape better than averaging.
// N must be odd (static_assert for compile-time, auto-corrected at runtime).
//
// update(): O(N)   Memory: N*T
//
//   SavitzkyGolayFilter<float> sg;           // default N = 5
//   SavitzkyGolayFilter<float, 7> sg7;       // wider window = more smoothing
//   void loop() {
//       sg.update(spectrumBin);
//       float smoothed = sg.value();         // peaks preserved
//   }
//
// Fixed-point:
//   using FP = fl::fixed_point<16,16>;
//   SavitzkyGolayFilter<FP, 5> sg;
//   for (int i = 0; i < 5; ++i) sg.update(FP(3.0f));
//   float result = sg.value().to_float();   // ≈ 3.0
template <typename T = float, fl::size N = 5>
class SavitzkyGolayFilter {
  public:
    SavitzkyGolayFilter() = default;
    explicit SavitzkyGolayFilter(fl::size capacity) : mImpl(capacity) {}
    FASTLED_FORCE_INLINE T update(T input) { return mImpl.update(input); }
    FASTLED_FORCE_INLINE T update(fl::span<const T> values) { return mImpl.update(values); }
    FASTLED_FORCE_INLINE T value() const { return mImpl.value(); }
    FASTLED_FORCE_INLINE void reset() { mImpl.reset(); }
    FASTLED_FORCE_INLINE bool full() const { return mImpl.full(); }
    FASTLED_FORCE_INLINE fl::size size() const { return mImpl.size(); }
    FASTLED_FORCE_INLINE fl::size capacity() const { return mImpl.capacity(); }
    FASTLED_FORCE_INLINE void resize(fl::size new_capacity) { mImpl.resize(new_capacity); }
  private:
    detail::SavitzkyGolayFilterImpl<T, N> mImpl;
};

// Bilateral filter: edge-preserving smoother weighted by value similarity.
// Computes exp() per sample in the window each update. Each sample's weight
// depends on how close its value is to the newest sample. Small sigma_range
// = only very similar values contribute (sharp edges kept). Large sigma_range
// = all values contribute equally (acts like a box filter).
//
// update(): O(N)   Memory: N*T   (one exp() per sample per update)
//
//   BilateralFilter<float> bf(1.0f);        // default N=5, sigma=1.0
//   BilateralFilter<float, 7> bf7(0.5f);    // 7-sample, tighter similarity
//   void loop() {
//       bf.update(ledBrightness);
//       float smoothed = bf.value();         // edges preserved
//   }
template <typename T = float, fl::size N = 5>
class BilateralFilter {
  public:
    explicit BilateralFilter(T sigma_range = T(1.0f)) : mImpl(sigma_range) {}
    BilateralFilter(fl::size capacity, T sigma_range) : mImpl(capacity, sigma_range) {}
    FASTLED_FORCE_INLINE T update(T input) { return mImpl.update(input); }
    FASTLED_FORCE_INLINE T update(fl::span<const T> values) { return mImpl.update(values); }
    FASTLED_FORCE_INLINE T value() const { return mImpl.value(); }
    FASTLED_FORCE_INLINE void reset() { mImpl.reset(); }
    FASTLED_FORCE_INLINE bool full() const { return mImpl.full(); }
    FASTLED_FORCE_INLINE fl::size size() const { return mImpl.size(); }
    FASTLED_FORCE_INLINE fl::size capacity() const { return mImpl.capacity(); }
    FASTLED_FORCE_INLINE void resize(fl::size new_capacity) { mImpl.resize(new_capacity); }
  private:
    detail::BilateralFilterImpl<T, N> mImpl;
};

// ============================================================================
// Multi-channel filters
// ============================================================================

// Multi-channel EMA with per-bin relative deviation measurement. Feeds a
// span of values (e.g., FFT bins, sensor array) each update, smooths each
// channel independently with an EMA, and returns the mean relative deviation
// from the smoothed values. Detects temporal instability across channels.
//
// update(): O(N)   Memory: N*T (one EMA state per channel)
//
// Use cases:
//   - Audio: detect spectral change over time (voice vs steady instrument)
//   - Sensors: detect unstable channels in a multi-sensor array
//
//   SpectralVariance<float> sv(0.2f);     // alpha=0.2, 5-frame window
//   void loop() {
//       float variance = sv.update(fftBins);  // mean per-bin relative change
//       if (variance > threshold) { /* spectral content is changing */ }
//   }
//
// Alpha controls tracking speed:
//   0.1 = slow (10-frame window), 0.2 = moderate (default), 0.5 = fast
template <typename T = float>
class SpectralVariance {
  public:
    explicit SpectralVariance(T alpha = T(0.2f), T floor = T(1e-4f))
        : mImpl(alpha, floor) {}
    FASTLED_FORCE_INLINE T update(fl::span<const T> bins) { return mImpl.update(bins); }
    FASTLED_FORCE_INLINE T value() const { return mImpl.value(); }
    FASTLED_FORCE_INLINE void reset() { mImpl.reset(); }
    FASTLED_FORCE_INLINE void setAlpha(T alpha) { mImpl.setAlpha(alpha); }
    FASTLED_FORCE_INLINE void setFloor(T floor) { mImpl.setFloor(floor); }
    FASTLED_FORCE_INLINE fl::size size() const { return mImpl.size(); }
  private:
    detail::SpectralVarianceImpl<T> mImpl;
};

} // namespace fl
