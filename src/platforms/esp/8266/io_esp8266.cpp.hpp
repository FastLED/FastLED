#pragma once

// IWYU pragma: private
// ok no namespace fl
#include "platforms/esp/is_esp.h" // IWYU pragma: keep

#ifdef FL_IS_ESP8266

// ESP8266 console on UART0, driven through the NONOS SDK register map and
// interrupt API. The hardware RX FIFO holds only 128 bytes, so an RX
// interrupt drains it into a ring buffer; a JSON-RPC request that arrives
// while show() is bit-banging survives until the sketch polls again.

#include "fl/stl/chrono.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"
#include "fl/system/yield.h"
#include "platforms/io.h"

// IWYU pragma: begin_keep
#include <esp8266_peri.h>  // SDK UART register map
#include <ets_sys.h>       // SDK ISR attach/enable
// IWYU pragma: end_keep

namespace fl {
namespace platforms {
namespace esp8266_console {

constexpr int kUart = 0;
constexpr u16 kRxSize = 512;  // power of two
constexpr u32 kRxFifoThreshold = 64;
constexpr u32 kRxTimeoutChars = 2;

struct State {
    volatile u8 rx[kRxSize];
    volatile u16 head;  // written by ISR
    volatile u16 tail;  // written by reader
    bool started;
};

// FL_LINT_ALLOW_GLOBAL(ISR-shared state: the IRAM RX handler must not call a flash-resident Singleton accessor)
static State sState = {};

static void IRAM_ATTR rxIsr(void*) FL_NO_EXCEPT {
    const u32 status = USIS(kUart);
    while ((USS(kUart) >> USRXC) & 0xFF) {
        const u8 c = static_cast<u8>(USF(kUart));
        const u16 next = static_cast<u16>((sState.head + 1) & (kRxSize - 1));
        if (next != sState.tail) {  // drop on overflow; never block in an ISR
            sState.rx[sState.head] = c;
            sState.head = next;
        }
    }
    USIC(kUart) = status;
}

inline void ensureStarted(u32 baud) FL_NO_EXCEPT {
    if (sState.started) {
        return;
    }
    sState.started = true;
    ETS_UART_INTR_DISABLE();
    // Route GPIO1/GPIO3 to U0TXD/U0RXD. The ROM leaves them muxed for its
    // boot log, but the core reclaims them as GPIO unless something muxes
    // them back, so writing the FIFO alone puts nothing on the wire.
    IOSWAP &= ~(1u << IOSWAPU0);
    const u8 kPins[] = {1, 3};  // U0TXD, U0RXD
    for (const u8 pin : kPins) {
        GPC(pin) = (GPC(pin) & (0xFu << GPCI));
        GPEC = (1u << pin);
        GPF(pin) = GPFFS(GPFFS_BUS(pin));
    }
    GPF(3) |= (1u << GPFPU);  // RX idles high
    USD(kUart) = ESP8266_CLOCK / baud;
    USC0(kUart) = (3u << UCBN) | (1u << UCSBN);  // 8N1
    USC0(kUart) |= (1u << UCRXRST) | (1u << UCTXRST);
    USC0(kUart) &= ~((1u << UCRXRST) | (1u << UCTXRST));
    USC1(kUart) = (kRxFifoThreshold << UCFFT) | (kRxTimeoutChars << UCTOT) |
                  (1u << UCTOE);
    USIC(kUart) = 0xFFFF;
    USIE(kUart) = (1u << UIFF) | (1u << UITO) | (1u << UIOF);
    ETS_UART_INTR_ATTACH(rxIsr, nullptr);
    ETS_UART_INTR_ENABLE();
}

inline void putByte(u8 c) FL_NO_EXCEPT {
    while (((USS(kUart) >> USTXC) & 0xFF) >= 127) {
    }
    USF(kUart) = c;
}

}  // namespace esp8266_console

void begin(u32 baudRate) FL_NO_EXCEPT {
    const u32 baud = baudRate ? baudRate : 115200;
    esp8266_console::ensureStarted(baud);
    // An earlier print may have started the UART at the default rate.
    USD(esp8266_console::kUart) = ESP8266_CLOCK / baud;
}

void print(const char* str) FL_NO_EXCEPT {
    if (!str) {
        return;
    }
    esp8266_console::ensureStarted(115200);
    while (*str) {
        esp8266_console::putByte(static_cast<u8>(*str++));
    }
}

void println(const char* str) FL_NO_EXCEPT {
    if (!str) {
        return;
    }
    print(str);
    esp8266_console::putByte('\n');
}

int available() FL_NO_EXCEPT {
    using namespace esp8266_console;
    ensureStarted(115200);
    const u16 count = static_cast<u16>((sState.head - sState.tail) & (kRxSize - 1));
    return count;
}

int peek() FL_NO_EXCEPT {
    using namespace esp8266_console;
    if (!available()) {
        return -1;
    }
    return sState.rx[sState.tail];
}

int read() FL_NO_EXCEPT {
    using namespace esp8266_console;
    if (!available()) {
        return -1;
    }
    const u8 c = sState.rx[sState.tail];
    sState.tail = static_cast<u16>((sState.tail + 1) & (kRxSize - 1));
    return c;
}

int readLineNative(char delimiter, char* out, int outLen) FL_NO_EXCEPT {
    if (outLen <= 0) {
        return 0;
    }
    // Same contract as Stream::readStringUntil: block until the delimiter or
    // a 1 s gap between bytes.
    const u32 timeoutMs = 1000;
    u32 start = fl::millis();
    int len = 0;
    while (true) {
        const int value = read();
        if (value < 0) {
            if (fl::millis() - start >= timeoutMs) {
                break;
            }
            fl::yield();
            continue;
        }
        const char c = static_cast<char>(value);
        if (c == delimiter) {
            break;
        }
        if (len < outLen - 1) {
            out[len++] = c;
        }
        start = fl::millis();
    }
    out[len] = '\0';
    return len;
}

bool flush(u32 timeoutMs) FL_NO_EXCEPT {
    const u32 start = fl::millis();
    while ((USS(esp8266_console::kUart) >> USTXC) & 0xFF) {
        if (fl::millis() - start >= timeoutMs) {
            return false;
        }
    }
    return true;
}

size_t write_bytes(const u8* buffer, size_t size) FL_NO_EXCEPT {
    if (!buffer) {
        return 0;
    }
    esp8266_console::ensureStarted(115200);
    for (size_t i = 0; i < size; ++i) {
        esp8266_console::putByte(buffer[i]);
    }
    return size;
}

bool serial_ready() FL_NO_EXCEPT {
    return true;  // a UART bridge has no host-open signal
}

bool serial_is_buffered() FL_NO_EXCEPT {
    return true;
}

}  // namespace platforms
}  // namespace fl

#endif // FL_IS_ESP8266
