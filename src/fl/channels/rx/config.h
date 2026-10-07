#pragma once

#include "fl/channels/rx.h"
#include "fl/channels/rx/types.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/optional.h"
#include "fl/stl/string.h"

namespace fl {

struct RxChannelConfig {
    fl::string name;
    int pin = -1;
    RxBackend backend = RxBackend::PLATFORM_DEFAULT;
    fl::string affinity;

    size_t edge_capacity = 4096;
    fl::optional<u32> hz = 40000000;
    u32 signal_range_min_ns = 100;
    u32 signal_range_max_ns = 100000;
    u32 skip_signals = 0;
    bool start_low = true;
    bool io_loop_back = false;
    bool use_dma = false;
    /// Decode thresholds for backends that decode while capturing (Teensy
    /// 4.x FlexPWM). Set it to the timing later passed to decode().
    fl::optional<ChipsetTiming4Phase> stream_timing;

    RxChannelConfig() FL_NO_EXCEPT = default;
    explicit RxChannelConfig(int pin_param) FL_NO_EXCEPT
        : pin(pin_param) {}
    RxChannelConfig(int pin_param, RxBackend backend_param) FL_NO_EXCEPT
        : pin(pin_param)
        , backend(backend_param) {}
};

}  // namespace fl
