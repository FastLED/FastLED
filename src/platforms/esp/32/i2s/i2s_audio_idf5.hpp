#include <iostream>

#include <limits>

#include "fl/iostream.h"
#include "fl/stdint.h"
#include "fl/atomic.h"

#include "i2s_audio.h"

#include <Arduino.h>
#include <stdint.h>
#include <driver/i2s.h>
#include "fl/circular_buffer.h"
#include "fl/printf.h"


#ifndef GPIO_NUM_7
#define GPIO_NUM_7 (7)
#endif

#ifndef GPIO_NUM_8
#define GPIO_NUM_8 (8)
#endif

#ifndef GPIO_NUM_4
#define GPIO_NUM_4 (4)
#endif

#ifndef GPIO_NUM_10
#define GPIO_NUM_10 (10)
#endif

#ifndef I2S_NUM_0
#define I2S_NUM_0 i2s_port_t(0)
#endif




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

namespace
{
  static_assert(AUDIO_BIT_RESOLUTION == 16, "Only 16 bit resolution is supported");
  static_assert(AUDIO_CHANNELS == 1, "Only 1 channel is supported");
  static_assert(sizeof(audio_sample_t) == 2, "audio_sample_t must be 16 bit");
  std::atomic<float> s_loudness_dB;
  std::atomic<uint32_t> s_loudness_updated;
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

