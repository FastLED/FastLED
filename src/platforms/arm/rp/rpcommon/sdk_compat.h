#pragma once

// IWYU pragma: private

#include "fl/stl/noexcept.h"
#include "fl/stl/int.h"
// IWYU pragma: begin_keep
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "pico/time.h"
// IWYU pragma: end_keep

namespace fl {
namespace rp_sdk {

// Arduino Mbed ships an older RP2040 SDK with renamed GPIO and SPI APIs.
#if defined(ARDUINO_ARCH_MBED)
inline void gpioInit(uint pin) FL_NO_EXCEPT { _gpio_init(pin); }
inline uint spiInit(spi_inst_t* spi, uint baud) FL_NO_EXCEPT { return _spi_init(spi, baud); }
inline uint uartDreq(uart_inst_t* uart, bool tx) FL_NO_EXCEPT { return uart_get_dreq(uart, tx); }
inline gpio_function uartFunction(uart_inst_t*, uint) FL_NO_EXCEPT { return GPIO_FUNC_UART; }
#else
inline void gpioInit(uint pin) FL_NO_EXCEPT { gpio_init(pin); }
inline uint spiInit(spi_inst_t* spi, uint baud) FL_NO_EXCEPT { return spi_init(spi, baud); }
inline uint uartDreq(uart_inst_t* uart, bool tx) FL_NO_EXCEPT { return uart_get_dreq_num(uart, tx); }
inline gpio_function_t uartFunction(uart_inst_t* uart, uint pin) FL_NO_EXCEPT { return UART_FUNCSEL_NUM(uart, pin); }
#endif

// Mbed disables the Pico default alarm pool, which owns its own hardware alarm.
#if PICO_TIME_DEFAULT_ALARM_POOL_DISABLED
inline bool timerAvailable() FL_NO_EXCEPT { return false; }
inline alarm_id_t addAlarm(u64, alarm_callback_t, void*) FL_NO_EXCEPT { return -1; }
inline bool cancelAlarm(alarm_id_t) FL_NO_EXCEPT { return false; }
#else
inline bool timerAvailable() FL_NO_EXCEPT { return true; }
inline alarm_id_t addAlarm(u64 us, alarm_callback_t callback, void* data) FL_NO_EXCEPT {
    return add_alarm_in_us(us, callback, data, true);
}
inline bool cancelAlarm(alarm_id_t id) FL_NO_EXCEPT { return cancel_alarm(id); }
#endif

} // namespace rp_sdk
} // namespace fl
