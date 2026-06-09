// ok no namespace fl
#pragma once

/// @file platforms/shared/channel_poll_signal.h
/// @brief Fallback channel-manager poll wake signal.

#include "fl/stl/atomic.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {

class ChannelPollSignal {
  public:
    ChannelPollSignal() FL_NOEXCEPT : mPending(false) {}

    void notify() FL_NOEXCEPT { mPending.store(true); }

    bool wait(fl::u32 timeoutMs) FL_NOEXCEPT {
        (void)timeoutMs;
        return mPending.exchange(false);
    }

  private:
    fl::atomic_bool mPending;
};

} // namespace platforms
} // namespace fl
