#include <iostream>

#include <limits>

#include "fl/iostream.h"
#include "fl/stdint.h"
#include "fl/atomic.h"

#include "i2s_audio.h"

// #include <Arduino.h>
// #include <stdint.h>
#include "fl/stdint.h"
#include <driver/i2s.h>
#include "fl/circular_buffer.h"
#include "fl/printf.h"
#include "fl/int.h"
#include "driver/gpio.h"




#ifndef GPIO_NUM_7
#define GPIO_NUM_7 gpio_num_t(7)
#endif

#ifndef GPIO_NUM_8
#define GPIO_NUM_8 gpio_num_t(8)
#endif

#ifndef GPIO_NUM_4
#define GPIO_NUM_4 gpio_num_t(4)
#endif

#ifndef GPIO_NUM_10
#define GPIO_NUM_10 gpio_num_t(10)
#endif

#ifndef I2S_NUM_0
#define I2S_NUM_0 i2s_port_t(0)
#endif

#define AUDIO_BIT_RESOLUTION 16



#define PIN_I2S_WS GPIO_NUM_7  // TODO change this pins
#define PIN_IS2_SD GPIO_NUM_8  // TODO change this pins
#define PIN_I2S_SCK GPIO_NUM_4  // TODO change this pins
#define I2S_NUM I2S_NUM_0




#define ENABLE_AUDIO_TASK 0

#define AUDIO_TASK_SAMPLING_PRIORITY 7

#define AUDIO_BUFFER_SAMPLES (AUDIO_RECORDING_SECONDS * AUDIO_SAMPLE_RATE * AUDIO_CHANNELS)

// During power
#define POWER_ON_TIME_MS 85  // Time to power on the microphone according to the datasheet.
#define POWER_OFF_TIME_MS 85 // Time to power off the microphone is 43 ms but we round up.
                             // Note that during power down, no data should be attempted to be read
                             // or the ESD diodes will be activated and the microphone will be damaged.

// TODO: Use static buffers for receiving DMA audio data
// DMA_ATTR uint8_t buffer[]="I want to send something";
// https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/memory-types.html

// I2S NOTES:

// Tutorial on how to use ULP to write out data to DAC.
// https://www.youtube.com/watch?v=6PGrsZmYAJ0

// Forum post discussing I2S during light sleep
// https://esp32.com/viewtopic.php?t=30649

// esp32c3 techinical reference manual for I2S
// https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32c3/api-reference/peripherals/i2s.html

// There may be some combination of clock gating/'force PU' settings which could work, stopping only the CPU but nothing else. Light sleep however, among other things, clock-gates and powers down the internal RAM ("retention mode"), so DMA cannot work.


// https://esp32.com/viewtopic.php?t=37242
// Probably the best you can do is to switch the CPU clock down to 80MHz while waiting for the I2S transfer.
// I imagine that the CPU core alone, just waiting for an interrupt, doesn't contribute too much to the power
// consumption while RAM and much of the high-speed clocks (APB,...) are running anyway. (Anecdotally, on
// other µC's I found that the PLL would contribute significantly to the idle power (like, 100MHz/2 needing
// more power than 50MHz/1), which seems plausible for the S3 too when the PLL is ticking along at 480MHz with
// not much else going on.) With CPU_CLK at 80MHz, the PLL can run at 'only' 320MHz; better yet would be to
// run without the PLL, i.e. CPU_CLK = XTAL_CLK = 40MHz, which you could also try.

namespace fl {

namespace
{
  enum {
    AUDIO_CHANNELS = 1,
    AUDIO_SAMPLE_RATE = 44100,
    AUDIO_DMA_BUFFER_COUNT = 8,
  };
  using audio_sample_t = fl::i16;
  static_assert(AUDIO_BIT_RESOLUTION == 16, "Only 16 bit resolution is supported");
  static_assert(AUDIO_CHANNELS == 1, "Only 1 channel is supported");
  static_assert(sizeof(audio_sample_t) == 2, "audio_sample_t must be 16 bit");
  int garbage_buffer_count = 0;

  const i2s_config_t i2s_config = {
      .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = AUDIO_SAMPLE_RATE,
      .bits_per_sample = i2s_bits_per_sample_t(AUDIO_BIT_RESOLUTION),
      .channel_format = i2s_channel_fmt_t(I2S_CHANNEL_FMT_ONLY_RIGHT),
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
      .intr_alloc_flags = 0,
      .dma_buf_count = AUDIO_DMA_BUFFER_COUNT,
      .dma_buf_len = IS2_AUDIO_BUFFER_LEN,
      //.use_apll = true
      // .tx_desc_auto_clear ?
  };

  const i2s_pin_config_t pin_config = {
      .bck_io_num = PIN_I2S_SCK,
      .ws_io_num = PIN_I2S_WS,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = PIN_IS2_SD};

  void i2s_audio_init()
  {

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM, &pin_config);
    i2s_zero_dma_buffer(I2S_NUM_0);
    // i2s_start(I2S_NUM_0);
    // GPIO_8 needs to have internal pullup enabled.
    //gpio_pullup_en(GPIO_NUM_8);
  }

  void i2s_audio_shutdown()
  {
    // i2s_stop(I2S_NUM_0);
    i2s_driver_uninstall(I2S_NUM_0);
  }



  float calc_rms_loudness(const audio_sample_t *samples, size_t num_samples)
  {
    uint64_t sum_of_squares = 0;
    for (size_t i = 0; i < num_samples; ++i)
    {
      sum_of_squares += samples[i] * samples[i];
    }
    double mean_square = static_cast<double>(sum_of_squares) / num_samples;
    return static_cast<float>(std::sqrt(mean_square));
  }



  bool s_audio_initialized = false;
} // anonymous namespace
}  // namespace fl
