# Testing

  * Esp32 testing
    * https://github.com/marketplace/actions/esp32-qemu-runner will run a sketch for X seconds and see's if it crashes
      * There's specific tests we'd like to run with this including the WS2812 and APA102 tests to test the clockless and clocked drivers

# Feature Enhancements

  * I2S driver for ESP32 WS2812
    * https://github.com/hpwit/I2SClocklessLedDriver
      * Our copy is here: https://github.com/FastLED/FastLED/blob/master/src/platforms/esp/32/clockless_i2s_esp32.h
    * Apparently, this driver allows MASSIVE parallelization for WS2812
    * Timing guide for reducing RMT frequency https://github.com/Makuna/NeoPixelBus/pull/795
    * ESp32 LED guide
      * web: https://components.espressif.com/components/espressif/led_strip
      * repo: https://github.com/espressif/idf-extra-components/tree/60c14263f3b69ac6e98ecae79beecbe5c18d5596/led_strip
      * adafruit conversation on RMT progress: https://github.com/adafruit/Adafruit_NeoPixel/issues/375


## RMT driver for esp-idf 5.1

Porting notes

It looks like the 5.1 built in driver is going to be the easiest to get
working. There's a sync manager designed for running all the RMT drivers
together. This should be done on the onEndShow() call to the ClockLess
driver.

Good sources of info:
  * esp32 forum: https://esp32.com/viewtopic.php?f=13&t=41170&p=135831&hilit=RMT#p135831
  * esp32 example: https://components.espressif.com/components/espressif/led_strip

*Random code contribution*
```C++
esp_err_t ledSetup()
{
   
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_RMT_TX_GPIO,        
        .max_leds = LED_MAX_LENGTH,              
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,            
        .flags.invert_out = false,              
    };
 
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,    
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = true,            
    };
    ESP_RETURN_ON_ERROR(led_strip_new_rmt_device(&strip_config, &rmt_config, &h_led_strip),
                        TAG, "failed to init RMT device");
 
    return ESP_OK;
}
```