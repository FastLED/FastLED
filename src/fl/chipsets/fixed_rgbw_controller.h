#pragma once

#include "fl/channels/clockless_bus_controller.h"
#include "fl/gfx/rgbw.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

/// Keep fixed RGBW behavior around both the original and rebound transport.
template<typename Base>
class FixedRgbwController : public Base {
  public:
    template<fl::Bus B>
    using WithBus = FixedRgbwController<
        typename ClocklessBusController<Base, B>::type>;

    FixedRgbwController() FL_NO_EXCEPT {
        this->setFixedRgbw(fl::RgbwDefault::value());
    }

    void init() FL_NO_EXCEPT override {
        Base::init();
    }
};

} // namespace detail
} // namespace fl
