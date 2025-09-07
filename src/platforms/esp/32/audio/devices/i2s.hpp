
#pragma once

#include "fl/vector.h"
#include "fl/warn.h"

#include "platforms/esp/32/audio/audio.h"

#include "platforms/esp/esp_version.h"

#if ESP_IDF_VERSION_5_OR_HIGHER
#include "platforms/esp/32/audio/devices/idf5_i2s_context.hpp"
#elif ESP_IDF_VERSION_4_OR_HIGHER
#include "platforms/esp/32/audio/devices/idf4_i2s_context.hpp"
#else
#error "This should not be reachable when using ESP-IDF < 4.0"
#endif  //

namespace fl {

class I2S_Audio : public IAudioInput {
  public:
    using I2SContext = esp_i2s::I2SContext;

    I2S_Audio(const AudioConfigI2S &config)
        : mStdConfig(config), mHasError(false) {}

    ~I2S_Audio() {}


    void init() override {}

    void start() override {
        using namespace esp_i2s;
        if (mI2sContextOpt) {
            FL_WARN("I2S channel is already initialized");
            return;
        }
        mI2sContextOpt = i2s_audio_init(mStdConfig);
    }

    void stop() override {
        using namespace esp_i2s;
        if (!mI2sContextOpt) {
            FL_WARN("I2S channel is not initialized");
            return;
        }
        i2s_audio_destroy(*mI2sContextOpt);
        mI2sContextOpt = fl::nullopt;
    }

    bool error(fl::string *msg = nullptr) override {
        if (msg && mHasError) {
            *msg = mErrorMessage;
        }
        return mHasError;
    }

    int read(fl::vector_inlined<i16, I2S_AUDIO_BUFFER_LEN> *buffer) override {
        using namespace esp_i2s;
        if (!mI2sContextOpt) {
            buffer->clear();
            FL_WARN("I2S channel is not initialized");
            return -1;
        }
        audio_sample_t buf[I2S_AUDIO_BUFFER_LEN];
        I2SContext &ctx = *mI2sContextOpt;
        int out = i2s_read_raw_samples(ctx, buf);
        if (out < 0) {
            return out;
        }
        if (out > 0) {
            buffer->assign(buf, buf + out);
            return static_cast<int>(buffer->size());
        }
        return 0;
    }

  private:
    AudioConfigI2S mStdConfig;
    bool mHasError;
    fl::string mErrorMessage;
    I2SContext mI2sContext;
    fl::optional<I2SContext> mI2sContextOpt;
};

} // namespace fl
