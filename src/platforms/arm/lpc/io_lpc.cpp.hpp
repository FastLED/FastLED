#pragma once

// IWYU pragma: private

#include "platforms/arm/lpc/is_lpc.h"

#ifdef FL_IS_ARM_LPC

#include "fl/stl/cstddef.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/stdint.h"
#include "platforms/arm/is_arm.h"
#include "platforms/io.h"

#if defined(ARDUINO)
// LPC Arduino core exposes the standard Serial API. LPC's HardwareSerial lacks
// readStringUntil(), so readLineNative() is implemented by hand below.
// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
#include "fl/system/yield.h"
#include "fl/stl/chrono.h"
// IWYU pragma: end_keep
#endif

#if defined(ARDUINO) && (defined(FL_IS_ARM_LPC_845) || defined(FL_IS_ARM_LPC_804))
// The core's HardwareSerial is fully polled and the LPC8xx USART has no
// receive FIFO: RXDAT holds ONE byte, and a byte that finishes arriving
// while RXDAT is still unread sets STAT.OVERRUNINT and is lost. At 115200
// baud that is an 87 us window, which an idle Remote::update() loop on a
// 24 MHz M0+ misses: on an LPC845-BRK the byte after a request's leading
// '{' was dropped on about half of all RPCs, so the device answered
// -32600 "method" (or nothing) instead of the call. Receive through
// USART0's RXRDY interrupt into a small ring instead.
//
// Ownership: the ring serves FastLED's serial input API -- fl::serial_begin()
// and fl::available()/peek()/read()/readStringUntil(), which Remote/RPC and
// AutoResearch use. A sketch that only uses Arduino `Serial` is unaffected:
// none of this code runs. Once fl::serial_begin() enables the interrupt, or
// an fl:: read drains RXDAT, received bytes belong to that API, so a sketch
// that mixes it with direct `Serial.read()`/`Serial.available()` must read
// through fl:: instead. Build with -DFL_LPC_SERIAL_RX_RING=0 to restore the
// polled HardwareSerial path (and its overrun losses).
#ifndef FL_LPC_SERIAL_RX_RING
#define FL_LPC_SERIAL_RX_RING 1
#endif
// IWYU pragma: begin_keep
#include "platforms/arm/lpc/led_sysdefs_arm_lpc.h"  // vendor CMSIS PAL: USART0, NVIC, PRIMASK
// IWYU pragma: end_keep
#endif

#if defined(FL_LPC_SERIAL_RX_RING) && FL_LPC_SERIAL_RX_RING
// Bytes buffered between the USART0 ISR and the polled readers. The ISR
// keeps up with the line however long a request is; the ring only has to
// cover how long the reader goes without draining (5.5 ms at 64 bytes).
#ifndef FL_LPC_SERIAL_RX_BUFFER_SIZE
#define FL_LPC_SERIAL_RX_BUFFER_SIZE 64
#endif
FL_STATIC_ASSERT(FL_LPC_SERIAL_RX_BUFFER_SIZE >= 2 &&
                     FL_LPC_SERIAL_RX_BUFFER_SIZE <= 256 &&
                     (FL_LPC_SERIAL_RX_BUFFER_SIZE &
                      (FL_LPC_SERIAL_RX_BUFFER_SIZE - 1)) == 0,
                 "FL_LPC_SERIAL_RX_BUFFER_SIZE must be a power of two from 2 to 256");

namespace fl {
namespace platforms {
namespace lpc_serial_rx {

// Shared between the USART0 ISR and task context. Single core: the ISR is
// atomic with respect to task code, and every task-side access runs inside
// a Masked critical section, so no field needs to be atomic.
struct Ring {
    u8 buf[FL_LPC_SERIAL_RX_BUFFER_SIZE];
    u8 head;  // next slot to write
    u8 tail;  // next slot to read
};

inline Ring& ring() FL_NO_EXCEPT {
    static Ring r = {};  // okay static in header -- single TU via _build.cpp.hpp
    return r;
}

constexpr u8 kMask = static_cast<u8>(FL_LPC_SERIAL_RX_BUFFER_SIZE - 1);

// Move every byte USART0 holds into the ring; a full ring drops the newest
// byte. Runs in the ISR, and inside Masked from the readers so input still
// works when the sketch started Serial itself and the IRQ is not enabled.
inline void drainUsart() FL_NO_EXCEPT {
    Ring& r = ring();
    while ((USART0->STAT & USART_STAT_RXRDY_MASK) != 0u) {
        const u8 b = static_cast<u8>(USART0->RXDAT);
        const u8 next = static_cast<u8>((r.head + 1u) & kMask);
        if (next != r.tail) {
            r.buf[r.head] = b;
            r.head = next;
        }
    }
}

// Task-side critical section: masks interrupts (restoring the caller's
// PRIMASK on exit) and first pulls in anything still sitting in RXDAT.
class Masked {
  public:
    Masked() FL_NO_EXCEPT : mPrimask(__get_PRIMASK()) {
        __disable_irq();
        drainUsart();
    }
    ~Masked() FL_NO_EXCEPT { __set_PRIMASK(mPrimask); }
    Masked(const Masked&) = delete;
    Masked& operator=(const Masked&) = delete;

