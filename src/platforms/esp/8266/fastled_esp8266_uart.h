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
#include "cpixel_ledcontroller.h"
#include "pixel_controller.h"
#include "eorder.h"
#include "fl/stl/stdint.h"

// Forward declarations for ESP8266 Arduino core functions
// These are provided by the ESP8266 Arduino framework
class HardwareSerial;
extern HardwareSerial Serial1;
extern "C" void delayMicroseconds(unsigned int us);
namespace fl {
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

// ---- Template Implementation -----------------------------------------------

// Helper to extract a 2-bit group from a byte, MSB-first.
// group 0: bits 7..6, group 1: 5..4, group 2: 3..2, group 3: 1..0
static inline uint8_t _pair_from_byte(uint8_t b, uint8_t groupIdx) {
    uint8_t shift = 6 - (groupIdx * 2);
    return (b >> shift) & 0x03;
}

template<EOrder RGB_ORDER>
inline uint8_t UARTController_ESP8266<RGB_ORDER>::encode2Bits(uint8_t twoBits) {
    // twoBits layout: bit1 (MSB) | bit0 (LSB)
    // Map: 00->0x88, 01->0x8C, 10->0xC8, 11->0xCC
    static const uint8_t LUT[4] = { 0x88, 0x8C, 0xC8, 0xCC };
    return LUT[twoBits & 0x03];
}

template<EOrder RGB_ORDER>
inline void UARTController_ESP8266<RGB_ORDER>::encodeByte(uint8_t b, uint8_t* out4) {
    out4[0] = encode2Bits(_pair_from_byte(b, 0));
    out4[1] = encode2Bits(_pair_from_byte(b, 1));
    out4[2] = encode2Bits(_pair_from_byte(b, 2));
    out4[3] = encode2Bits(_pair_from_byte(b, 3));
}

template<EOrder RGB_ORDER>
inline void UARTController_ESP8266<RGB_ORDER>::uartFlush() {
    Serial1.flush(); // blocks until TX buffer drained
}

template<EOrder RGB_ORDER>
void UARTController_ESP8266<RGB_ORDER>::beginUartIfNeeded() {
    // Re-init every show() is unnecessary; only if not begun or baud changed.
    static uint32_t s_currentBaud = 0;
    if (s_currentBaud != mBaud) {
        // UART1 on ESP8266 is TX-only on GPIO2.
        Serial1.begin(mBaud);
        // Optional: invert if experimenting with different encodings
        // Serial1.set_tx_invert(false);
        s_currentBaud = mBaud;
    }
}

template<EOrder RGB_ORDER>
void UARTController_ESP8266<RGB_ORDER>::init() {
    beginUartIfNeeded();
}

template<EOrder RGB_ORDER>
void UARTController_ESP8266<RGB_ORDER>::clearLeds(int nLeds) {
    // Send zeros as a quick clear.
    // Build a tiny zero frame for one LED and reuse.
    const int bytesPerLed = 12; // 24 bits -> 12 UART bytes
    uint8_t zeroEncoded[bytesPerLed];
    // 0x00 encodes to 0x88,0x88,0x88,0x88
    for (int i = 0; i < bytesPerLed; ++i) zeroEncoded[i] = 0x88;

    beginUartIfNeeded();
    for (int i = 0; i < nLeds; ++i) {
        Serial1.write(zeroEncoded, bytesPerLed);
    }
    uartFlush();
    delayMicroseconds(mResetUs);
}

template<EOrder RGB_ORDER>
void UARTController_ESP8266<RGB_ORDER>::showPixels(PixelController<RGB_ORDER> &pixels) {
    beginUartIfNeeded();

    const int n = pixels.mLen;
    if (n <= 0) return;

    // Temporary buffer for one LED (3 colors * 4 UART bytes each = 12)
    uint8_t enc[12];

    // Iterate pixels; encode and stream out.
    pixels.preStepFirstByteDithering();
    for (int i = 0; i < n; ++i) {
        // Fetch pixel in wire order with FastLED's scaling/dithering applied
        // loadAndScale0/1/2() already handle RGB_ORDER reordering
        uint8_t b0 = pixels.loadAndScale0();
        uint8_t b1 = pixels.loadAndScale1();
        uint8_t b2 = pixels.loadAndScale2();

        // Encode each byte into 4 UART bytes (MSB-first)
        encodeByte(b0, &enc[0]);
        encodeByte(b1, &enc[4]);
        encodeByte(b2, &enc[8]);

        Serial1.write(enc, sizeof(enc));
        pixels.stepDithering();
        pixels.advanceData();
    }

    uartFlush();
    delayMicroseconds(mResetUs);
}
}  // namespace fl
#endif // ARDUINO_ARCH_ESP8266 || ESP8266
