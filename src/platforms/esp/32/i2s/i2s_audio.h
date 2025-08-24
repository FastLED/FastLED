

/// BETA DRIVER FOR THE IMP441 MICROPHONE
/// THIS IS NOT FINISHED YET
/// right now the pins are hard coded to
/// this driver will be considered "done" when all the pins are configurable.

/// WordSelect pin is GPIO_NUM_7
/// SerialData pin is GPIO_NUM_8
/// SerialClock pin is GPIO_NUM_4
/*
#define PIN_I2S_WS GPIO_NUM_7  // TODO change this pins
#define PIN_IS2_SD GPIO_NUM_8  // TODO change this pins
#define PIN_I2S_SCK GPIO_NUM_4  // TODO change this pins
*/


#include "fl/stdint.h"
#include "fl/int.h"
#include "fl/vector.h"


#define IS2_AUDIO_BUFFER_LEN 512

namespace fl {

struct I2SAudioConfig {
    bool wait_for_power_on = false;
    int mPinWs;
    int mPinSd;
    int mPinClk;
    int mI2sNum;
    I2SAudioConfig(int pin_ws, int pin_sd, int pin_clk, int i2s_num)
        : mPinWs(pin_ws), mPinSd(pin_sd), mPinClk(pin_clk), mI2sNum(i2s_num) {}
};

void i2s_audio_init(const I2SAudioConfig& config);


// Worked in previous life but needs re-validation before becoming part of this api.
// HELP WANTED!
//void i2s_audio_enter_light_sleep();
//void i2s_audio_exit_light_sleep();
//void i2s_audio_shutdown();

size_t i2s_read_raw_samples(fl::vector_inlined<i16, IS2_AUDIO_BUFFER_LEN>* buffer);
double i2s_loudness_to_rms_imp441(double rms_loudness);

}  // namespace fl