  private:
    u32 mPrimask;
};

// Pops (or, with consume == false, peeks) the oldest byte; -1 if empty.
inline int take(bool consume) FL_NO_EXCEPT {
    Masked masked;
    Ring& r = ring();
    if (r.head == r.tail) {
        return -1;
    }
    const int value = r.buf[r.tail];
    if (consume) {
        r.tail = static_cast<u8>((r.tail + 1u) & kMask);
    }
    return value;
}

}  // namespace lpc_serial_rx
}  // namespace platforms
}  // namespace fl

// Strong override of the core's weak vector alias (framework-arduino-lpc8xx
// #38). Exactly one definition: this file is included once, from the LPC
// _build.cpp.hpp unity slot.
extern "C" void USART0_IRQHandler(void) {
    fl::platforms::lpc_serial_rx::drainUsart();
}
#endif  // FL_LPC_SERIAL_RX_RING

namespace fl {
namespace platforms {

#if defined(ARDUINO)

void begin(u32 baudRate) FL_NO_EXCEPT {
    Serial.begin(baudRate);
#if defined(FL_LPC_SERIAL_RX_RING) && FL_LPC_SERIAL_RX_RING
    lpc_serial_rx::Masked masked;  // also discards a byte left in RXDAT
    lpc_serial_rx::ring().tail = lpc_serial_rx::ring().head;
    USART0->INTENSET = USART_INTENSET_RXRDYEN_MASK;
    NVIC_ClearPendingIRQ(USART0_IRQn);
    NVIC_EnableIRQ(USART0_IRQn);
#endif
}

void print(const char* str) FL_NO_EXCEPT {
    if (!Serial) return;
    Serial.print(str);
}

void println(const char* str) FL_NO_EXCEPT {
    if (!Serial) return;
    // FastLED #3313: Serial.println silently drops on LPC8xx -- bytes
    // never reach the USB-VCOM bridge. Empirically, splitting into two
    // Serial.print calls (the same byte-write path FL_DBG uses via
    // fl::printf -> platforms::print) reaches the host reliably. Root
    // cause is in Print::println's path in the Arduino core; filed
    // separately. This workaround unblocks every fl::println / FL_WARN_LIT
    // call site on LPC.
    Serial.print(str);
    Serial.print("\r\n");
}

#if defined(FL_LPC_SERIAL_RX_RING) && FL_LPC_SERIAL_RX_RING
int available() FL_NO_EXCEPT {
    lpc_serial_rx::Masked masked;
    const lpc_serial_rx::Ring& r = lpc_serial_rx::ring();
    return static_cast<u8>((r.head - r.tail) & lpc_serial_rx::kMask);
}

int peek() FL_NO_EXCEPT {
    return lpc_serial_rx::take(false);
}

int read() FL_NO_EXCEPT {
    return lpc_serial_rx::take(true);
}
#else
int available() FL_NO_EXCEPT {
    return Serial.available();
}

int peek() FL_NO_EXCEPT {
    return Serial.peek();
}

int read() FL_NO_EXCEPT {
    return Serial.read();
}
#endif

// LPC's HardwareSerial lacks readStringUntil(), so read the line manually.
// Goes through read() above, not Serial, so it shares the receive ring.
int readLineNative(char delimiter, char* out, int outLen) FL_NO_EXCEPT {
    if (outLen <= 0) {
        return 0;
    }
    const unsigned long timeoutMs = 1000UL;
    unsigned long start = fl::millis();
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
    (void)timeoutMs;
    if (!Serial) return true;
    Serial.flush();
    return true;
}

size_t write_bytes(const u8* buffer, size_t size) FL_NO_EXCEPT {
    if (!Serial) return 0;
    return Serial.write(buffer, size);
}

bool serial_ready() FL_NO_EXCEPT {
    return Serial ? true : false;
}

bool serial_is_buffered() FL_NO_EXCEPT {
    return true;  // LPC Arduino uses buffered UART
}

#else  // bare-metal: no Serial available yet

void begin(u32 baudRate) FL_NO_EXCEPT {
    (void)baudRate;
}

void print(const char* str) FL_NO_EXCEPT {
    (void)str;
}

void println(const char* str) FL_NO_EXCEPT {
    (void)str;
}

int available() FL_NO_EXCEPT {
    return 0;
}

int peek() FL_NO_EXCEPT {
    return -1;
}

int read() FL_NO_EXCEPT {
    return -1;
}

int readLineNative(char delimiter, char* out, int outLen) FL_NO_EXCEPT {
    (void)delimiter;
    (void)out;
    (void)outLen;
    return -1;
}

bool flush(u32 timeoutMs) FL_NO_EXCEPT {
    (void)timeoutMs;
    return true;
}

size_t write_bytes(const u8* buffer, size_t size) FL_NO_EXCEPT {
    (void)buffer;
    (void)size;
    return 0;
}

bool serial_ready() FL_NO_EXCEPT {
    return false;
}

bool serial_is_buffered() FL_NO_EXCEPT {
    return false;
}

#endif  // ARDUINO

}  // namespace platforms
}  // namespace fl

#endif  // FL_IS_ARM_LPC
