#pragma once

#include "fl/channels/worker_policy.h"
#include "fl/gfx/rgbw.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

/// Keep fixed RGBW behavior around both the original and rebound transport.
template<typename Base>
class FixedRgbwController : public Base {
  public:
    template<fl::Bus B, fl::Core CORE>
    using WithWorker = FixedRgbwController<
        typename ClocklessWorkerController<Base, B, CORE>::type>;

    FixedRgbwController() FL_NO_EXCEPT {
        this->setFixedRgbw(fl::RgbwDefault::value());
    }

    void init() FL_NO_EXCEPT override {
        Base::init();
    }
};

} // namespace detail
} // namespace fl
