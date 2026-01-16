// ESP I/O implementation - Trampoline dispatcher
//
// This file includes either Arduino or ESP-IDF backend based on build flags:
// - Uses Arduino Serial if ARDUINO is defined (unless FL_NO_ARDUINO is set)
// - Uses ESP-IDF ROM functions otherwise
//
// Build flags:
// - ARDUINO: Defined by Arduino framework
// - FL_NO_ARDUINO: Force ESP-IDF backend even when Arduino is available
//
// ok no namespace fl

#if defined(ESP32) || defined(ESP8266)

// Determine which backend to use at compile time and include it
#if defined(ARDUINO) && !defined(FL_NO_ARDUINO)
    // Use Arduino backend - includes Serial.print() implementation
    #include "io_esp_arduino.hpp"
#else
    // Use ESP-IDF backend - includes ROM UART implementation
    // Warning, this version doesnt seem to work.
    #include "io_esp_idf.hpp"
#endif

#endif  // ESP32 || ESP8266
