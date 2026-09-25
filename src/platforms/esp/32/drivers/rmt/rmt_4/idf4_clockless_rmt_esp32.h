/*
 * Integration into FastLED ClocklessController
 * Copyright (c) 2024, Zach Vorhies
 * Copyright (c) 2018,2019,2020 Samuel Z. Guyer
 * Copyright (c) 2017 Thomas Basler
 * Copyright (c) 2017 Martin F. Falatic
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

// IWYU pragma: private

#include "platforms/esp/32/feature_flags/enabled.h"

#if !FASTLED_RMT5

// Signal to the world that we have a ClocklessController to allow WS2812 and others
#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

#include "fl/channels/slim_bridge_controller.h"
#include "platforms/esp/32/core/fastpin_esp32.h"
#include "platforms/esp/32/drivers/rmt/rmt_4/bus_traits.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"

namespace fl {

// Thin bridge (issue #4584): registration with ChannelManager happens in
// SlimBridgeController's constructor via DriverTraits::registerWithManager(),
// which is the only path that makes ChannelManager::onEndFrame() aware of
// this driver so it actually gets its show() called.
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessIdf4 : public SlimBridgeController<DATA_PIN, TIMING, RGB_ORDER, WAIT_TIME, BusTraits<Bus::RMT>>
{
private:
    // -- Verify that the pin is valid
    FL_STATIC_ASSERT(FastPin<DATA_PIN>::validpin(), "This pin has been marked as an invalid pin, common reasons includes it being a ground pin, read only, or too noisy (e.g. hooked up to the uart).");

public:
    u16 getMaxRefreshRate() const FL_NO_EXCEPT override { return 400; }
};

// Backward compatibility alias
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
using ClocklessRMT = ClocklessIdf4<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;

}  // namespace fl

#endif // !FASTLED_RMT5
