#pragma once

// Opt-in UART WS2812 driver for ESP8266 (UART1 / GPIO2).
// Requires: #define FASTLED_ESP8266_UART before including <FastLED.h>
// or add the controller explicitly via FastLED.addLeds<UARTController_ESP8266<GRB>>(...) .

#if defined(ARDUINO_ARCH_ESP8266) || defined(ESP8266)

#ifndef FASTLED_ESP8266_UART_BAUD
#define FASTLED_ESP8266_UART_BAUD 3200000UL  // 3.2 Mbps (4 UART bits = 1.25us LED bit)
#endif

#ifndef FASTLED_ESP8266_UART_RESET_US
#define FASTLED_ESP8266_UART_RESET_US 300    // WS2812 reset/latch; spec >= 50us. Use 300us to be safe.
#endif

#include "led_sysdefs_esp8266.h"
#include "fl/namespace.h"
#include "cpixel_ledcontroller.h"
#include "pixel_controller.h"
#include "eorder.h"
#include "fl/stdint.h"

// Forward declarations for ESP8266 Arduino core functions
// These are provided by the ESP8266 Arduino framework
class HardwareSerial;
extern HardwareSerial Serial1;
extern "C" void delayMicroseconds(unsigned int us);

FASTLED_NAMESPACE_BEGIN

// UART-based WS2812/WS2811 driver for ESP8266 (UART1, TX on GPIO2).
// Initial implementation: RGB only; RGBW can be added by extending encode loop.
template<EOrder RGB_ORDER = GRB>
class UARTController_ESP8266 : public CPixelLEDController<RGB_ORDER> {
public:
    UARTController_ESP8266(int dataPin = 2 /* GPIO2 TX only */)
        : mDataPin(dataPin), mBaud(FASTLED_ESP8266_UART_BAUD) {}

    // Optional: allow runtime override of baud/reset.
    void setBaud(uint32_t baud) { mBaud = baud; }
    void setResetTimeUs(uint16_t us) { mResetUs = us; }

    virtual void init() override;
    virtual void clearLeds(int nLeds) override;
    virtual uint16_t getMaxRefreshRate() const override { return 400; }

protected:
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override;

private:
    int mDataPin;
    uint32_t mBaud;
    uint16_t mResetUs = FASTLED_ESP8266_UART_RESET_US;

    // Encodes two WS2812 bits into one UART byte using 4-bit symbols:
    // 0 -> 1000, 1 -> 1100 ; packed as pairs: 00->0x88, 01->0x8C, 10->0xC8, 11->0xCC.
    static inline uint8_t encode2Bits(uint8_t twoBits);

    // Encode one 8-bit color to 4 UART bytes (MSB first).
    static inline void encodeByte(uint8_t b, uint8_t* out4);

    // Block until UART TX finishes.
    static inline void uartFlush();

    // Setup Serial1 safely (TX-only).
    void beginUartIfNeeded();
};

FASTLED_NAMESPACE_END

#endif // ARDUINO_ARCH_ESP8266 || ESP8266
