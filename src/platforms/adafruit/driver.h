
#include "fl/unique_ptr.h"
#include "pixel_iterator.h"

namespace fl {

class PixelIterator;

/// Interface for Adafruit NeoPixel driver - implementation in clockless.cpp
class IAdafruitNeoPixelDriver {
public:
    /// Static factory method to create driver implementation
    static unique_ptr<IAdafruitNeoPixelDriver> create();

    virtual ~IAdafruitNeoPixelDriver() = default;
    
    /// Initialize the driver with data pin and RGBW mode
    virtual void init(int dataPin) = 0;
    
    /// Output pixels to the LED strip
    virtual void showPixels(PixelIterator& pixelIterator) = 0;

};

} // namespace fl