  double audio_loudness_to_dB(double rms_loudness)
  {
    // This is a rough approximation of the loudness to dB scale.
    // The data was taken from the following video featuring brown
    // noise: https://www.youtube.com/watch?v=hXetO_bYcMo
    // This linear regression was done on the following data:
    // DB | LOUDNESS
    // ---+---------
    // 50 | 15
    // 55 | 22
    // 60 | 33
    // 65 | 56
    // 70 | 104
    // 75 | 190
    // 80 | 333
    // This will produce an exponential regression of the form:
    //  0.0833 * std::exp(0.119 * x);
    // Below is the inverse exponential regression.
    const float kCoefficient = 0.119f;
    const float kIntercept = 0.0833f;
    const float kInverseCoefficient = 1.0f / kCoefficient; // Maybe faster to precompute this.
    const float kInverseIntercept = 1.0f / kIntercept;
    return std::log(rms_loudness * kInverseIntercept) * kInverseCoefficient;
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

  size_t read_raw_samples(audio_sample_t (&buffer)[IS2_AUDIO_BUFFER_LEN])
  {
    size_t bytes_read = 0;
    i2s_event_t event;

    uint32_t current_time = millis();
    esp_err_t result = i2s_read(I2S_NUM_0, buffer, sizeof(buffer), &bytes_read, 0);
    if (result == ESP_OK)
    {
      if (bytes_read > 0)
      {
        // cout << "Bytes read: " << bytes_read << endl;
        const size_t count = bytes_read / sizeof(audio_sample_t);
        return count;
      }
    }
    return 0;
  }

  bool update_audio_samples(audio_buffer_t* dst)
  {
    // audio_sample_t buffer[IS2_AUDIO_BUFFER_LEN] = {0};
    bool updated = false;
    while (true)
    {
      size_t samples_read = read_raw_samples(*dst);
      if (samples_read <= 0)
      {
        break;
      }
      if (garbage_buffer_count > 0)
      {
        --garbage_buffer_count;
        continue;
      }
      updated = true;
      float rms = calc_rms_loudness(*dst, samples_read);
      s_loudness_dB.store(audio_loudness_to_dB(rms));
      s_loudness_updated.store(millis());
    }
    return updated;
  }

  bool s_audio_initialized = false;
} // anonymous namespace

void audio_task(void *pvParameters)
{
  while (true)
  {
    // Drain out all pending buffers.
    while (update_audio_samples(nullptr))  // FIXME
    {
      ;
    }
    delay_task_ms(7);
  }
}

void audio_init(bool wait_for_power_on)
{
  if (s_audio_initialized)
  {
    cout << "Audio already initialized." << endl;
    return;
  }
  s_audio_initialized = true;
  pinMode(PIN_AUDIO_PWR, OUTPUT);
  digitalWrite(PIN_AUDIO_PWR, HIGH); // Power on the IS2 microphone.
  i2s_audio_init();
  if (wait_for_power_on)
  {
    delay_task_ms(POWER_ON_TIME_MS); // Wait for the microphone to power on.
  }

  // start a task to read the audio samples using psram
  // TaskCreatePsramPinnedToCore(
  //    audio_task, "audio_task", 4096, NULL, AUDIO_TASK_SAMPLING_PRIORITY, NULL, 0);

#if ENABLE_AUDIO_TASK
  xTaskCreatePinnedToCore(
      audio_task, "audio_task", 4096, NULL, AUDIO_TASK_SAMPLING_PRIORITY, NULL, 0);
#endif
}

// UNTESTED
void audio_shutdown()
{
  // i2s_stop(I2S_NUM_0);          // Stop the I2S
  // i2s_driver_uninstall(I2S_NUM_0); // Uninstall the driver
  s_audio_initialized = false;
}

audio_state_t audio_update()
{
  audio_buffer_t buffer = {0};
  uint32_t start_time = millis();
  update_audio_samples(&buffer);
#if ENABLE_AUDIO_TASK
  for (int i = 0; i < 3; i++)
  {
    vPortYield();
  }
#endif
  audio_state_t state = audio_state_t(audio_loudness_dB(), s_loudness_updated.load(), buffer);
  return state;
}

float audio_loudness_dB() { return s_loudness_dB.load(); }

// Audio

void audio_loudness_test()
{
  Buffer<double> sample_buffer;
  sample_buffer.init(32);
  cout << "Done initializing audio buffers" << endl;

  while (true)
  {
    // This is a test to see how loud the audio is.
    // It's not used in the final product.
    audio_sample_t buffer[IS2_AUDIO_BUFFER_LEN] = {0};
    size_t samples_read = read_raw_samples(buffer);
    if (samples_read > 0)
    {
      double rms_loudness = calc_rms_loudness(buffer, samples_read);
      sample_buffer.write(&rms_loudness, 1);
      double avg = 0;
      for (size_t i = 0; i < sample_buffer.size(); ++i)
      {
        avg += sample_buffer[i];
      }
      avg /= sample_buffer.size();
      String loudness_str = String(avg, 3);
      // mymyprintf("Avg rms loudness: %s\n", loudness_str.c_str());
      float dB = audio_loudness_to_dB(avg);
      String dB_str = String(dB, 3);
      PRINTF("dB: %s, loudness: %s\n", dB_str.c_str(), loudness_str.c_str());
      // buffer->clear();
      sample_buffer.clear();
    }
  }
}

void audio_enter_light_sleep()
{
  audio_sample_t buffer[IS2_AUDIO_BUFFER_LEN] = {0};
  // i2s_stop(I2S_NUM_0);          // Stop the I2S
  i2s_audio_shutdown();
  // i2s_zero_dma_buffer(I2S_NUM_0);
  digitalWrite(PIN_AUDIO_PWR, HIGH);
  gpio_hold_en(PIN_I2S_WS);
  gpio_hold_en(PIN_IS2_SD);
  gpio_hold_en(PIN_I2S_SCK);
  gpio_hold_en(PIN_AUDIO_PWR);
}

void audio_exit_light_sleep()
{
  gpio_hold_dis(PIN_I2S_WS);
  gpio_hold_dis(PIN_IS2_SD);
  gpio_hold_dis(PIN_I2S_SCK);
  // gpio_hold_dis(PIN_AUDIO_PWR);
  delay(160);
  // i2s_start(I2S_NUM_0);
  i2s_audio_init();
  // audio_sample_t buffer[IS2_AUDIO_BUFFER_LEN] = {0};
  // uint32_t future_time = millis() + 180;
  // while (millis() < future_time) {
  //   read_raw_samples(buffer);
  // }
  // delay(180);
  // delay(POWER_ON_TIME_MS * 2);
  //  garbage_buffer_count = 16;
}
