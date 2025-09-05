#pragma once

#ifndef USE_FASTLED
#define USE_FASTLED
#endif  // USE_FASTLED

#define COLOR_ORDER_RBG

#include "src/I2SClockLessLedDriver.h"

// Bring I2SClocklessLedDriver into fl namespace
namespace fl {
    using I2SClocklessLedDriver = ::I2SClocklessLedDriver;
    
    // Create the ESP32S3 specific alias that the code expects
    using I2SClocklessLedDriveresp32S3 = I2SClocklessLedDriver;
}
