
#include <limits>
//#include <driver/i2s_std.h>
#include <driver/i2s.h>


#include "i2s_audio.h"


#include "fl/iostream.h"
#include "fl/stdint.h"
#include "fl/atomic.h"

//#include "alloc.h"
// #include "buffer.hpp"
// #include "fl/printf.h"
#include "fl/circular_buffer.h"
// #include "time.h"
//#include <atomic>
// #include <stdio.h>

//#include "alloc.h"


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

// There may be some combination of clock gating/'force PU' settings which could
// work, stopping only the CPU but nothing else. Light sleep however, among
// other things, clock-gates and powers down the internal RAM ("retention
// mode"), so DMA cannot work.

// https://esp32.com/viewtopic.php?t=37242
// Probably the best you can do is to switch the CPU clock down to 80MHz while
// waiting for the I2S transfer. I imagine that the CPU core alone, just waiting
// for an interrupt, doesn't contribute too much to the power consumption while
// RAM and much of the high-speed clocks (APB,...) are running anyway.
// (Anecdotally, on other µC's I found that the PLL would contribute
// significantly to the idle power (like, 100MHz/2 needing more power than
// 50MHz/1), which seems plausible for the S3 too when the PLL is ticking along
// at 480MHz with not much else going on.) With CPU_CLK at 80MHz, the PLL can
// run at 'only' 320MHz; better yet would be to run without the PLL, i.e.
// CPU_CLK = XTAL_CLK = 40MHz, which you could also try.

// During power
#define POWER_ON_TIME_MS                                                       \
    85 // Time to power on the microphone according to the datasheet.
#define POWER_OFF_TIME_MS                                                      \
    85 // Time to power off the microphone is 43 ms but we round up.
       // Note that during power down, no data should be attempted to be read
       // or the ESD diodes will be activated and the microphone will be
       // damaged.

#define AUDIO_BIT_RESOLUTION 16
#define AUDIO_SAMPLE_RATE (44100ul / 1)
#define AUDIO_CHANNELS 1 // Not tested with 2 channels
#define AUDIO_DMA_BUFFER_COUNT 3
#define AUDIO_RECORDING_SECONDS 1

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


namespace fl {

using audio_sample_t = int16_t;

namespace {
static_assert(AUDIO_BIT_RESOLUTION == 16,
              "Only 16 bit resolution is supported");
static_assert(AUDIO_CHANNELS == 1, "Only 1 channel is supported");
static_assert(sizeof(audio_sample_t) == 2, "audio_sample_t must be 16 bit");
fl::atomic<float> s_loudness_dB;
fl::atomic<uint32_t> s_loudness_updated;
int garbage_buffer_count = 0;
bool s_i2s_audio_initialized = false;

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

const i2s_pin_config_t pin_config = {.bck_io_num = PIN_I2S_SCK,
                                     .ws_io_num = PIN_I2S_WS,
                                     .data_out_num = I2S_PIN_NO_CHANGE,
                                     .data_in_num = PIN_IS2_SD};

inline void delay_task_ms(uint32_t ms) { vTaskDelay(ms / portTICK_PERIOD_MS); }

} // anonymous namespace


void i2s_audio_shutdown() {
    // i2s_stop(I2S_NUM_0);
    i2s_driver_uninstall(I2S_NUM_0);
    s_i2s_audio_initialized = false;
}

void i2s_audio_init(bool wait_for_power_on) {
    if (s_i2s_audio_initialized) {
        fl::cout << "Audio already initialized." << fl::endl;
        return;
    }
    s_i2s_audio_initialized = true;
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM, &pin_config);
    i2s_zero_dma_buffer(I2S_NUM_0);
    if (wait_for_power_on) {
        delay_task_ms(POWER_ON_TIME_MS); // Wait for the microphone to power on.
    }
}

size_t i2s_read_raw_samples(audio_sample_t (&buffer)[IS2_AUDIO_BUFFER_LEN]) {
    size_t bytes_read = 0;
    i2s_event_t event;

    esp_err_t result =
        i2s_read(I2S_NUM_0, buffer, sizeof(buffer), &bytes_read, 0);
    if (result == ESP_OK) {
        if (bytes_read > 0) {
            // cout << "Bytes read: " << bytes_read << endl;
            const size_t count = bytes_read / sizeof(audio_sample_t);
            return count;
        }
    }
    return 0;
}

double i2s_loudness_to_rms_imp441(double rms_loudness) {
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
    const float kInverseCoefficient =
        1.0f / kCoefficient; // Maybe faster to precompute this.
    const float kInverseIntercept = 1.0f / kIntercept;
    return std::log(rms_loudness * kInverseIntercept) * kInverseCoefficient;
}

}
