

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

#include "../examples/FestivalStick/FestivalStick.ino"
