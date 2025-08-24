

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
#include "fl/variant.h"
#include "fl/shared_ptr.h"


#define IS2_AUDIO_BUFFER_LEN 512

namespace fl {

enum MicChannel {
    MicChannelL = 0,
    MicChannelR = 1,
};

struct I2SStandardConfig {
    int mPinWs;
    int mPinSd;
    int mPinClk;
    int mI2sNum;
    bool mInvert = false;
    MicChannel mMicChannel;
    I2SStandardConfig(int pin_ws, int pin_sd, int pin_clk, int i2s_num, MicChannel mic_channel)
        : mPinWs(pin_ws), mPinSd(pin_sd), mPinClk(pin_clk), mI2sNum(i2s_num), mMicChannel(mic_channel) {}
};

struct I2SPdmConfig {
    int mPinDin;
    int mPinClk;
    int mI2sNum;
    bool mInvert = false;
    I2SPdmConfig(int pin_din, int pin_clk, int i2s_num, bool invert = false)
        : mPinDin(pin_din), mPinClk(pin_clk), mI2sNum(i2s_num), mInvert(invert) {}
};

using I2SConfig = fl::Variant<I2SStandardConfig, I2SPdmConfig>;



class IEspI2SAudioSource {
public:
    // This is the single factory function for creating the audio source. If the creation was successful, then
    // the return value will be non-null. If the creation was not successful, then the return value will be null
    // and the error_message will be set to a non-empty string.
    // Keep in mind that the I2SConfig is a variant type. Many esp types do not support all the types in the variant.
    // For example, the I2SPdmConfig is not supported on the ESP32-C3 and in this case it will return a null pointer
    // and the error_message will be set to a non-empty string.
    // Implimentation notes:
    //   It's very important that the implimentation uses a esp task to fill in the buffer. The reason is that
    //   there will be looooong delays during FastLED show() on some esp platforms, for example idf 4.4. If we 
    static fl::shared_ptr<IEspI2SAudioSource> create(const I2SConfig& config, fl::string* error_message = nullptr);


    virtual ~IEspI2SAudioSource() = default;
    virtual void init() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void read(fl::vector_inlined<i16, IS2_AUDIO_BUFFER_LEN>* buffer) = 0;
};

// void i2s_audio_init(const I2SConfig& config);


// // Worked in previous life but needs re-validation before becoming part of this api.
// // HELP WANTED!
// //void i2s_audio_enter_light_sleep();
// //void i2s_audio_exit_light_sleep();
// //void i2s_audio_shutdown();

// size_t i2s_read_raw_samples(fl::vector_inlined<i16, IS2_AUDIO_BUFFER_LEN>* buffer);
// double i2s_loudness_to_rms_imp441(double rms_loudness);

}  // namespace fl
