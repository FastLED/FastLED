#ifndef __INC_WS2812SERIAL_CONTROLLER_H
#define __INC_WS2812SERIAL_CONTROLLER_H

#ifdef USE_WS2812SERIAL
namespace fl {
template<int DATA_PIN, EOrder RGB_ORDER>
class CWS2812SerialController : public CPixelLEDController<RGB_ORDER, 8, 0xFF> {
    WS2812Serial *pserial;
    u8 *drawbuffer,*framebuffer;

    void _init(int nLeds) {
        if (pserial == nullptr) {
            drawbuffer = (u8*)malloc(nLeds * 3);
            framebuffer = (u8*)malloc(nLeds * 12);
            pserial = new WS2812Serial(nLeds, framebuffer, drawbuffer, DATA_PIN, WS2812_RGB);
            pserial->begin();
        }
    }

public:
    CWS2812SerialController() { pserial = nullptr; }

    virtual void init() { /* do nothing yet */ }

    virtual void showPixels(PixelController<RGB_ORDER, 8, 0xFF> & pixels) {
        _init(pixels.size());

        u8 *p = drawbuffer;

        while(pixels.has(1)) {
            *p++ = pixels.loadAndScale0();
            *p++ = pixels.loadAndScale1();
            *p++ = pixels.loadAndScale2();
            pixels.stepDithering();
            pixels.advanceData();
        }
        pserial->show();
    }

};
}  // namespace fl
#endif // USE_WS2812SERIAL
#endif // __INC_WS2812SERIAL_CONTROLLER_H
