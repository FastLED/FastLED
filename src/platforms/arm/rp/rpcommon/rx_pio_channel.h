#pragma once

// IWYU pragma: private

#include "platforms/arm/rp/is_rp.h"

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

#include "fl/channels/rx.h"
#include "platforms/arm/rp/rpcommon/rp_pio_edge_capture.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"

namespace fl {

constexpr size_t kRpPioRxEdgeCapacity = 100u * 3u * 16u + 1u;
using RpPioRxEdgeStorage = fl::FixedVector<EdgeTime, kRpPioRxEdgeCapacity>;

/// @brief RP PIO RX lifecycle device. Capture programming is Phase 2.
class RpPioRxDevice : public RxDevice {
  public:
    static fl::shared_ptr<RxDevice> create(int pin) FL_NO_EXCEPT;

    bool begin(const RxConfig& config) FL_NO_EXCEPT override;
    const char* lastBeginError() const FL_NO_EXCEPT override { return mLastBeginError; }
    bool finished() const FL_NO_EXCEPT override;
    RxWaitResult wait(u32 timeout_ms) FL_NO_EXCEPT override;
    fl::result<u32, DecodeError> decode(const ChipsetTiming4Phase& timing,
                                        fl::span<u8> out) FL_NO_EXCEPT override;
    size_t getRawEdgeTimes(fl::span<EdgeTime> out, size_t offset = 0) FL_NO_EXCEPT override;
    const char* name() const FL_NO_EXCEPT override;
    int getPin() const FL_NO_EXCEPT override;
    bool injectEdges(fl::span<const EdgeTime> edges) FL_NO_EXCEPT override;
    ~RpPioRxDevice() FL_NO_EXCEPT override;

  private:
    template <typename T, typename... Args>
    friend fl::shared_ptr<T> fl::make_shared(Args&&... args) FL_NO_EXCEPT;
    explicit RpPioRxDevice(int pin) FL_NO_EXCEPT;
    void stop() FL_NO_EXCEPT;
    void collectDurations() FL_NO_EXCEPT;
    void finishTailIfIdle() FL_NO_EXCEPT;

    int mPin;
    void* mPio;
    int mStateMachine;
    int mDmaChannel;
    int mProgramOffset;
    u32 mPioClockHz;
    u32 mTailLimitNs;
    u32 mLastTransitionUs;
    size_t mCapacity;
    bool mArmed;
    bool mFinished;
    bool mOverflow;
    bool mProgramLoaded;
    bool mIdleHigh;
    bool mSampleHigh;
    bool mHaveSample;
    const char* mLastBeginError;
    size_t mDmaWordCount;
    size_t mDmaWordsProcessed;
    size_t mSampleRunTicks;
    u16 mProgramInstructions[16];
    RpPioEdgeCapture mCapture;
    u32* mDmaWords;
    RpPioRxEdgeStorage* mEdges;
};

}  // namespace fl

#endif
