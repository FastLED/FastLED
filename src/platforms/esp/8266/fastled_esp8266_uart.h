#pragma once

// IWYU pragma: private

// Opt-in UART WS2812 driver for ESP8266 (UART1 / GPIO2).
// Requires: #define FASTLED_ESP8266_UART before including <FastLED.h>
// or add the controller explicitly via FastLED.addLeds<UARTController_ESP8266<GRB>>(...) .
//
// Wire format: UART1 runs 6N1 with TX inverted at 3.2 Mbaud, so one UART
// frame is 8 slots of 312.5 ns and carries two LED bits:
//
//   slot:  start  d0  d1  d2 | d3  d4  d5  stop
//   wire:    H   ~d0 ~d1 ~d2 |~d3 ~d4 ~d5   L
//
// The inverted start bit opens the first LED bit high and the inverted stop
// bit closes the second one low; the line idles low between frames, which
// is also the WS2812 latch level. LED '0' = H L L L (T0H 312 ns), LED '1' =
// H H H L (T1H 937 ns), both inside the WS2812B datasheet windows.

#include "platforms/esp/is_esp.h"

#if defined(FL_IS_ESP8266)

#ifndef FASTLED_ESP8266_UART_BAUD
#define FASTLED_ESP8266_UART_BAUD 3200000UL  // 4 UART slots = one 1.25 us LED bit
#endif

#ifndef FASTLED_ESP8266_UART_RESET_US
#define FASTLED_ESP8266_UART_RESET_US 300    // WS2812 reset/latch; spec >= 50us. Use 300us to be safe.
#endif

#include "platforms/esp/8266/led_sysdefs_esp8266.h"
#include "cpixel_ledcontroller.h"
#include "pixel_controller.h"
#include "eorder.h"
#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"
#include "fl/system/delay.h"

// IWYU pragma: begin_keep
#include <esp8266_peri.h>  // SDK UART/GPIO register map
// IWYU pragma: end_keep

namespace fl {
namespace esp8266_uart {

constexpr int kUart = 1;
constexpr u8 kTxPin = 2;         // U1TXD is only routed to GPIO2
constexpr u8 kFifoDepth = 128;

// UART byte for an MSB-first pair of LED bits, (first << 1) | second.
inline u8 encodeBitPair(u8 twoBits) FL_NO_EXCEPT {
    static const u8 kLut[4] = {0x37, 0x07, 0x34, 0x04};
    return kLut[twoBits & 0x03];
}

// One LED byte (MSB first) becomes four UART bytes.
inline void encodeByte(u8 b, u8* out4) FL_NO_EXCEPT {
    out4[0] = encodeBitPair(static_cast<u8>(b >> 6));
    out4[1] = encodeBitPair(static_cast<u8>(b >> 4));
    out4[2] = encodeBitPair(static_cast<u8>(b >> 2));
    out4[3] = encodeBitPair(b);
}

inline void begin(u32 baud) FL_NO_EXCEPT {
    GPC(kTxPin) = (GPC(kTxPin) & (0xFu << GPCI));
    GPEC = (1u << kTxPin);
    GPF(kTxPin) = GPFFS(GPFFS_BUS(kTxPin));
    USD(kUart) = ESP8266_CLOCK / baud;
    // 6 data bits, 1 stop bit, no parity, TX inverted so the idle line is low.
    USC0(kUart) = (1u << UCBN) | (1u << UCSBN) | (1u << UCTXI);
    USC0(kUart) |= (1u << UCTXRST);
    USC0(kUart) &= ~(1u << UCTXRST);
    USIE(kUart) = 0;
}

inline u8 txFifoCount() FL_NO_EXCEPT {
    return static_cast<u8>((USS(kUart) >> USTXC) & 0xFF);
}

inline void write(const u8* data, u16 len) FL_NO_EXCEPT {
    for (u16 i = 0; i < len; ++i) {
        while (txFifoCount() >= kFifoDepth - 1) {
        }
        USF(kUart) = data[i];
    }
}

// Spin until the last frame has left the shift register.
inline void flush() FL_NO_EXCEPT {
    while (txFifoCount() != 0) {
    }
    // The FIFO count drops when the shifter takes the last byte; give that
    // frame (8 slots) time to reach the wire.
    fl::delayMicroseconds(4);
}

}  // namespace esp8266_uart

// UART-based WS2812/WS2811 driver for ESP8266 (UART1, TX on GPIO2).
// RGB only; RGBW can be added by extending the encode loop.
template<EOrder RGB_ORDER = GRB>
class UARTController_ESP8266 : public CPixelLEDController<RGB_ORDER> {
public:
    UARTController_ESP8266(int dataPin = 2 /* GPIO2 TX only */)
        FL_NO_EXCEPT : mDataPin(dataPin), mBaud(FASTLED_ESP8266_UART_BAUD) {}

    // Optional: allow runtime override of baud/reset.
    void setBaud(u32 baud) FL_NO_EXCEPT { mBaud = baud; mBegunBaud = 0; }
    void setResetTimeUs(u16 us) FL_NO_EXCEPT { mResetUs = us; }

    virtual void init() FL_NO_EXCEPT override { beginUartIfNeeded(); }
    virtual void clearLeds(int nLeds) FL_NO_EXCEPT override;
    virtual u16 getMaxRefreshRate() const FL_NO_EXCEPT override { return 400; }

protected:
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) FL_NO_EXCEPT override;

private:
    int mDataPin;  // informational: UART1 TX cannot leave GPIO2
    u32 mBaud;
    u32 mBegunBaud = 0;
    u16 mResetUs = FASTLED_ESP8266_UART_RESET_US;

    void beginUartIfNeeded() FL_NO_EXCEPT {
        if (mBegunBaud != mBaud) {
            esp8266_uart::begin(mBaud);
            mBegunBaud = mBaud;
        }
    }
};

// ---- Template Implementation -----------------------------------------------

template<EOrder RGB_ORDER>
void UARTController_ESP8266<RGB_ORDER>::clearLeds(int nLeds) FL_NO_EXCEPT {
    u8 zero[4];
    esp8266_uart::encodeByte(0, zero);
    beginUartIfNeeded();
    for (int i = 0; i < nLeds * 3; ++i) {
        esp8266_uart::write(zero, sizeof(zero));
    }
    esp8266_uart::flush();
    fl::delayMicroseconds(mResetUs);
}

template<EOrder RGB_ORDER>
void UARTController_ESP8266<RGB_ORDER>::showPixels(PixelController<RGB_ORDER> &pixels) FL_NO_EXCEPT {
    beginUartIfNeeded();

    const int n = pixels.mLen;
    if (n <= 0) return;

    // One LED = 3 colour bytes * 4 UART bytes.
    u8 enc[12];

    pixels.preStepFirstByteDithering();
    for (int i = 0; i < n; ++i) {
        // loadAndScale0/1/2() return wire order with scaling/dithering applied.
        esp8266_uart::encodeByte(pixels.loadAndScale0(), &enc[0]);
        esp8266_uart::encodeByte(pixels.loadAndScale1(), &enc[4]);
        esp8266_uart::encodeByte(pixels.loadAndScale2(), &enc[8]);
        esp8266_uart::write(enc, sizeof(enc));
        pixels.stepDithering();
        pixels.advanceData();
    }

    esp8266_uart::flush();
    fl::delayMicroseconds(mResetUs);
}
}  // namespace fl
#endif // FL_IS_ESP8266
