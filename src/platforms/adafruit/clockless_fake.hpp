/// @file clockless.cpp
/// Implementation of IAdafruitNeoPixelDriver
/// 
/// This file contains the actual Adafruit_NeoPixel integration, keeping the
/// dependency isolated from header files to avoid PlatformIO LDF issues.


#include "platforms/adafruit/driver.h"
#include "fl/unused.h"
#include "fl/warn.h"

namespace fl {

// Concrete implementation of IAdafruitNeoPixelDriver
class AdafruitNeoPixelDriverFake : public IAdafruitNeoPixelDriver {
public:
    AdafruitNeoPixelDriverFake()  {}
    
    ~AdafruitNeoPixelDriverFake() override = default;
    
    void init(int dataPin) override {
        FL_UNUSED(dataPin);
        FL_WARN("Please install adafruit neopixel package to use this api bridge.");
    }
    
    void showPixels(PixelIterator& pixelIterator) override {
        FL_UNUSED(pixelIterator);
        FL_WARN("Please install adafruit neopixel package to use this api bridge.");
    }
};

// Static factory method implementation
fl::unique_ptr<IAdafruitNeoPixelDriver> IAdafruitNeoPixelDriver::create() {
    return fl::unique_ptr<IAdafruitNeoPixelDriver>(new AdafruitNeoPixelDriverFake());
}

} // namespace fl
