

#if defined(ESP32)

#include "esp_log.h"

// use gcc intialize constructor
// to set log level to ESP_LOG_VERBOSE
// before setup() is called
__attribute__((constructor))
void on_startup() {
  esp_log_level_set("*", ESP_LOG_VERBOSE);        // set all components to ERROR level
}

#endif  // ESP32

// Testing RMT RX Validation with GPIO Loopback
// Multi-LED validation test using io_loop_back (no jumper wire needed)

#include "../examples/Validation/Validation.ino"
