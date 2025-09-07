
#pragma once

#include "fl/warn.h"
#include "fl/audio_input.h"


#include "platforms/esp/esp_version.h"

#ifndef FASTLED_ESP32_I2S_SUPPORTED
    #if defined(CONFIG_IDF_TARGET_ESP32C2)
        #define FASTLED_ESP32_I2S_SUPPORTED 0
    #endif
#endif

#ifndef FASTLED_ESP32_I2S_SUPPORTED
    #if ESP_IDF_VERSION_5_OR_HIGHER
        #define FASTLED_ESP32_I2S_SUPPORTED 1
        #include "platforms/esp/32/audio/devices/idf5_i2s_context.hpp"
    #elif ESP_IDF_VERSION_4_OR_HIGHER
        #define FASTLED_ESP32_I2S_SUPPORTED 1
        #include "platforms/esp/32/audio/devices/idf4_i2s_context.hpp"
    #else
        #define FASTLED_ESP32_I2S_SUPPORTED 0
    #endif
#endif

namespace fl {

#if FASTLED_ESP32_I2S_SUPPORTED

class I2S_Audio : public IAudioInput {
  public:
    using I2SContext = esp_i2s::I2SContext;

    I2S_Audio(const AudioConfigI2S &config)
        : mStdConfig(config), mHasError(false), mTotalSamplesRead(0) {}

    ~I2S_Audio() {}

    void start() override {
        using namespace esp_i2s;
        if (mI2sContextOpt) {
            FL_WARN("I2S channel is already initialized");
            return;
        }
        I2SContext ctx = i2s_audio_init(mStdConfig);
        mI2sContextOpt = ctx;
        mTotalSamplesRead = 0;  // Reset sample counter on start
    }

    void stop() override {
        using namespace esp_i2s;
        if (!mI2sContextOpt) {
            FL_WARN("I2S channel is not initialized");
            return;
        }
        i2s_audio_destroy(*mI2sContextOpt);
        mI2sContextOpt = fl::nullopt;
        mTotalSamplesRead = 0;  // Reset sample counter on stop
    }

    bool error(fl::string *msg = nullptr) override {
        if (msg && mHasError) {
            *msg = mErrorMessage;
        }
        return mHasError;
    }

    AudioSample read() override {
        using namespace esp_i2s;
        if (!mI2sContextOpt) {
            FL_WARN("I2S channel is not initialized");
            return AudioSample();  // Invalid sample
        }
        
        audio_sample_t buf[I2S_AUDIO_BUFFER_LEN];
        const I2SContext &ctx = *mI2sContextOpt;
        size_t samples_read_size = i2s_read_raw_samples(ctx, buf);
        int samples_read = static_cast<int>(samples_read_size);
        
        if (samples_read <= 0) {
            return AudioSample();  // Invalid sample
        }
        
        // Calculate timestamp based on sample rate and total samples read
        fl::u32 timestamp_ms = static_cast<fl::u32>((mTotalSamplesRead * 1000ULL) / mStdConfig.mSampleRate);
        
        // Update total samples counter
        mTotalSamplesRead += samples_read;

        fl::span<const fl::i16> data(buf, samples_read);
        
        // Create AudioSample with pooled AudioSampleImpl (pooling handled internally)
        return AudioSample(data, timestamp_ms);
    }

  private:
    AudioConfigI2S mStdConfig;
    bool mHasError;
    fl::string mErrorMessage;
    fl::optional<I2SContext> mI2sContextOpt;
    fl::u64 mTotalSamplesRead;
};

#endif // FASTLED_ESP32_I2S_SUPPORTED


} // namespace fl
