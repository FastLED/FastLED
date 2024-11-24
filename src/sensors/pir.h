#pragma once

#include <stdint.h>

#include "digital_pin.h"
#include "fl/ptr.h"
#include "ui.h"

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN
class InputPin;
FASTLED_NAMESPACE_END

namespace fl {

// A passive infrared sensor common on amazon.
// Instantiating this class will create a ui Button when
// compiling using the FastLED web compiler.
class Pir {
  public:
    Pir(int pin);
    bool detect();
    operator bool() { return detect(); }

  private:
    Button mButton;
    DigitalPin mPin;
};


} // namespace fl