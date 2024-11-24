#pragma once


#include <stdint.h>

#include "ui.h"
#include "fl/ptr.h"

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN
class InputPin;
FASTLED_NAMESPACE_END


namespace fl {


// A passive infrared sensor common on amazon.
class Pir {
  public:
    Pir(int pin);
    bool detect();
    operator bool() { return detect(); }

  private:
    Button mButton;
    fl::scoped_ptr<InputPin> mImpl;

};

}  // namespace fl