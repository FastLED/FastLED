

#ifdef ARDUINO_ESP32_DEV
#include "fl/compiler_control.h"

#include "platforms/esp/esp_version.h"
#include "driver/ledc.h"
#include "esp32-hal-ledc.h"

// Ancient versions of ESP32 on Arduino (IDF < 4.0) did not have analogWrite() defined.
// IDF 4.4 and 5.0+ both have analogWrite() available, so this polyfill is only needed
// for very old IDF versions. We use a weak symbol so it auto-disables on newer platforms.
#if !ESP_IDF_VERSION_4_OR_HIGHER
FL_LINK_WEAK void analogWrite(uint8_t pin, int value) {
  // Setup PWM channel for the pin if not already done
  static bool channels_setup[16] = {false}; // ESP32 has 16 PWM channels
  static uint8_t channel_counter = 0;
  
  // Find or assign channel for this pin
  static uint8_t pin_to_channel[40] = {255}; // ESP32 has up to 40 GPIO pins, 255 = unassigned
  if (pin_to_channel[pin] == 255) {
    pin_to_channel[pin] = channel_counter++;
    if (channel_counter > 15) channel_counter = 0; // Wrap around
  }
  
  uint8_t channel = pin_to_channel[pin];
  
  // Setup channel if not already done
  if (!channels_setup[channel]) {
    ledcSetup(channel, 5000, 8); // 5kHz frequency, 8-bit resolution
    ledcAttachPin(pin, channel);
    channels_setup[channel] = true;
  }
  
  // Write PWM value (0-255 for 8-bit resolution)
  ledcWrite(channel, value);
}
#endif // !ESP_IDF_VERSION_4_OR_HIGHER



#endif  // ESP32
