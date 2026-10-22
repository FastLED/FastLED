#pragma once
#include <esp_heap_caps.h>
#include <esp_arduino_version.h>
#include <esp_idf_version.h>
#include "fl/system/serial.h"
#include "fl/stl/json.h"
#if !FL_TEST_SERIAL_NATIVE_ONLY
#include "fl/system/arduino.h"
#include <esp32-hal-uart.h>
#include <driver/uart.h>
#include "fl/system/delay.h"
#endif
#if FL_TEST_SERIAL_NATIVE_ONLY
namespace serial_startup_hil {
inline void begin() { fl::serial_begin(115200); }
inline fl::json probe(const fl::json&) {
    const auto before = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    const bool integrityBefore = heap_caps_check_integrity_all(true);
    const fl::u8 nativeRecord[] = "NATIVE_4892_RECORD\r\n";
    bool nativeWrites = true;
    for (int i = 0; i < 8; ++i) {
        nativeWrites = (fl::write_bytes(nativeRecord, sizeof(nativeRecord) - 1) == sizeof(nativeRecord) - 1) && nativeWrites;
    }
    fl::flush(50);
    const bool ready = fl::serial_ready();
    const bool integrityAfter = heap_caps_check_integrity_all(true);
    const auto after = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    fl::json result = fl::json::object();
    result.set("coreVersion", ESP_ARDUINO_VERSION_STR);
    result.set("sdkMatches", ESP_IDF_VERSION == ESP_IDF_VERSION_VAL(5, 5, 5));
    result.set("nativeOnly", true);
    result.set("arduinoFirst", false);
    result.set("nativeWrites", nativeWrites);
    result.set("writeReady", ready);
    result.set("heapBefore", static_cast<int64_t>(before));
    result.set("heapAfter", static_cast<int64_t>(after));
    result.set("heapIntegrity", integrityBefore && integrityAfter);
    result.set("pass", nativeWrites && ready && integrityBefore && integrityAfter);
    return result;
}
}

#else
namespace serial_startup_hil {
struct Startup {
    int rx = -99;
    int tx = -99;
    bool writeReady = false;
};
inline Startup& startup() { static Startup value; return value; }
inline void begin() {
    Startup& s = startup();
    s.rx = uart_get_RxPin(0);
    s.tx = uart_get_TxPin(0);
#if FL_TEST_ARDUINO_FIRST
    Serial0.begin(115200);
    fl::serial_begin(115200);
#else
    fl::serial_begin(115200);
    Serial0.begin(115200);
#endif
    s.writeReady = Serial0.availableForWrite() > 0;
}
inline fl::json probe(const fl::json&) {
    const auto before = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    const bool integrityBefore = heap_caps_check_integrity_all(true);
    Serial0.flush();
    fl::flush(50);
    const bool explicitPins = Serial0.setPins(16, 17);
    const bool explicitObserved = uart_get_RxPin(0) == 16 && uart_get_TxPin(0) == 17;
    const bool restored = Serial0.setPins(SOC_RX0, SOC_TX0);
    const bool restoredObserved = uart_get_RxPin(0) == SOC_RX0 && uart_get_TxPin(0) == SOC_TX0;
    // The temporary RX pin is unwired. Discard input from that route after
    // returning to the host's RX pin, before accepting the next RPC command.
    fl::delayMicroseconds(200);
    size_t transitionBytes = 0;
    uart_get_buffered_data_len(UART_NUM_0, &transitionBytes);
    uart_flush_input(UART_NUM_0);
    bool nativeWrites = true;
    Serial0.print("\r\n");
    const fl::u8 nativeRecord[] = "NATIVE_4892_RECORD\r\n";
    for (int i = 0; i < 8; ++i) {
        Serial0.println("ARDUINO_4892_RECORD");
        nativeWrites = (fl::write_bytes(nativeRecord, sizeof(nativeRecord) - 1) == sizeof(nativeRecord) - 1) && nativeWrites;
    }
    Serial0.flush();
    fl::flush(50);
    const bool integrityAfter = heap_caps_check_integrity_all(true);
    const auto after = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    const Startup& s = startup();
    fl::json result = fl::json::object();
    result.set("coreVersion", ESP_ARDUINO_VERSION_STR);
    result.set("sdkMatches", ESP_IDF_VERSION == ESP_IDF_VERSION_VAL(5, 5, 5));
    result.set("nativeOnly", false);
    result.set("startupRx", s.rx);
    result.set("startupTx", s.tx);
    result.set("expectedRx", SOC_RX0);
    result.set("expectedTx", SOC_TX0);
    result.set("writeReady", s.writeReady);
    result.set("nativeWrites", nativeWrites);
    result.set("transitionBytesDiscarded", static_cast<int64_t>(transitionBytes));
    result.set("explicitPins", explicitPins && explicitObserved);
    result.set("restored", restored && restoredObserved);
    result.set("heapBefore", static_cast<int64_t>(before));
    result.set("heapAfter", static_cast<int64_t>(after));
    result.set("heapIntegrity", integrityBefore && integrityAfter);
    result.set("arduinoFirst", FL_TEST_ARDUINO_FIRST != 0);
    result.set("pass", s.rx == SOC_RX0 && s.tx == SOC_TX0 && s.writeReady && nativeWrites && explicitPins && explicitObserved && restored && restoredObserved && integrityBefore && integrityAfter);
    return result;
}
}

#endif
