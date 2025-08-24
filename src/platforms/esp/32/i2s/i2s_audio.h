


#include "fl/stdint.h"
#include "fl/int.h"


#define IS2_AUDIO_BUFFER_LEN 512

namespace fl {

void i2s_audio_init(bool wait_for_power_on);
void i2s_audio_enter_light_sleep();
void i2s_audio_exit_light_sleep();
void i2s_audio_shutdown();

size_t i2s_read_raw_samples(i16 (&buffer)[IS2_AUDIO_BUFFER_LEN]);

double i2s_loudness_to_rms_imp441(double rms_loudness);

}  // namespace fl
