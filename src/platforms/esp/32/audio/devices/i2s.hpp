
#pragma once

// IWYU pragma: private

#include "platforms/esp/is_esp.h"

#include "fl/log/log.h"
#include "fl/audio/audio_input.h"


#include "platforms/esp/esp_version.h"

#ifndef FASTLED_ESP32_I2S_SUPPORTED
    #if defined(FL_IS_ESP_32C2)
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
#include "fl/stl/noexcept.h"
    #else
        #define FASTLED_ESP32_I2S_SUPPORTED 0
    #endif
#endif

namespace fl {

#if FASTLED_ESP32_I2S_SUPPORTED

class I2S_Audio : public audio::IInput {
  public:
    using I2SContext = esp_i2s::I2SContext;

    I2S_Audio(const audio::ConfigI2S &config)
        : mStdConfig(config), mHasError(false), mTotalSamplesRead(0) {}

    ~I2S_Audio() { stop(); }

    void start() FL_NOEXCEPT override {
        if (mI2sContextOpt) {
            FL_WARN("I2S channel is already initialized");
            return;
        }
        esp_i2s::I2SContext ctx = esp_i2s::i2s_audio_init(mStdConfig);
        mI2sContextOpt = ctx;
        mTotalSamplesRead = 0;  // Reset sample counter on start
    }

    void stop() FL_NOEXCEPT override {
        if (!mI2sContextOpt) {
            return;
        }
        esp_i2s::i2s_audio_destroy(*mI2sContextOpt);
        mI2sContextOpt = fl::nullopt;
        mTotalSamplesRead = 0;  // Reset sample counter on stop
    }

    bool error(fl::string *msg = nullptr) FL_NOEXCEPT override {
        if (msg && mHasError) {
            *msg = mErrorMessage;
        }
        return mHasError;
    }

    audio::Sample read() FL_NOEXCEPT override {
        if (!mI2sContextOpt) {
            FL_WARN("I2S channel is not initialized");
            return audio::Sample();  // Invalid sample
        }

        esp_i2s::audio_sample_t buf[I2S_AUDIO_BUFFER_LEN];
        const esp_i2s::I2SContext &ctx = *mI2sContextOpt;
        size_t samples_read_size = esp_i2s::i2s_read_raw_samples(ctx, buf);
        int samples_read = static_cast<int>(samples_read_size);
        
        if (samples_read <= 0) {
            return audio::Sample();  // Invalid sample
        }
        
        // Calculate timestamp based on sample rate and total samples read
        u32 timestamp_ms = static_cast<u32>((mTotalSamplesRead * 1000ULL) / mStdConfig.mSampleRate);
        
        // Update total samples counter
        mTotalSamplesRead += samples_read;

        fl::span<const i16> data(buf, samples_read);
        
        // Create audio::Sample with pooled audio::SampleImpl (pooling handled internally)
        return audio::Sample(data, timestamp_ms);
    }

  private:
    audio::ConfigI2S mStdConfig;
    bool mHasError;
    fl::string mErrorMessage;
    fl::optional<I2SContext> mI2sContextOpt;
    u64 mTotalSamplesRead;
};

#endif // FASTLED_ESP32_I2S_SUPPORTED


} // namespace fl
