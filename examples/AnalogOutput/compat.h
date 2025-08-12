

#ifdef ESP32
#include "fl/compiler_control.h"

#include "platforms/esp/esp_version.h"
#include "driver/ledc.h"
#include "esp32-hal-ledc.h"

// Ancient versions of Esp32 on Arduino did not have analogWrite() defined. Therefore
// on all esp32 platforms, we define it ourselves. It's hard to detect the idf3.3 so we
// just define analogWrite() for **all** platforms, but leave it as a weak symbol so that
// it auto disables on new platforms.
FL_WEAK void analogWrite(uint8_t pin, int value) {
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



#endif  // ESP32
