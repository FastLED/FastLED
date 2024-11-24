#define FASTLED_INTERNAL
#include "FastLED.h"

#include "sensors/pir.h"
#include "fastpin.h"


namespace fl {


namespace {
  int g_counter = 0;
  Str buttonName() {
    int count = g_counter++;
    if (count == 0) {
      return Str("PIR");
    }
    return Str("Pir ") << g_counter++;
  }
}


Pir::Pir(int pin): mButton(buttonName().c_str()) {
    // pinMode(pin, INPUT);
    mImpl.reset(new InputPin(pin));
}

bool Pir::detect() {
    return mImpl->hival() || mButton.clicked();
}

}  // namespace fl