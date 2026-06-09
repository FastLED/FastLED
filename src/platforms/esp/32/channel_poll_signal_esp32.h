#pragma once

/// @file platforms/esp/32/channel_poll_signal_esp32.h
/// @brief ESP32 channel-manager poll wake signal.

#include "fl/stl/atomic.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/semaphore.h"

namespace fl {
namespace platforms {

class ChannelPollSignal {
  public:
    ChannelPollSignal() FL_NOEXCEPT : mSignal(0), mPending(false) {}

    void notify() FL_NOEXCEPT {
        const bool alreadyPending = mPending.exchange(true, fl::memory_order_acq_rel);
        if (!alreadyPending) {
            mSignal.release();
        }
    }

    bool wait(fl::u32 timeoutMs) FL_NOEXCEPT {
        const bool signaled = (timeoutMs == 0) ? waitForever() : mSignal.try_acquire_for_ms(timeoutMs);
        if (signaled) {
            mPending.store(false, fl::memory_order_release);
        }
        return signaled;
    }

  private:
    bool waitForever() FL_NOEXCEPT {
        mSignal.acquire();
        return true;
    }

    fl::binary_semaphore mSignal;
    fl::atomic_bool mPending;
};

} // namespace platforms
} // namespace fl
