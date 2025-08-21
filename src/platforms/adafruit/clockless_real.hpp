/// @file clockless.cpp
/// Implementation of IAdafruitNeoPixelDriver
/// 
/// This file contains the actual Adafruit_NeoPixel integration, keeping the
/// dependency isolated from header files to avoid PlatformIO LDF issues.

#include <Adafruit_NeoPixel.h>


#include "fl/unique_ptr.h"
#include "fl/memory.h"
#include "pixel_iterator.h"
#include "platforms/adafruit/driver.h"

namespace fl {

// Concrete implementation of IAdafruitNeoPixelDriver
class AdafruitNeoPixelDriverImpl : public IAdafruitNeoPixelDriver {
private:
    unique_ptr<Adafruit_NeoPixel> mNeoPixel;
    bool mInitialized;
    int mDataPin;
    
public:
    AdafruitNeoPixelDriverImpl() 
        : mNeoPixel(nullptr), mInitialized(false), mDataPin(-1) {}
    
    ~AdafruitNeoPixelDriverImpl() override = default;
    
    void init(int dataPin) override {
        if (!mInitialized) {
            mDataPin = dataPin;
            mInitialized = true;
        }
    }
    
    void showPixels(PixelIterator& pixelIterator) override {
        if (!mInitialized) {
            return;
        }
        
        // Get pixel count from iterator
        int numPixels = pixelIterator.size();
        auto rgbw = pixelIterator.get_rgbw();
        
        // Determine the appropriate NeoPixel type based on RGBW mode
        uint16_t neoPixelType = NEO_KHZ800;
        if (rgbw.active()) {
            neoPixelType |= NEO_RGBW;  // RGBW mode
        } else {
            neoPixelType |= NEO_RGB;   // RGB mode
        }
        
        // Create or recreate NeoPixel instance if needed
        if (!mNeoPixel || mNeoPixel->numPixels() != numPixels) {
            if (mNeoPixel) {
                mNeoPixel.reset();
            }
            mNeoPixel = fl::make_unique<Adafruit_NeoPixel>(
                numPixels, mDataPin, neoPixelType);
            mNeoPixel->begin();
        }
        
        // Convert pixel data using PixelIterator and send to Adafruit_NeoPixel
        if (rgbw.active()) {
            // RGBW mode
            for (int i = 0; pixelIterator.has(1); ++i) {
                fl::u8 r, g, b, w;
                pixelIterator.loadAndScaleRGBW(&r, &g, &b, &w);
                mNeoPixel->setPixelColor(i, r, g, b, w);
                pixelIterator.advanceData();
            }
        } else {
            // RGB mode
            for (int i = 0; pixelIterator.has(1); ++i) {
                fl::u8 r, g, b;
                pixelIterator.loadAndScaleRGB(&r, &g, &b);
                mNeoPixel->setPixelColor(i, r, g, b);
                pixelIterator.advanceData();
            }
        }
        
        // Output to LEDs
        mNeoPixel->show();
    }
};

// Static factory method implementation
fl::unique_ptr<IAdafruitNeoPixelDriver> IAdafruitNeoPixelDriver::create() {
    return fl::make_unique<AdafruitNeoPixelDriverImpl>();
}

} // namespace fl
