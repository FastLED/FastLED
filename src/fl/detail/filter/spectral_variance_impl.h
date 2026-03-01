#pragma once

#include "fl/stl/span.h"
#include "fl/stl/vector.h"

namespace fl {
namespace detail {

/// Multi-channel EMA with per-bin relative deviation measurement.
///
/// Smooths each channel (bin) with an exponential moving average, then
/// computes the mean relative deviation of the current values from the
/// smoothed values. Useful for detecting temporal instability in
/// multi-dimensional signals (e.g., spectral bins, sensor arrays).
///
/// update(): O(N)   Memory: N*T (one EMA state per channel)
///
///   SpectralVarianceImpl<float> sv(0.2f);  // alpha=0.2, ~5 frame window
///   float variance = sv.update(fftBins);   // returns mean relative deviation
///
/// The alpha parameter controls the EMA speed:
///   - alpha=0.1: slow tracking, 10-frame window, detects slower changes
///   - alpha=0.2: moderate tracking, 5-frame window (default)
///   - alpha=0.5: fast tracking, 2-frame window, detects rapid transients
///
/// The floor parameter prevents division by zero in near-silent channels.
/// Channels below the floor are excluded from the deviation calculation.
template <typename T = float>
class SpectralVarianceImpl {
  public:
    explicit SpectralVarianceImpl(T alpha = T(0.2f), T floor = T(1e-4f))
        : mAlpha(alpha), mFloor(floor), mVariance(T(0)) {}

    /// Feed a new frame of multi-channel data. Returns mean relative deviation.
    /// First call initializes internal state and returns 0.
    T update(fl::span<const T> bins) {
        const int n = static_cast<int>(bins.size());
        if (n == 0) return T(0);

        // Initialize on first call or if channel count changes
        if (mSmoothed.size() != static_cast<fl::size>(n)) {
            mSmoothed.resize(n);
            for (int i = 0; i < n; ++i) {
                mSmoothed[i] = bins[i];
            }
            mVariance = T(0);
            return T(0);
        }

        T sumRelChange = T(0);
        int activeCount = 0;

        for (int i = 0; i < n; ++i) {
            T smoothed = mSmoothed[i];
            T denom = (smoothed > mFloor) ? smoothed : mFloor;

            // Only count channels with meaningful energy
            if (smoothed > mFloor) {
                T diff = bins[i] - smoothed;
                T absDiff = (diff >= T(0)) ? diff : -diff;
                sumRelChange += absDiff / denom;
                ++activeCount;
            }

            // Update EMA: smoothed = alpha * current + (1-alpha) * smoothed
            mSmoothed[i] = mAlpha * bins[i] + (T(1) - mAlpha) * smoothed;
        }

        mVariance = (activeCount >= 2)
                        ? sumRelChange / static_cast<T>(activeCount)
                        : T(0);
        return mVariance;
    }

    /// Get the last computed variance without updating.
    T value() const { return mVariance; }

    /// Reset all internal state.
    void reset() {
        mSmoothed.clear();
        mVariance = T(0);
    }

    /// Change the EMA alpha (tracking speed).
    void setAlpha(T alpha) { mAlpha = alpha; }

    /// Change the floor threshold for active channel detection.
    void setFloor(T floor) { mFloor = floor; }

    /// Get the number of channels being tracked.
    fl::size size() const { return mSmoothed.size(); }

  private:
    T mAlpha;
    T mFloor;
    T mVariance;
    fl::vector<T> mSmoothed;
};

} // namespace detail
} // namespace fl
