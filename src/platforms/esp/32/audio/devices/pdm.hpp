
#pragma once

// IWYU pragma: private

#include "platforms/esp/is_esp.h"

#include "fl/audio/audio_input.h"
#include "fl/log/log.h"

#include "platforms/esp/esp_version.h"

// Check if this ESP32 variant supports PDM RX
#ifndef FASTLED_ESP32_PDM_SUPPORTED
#if defined(FL_IS_ESP_32C2)
// ESP32-C2 has no I2S at all
#define FASTLED_ESP32_PDM_SUPPORTED 0
#endif
#endif

#ifndef FASTLED_ESP32_PDM_SUPPORTED
// IWYU pragma: begin_keep
#include "soc/soc_caps.h"
// IWYU pragma: end_keep
#if defined(SOC_I2S_SUPPORTS_PDM_RX) && SOC_I2S_SUPPORTS_PDM_RX
#if ESP_IDF_VERSION_5_OR_HIGHER
#define FASTLED_ESP32_PDM_SUPPORTED 1
#include "platforms/esp/32/audio/devices/idf5_pdm_context.hpp"
#elif ESP_IDF_VERSION_4_OR_HIGHER
#define FASTLED_ESP32_PDM_SUPPORTED 1
#include "platforms/esp/32/audio/devices/idf4_pdm_context.hpp"
#else
#define FASTLED_ESP32_PDM_SUPPORTED 0
#endif
#else
#define FASTLED_ESP32_PDM_SUPPORTED 0
#endif
#endif

namespace fl {

#if FASTLED_ESP32_PDM_SUPPORTED

class PDM_Audio : public audio::IInput {
  public:
    using PDMContext = esp_pdm::PDMContext;

    PDM_Audio(const audio::ConfigPdm &config)
        : mPdmConfig(config), mHasError(false), mTotalSamplesRead(0) {}

    ~PDM_Audio() { stop(); }

    void start() FL_NOEXCEPT override {
        if (mPdmContextOpt) {
            FL_WARN("PDM channel is already initialized");
            return;
        }
        esp_pdm::PDMContext ctx = esp_pdm::pdm_audio_init(mPdmConfig);
        if (!ctx.valid) {
            mHasError = true;
            mErrorMessage = "Failed to initialize PDM audio";
            return;
        }
        mPdmContextOpt = ctx;
        mTotalSamplesRead = 0;
    }

    void stop() FL_NOEXCEPT override {
        if (!mPdmContextOpt) {
            return;
        }
        esp_pdm::pdm_audio_destroy(*mPdmContextOpt);
        mPdmContextOpt = fl::nullopt;
        mTotalSamplesRead = 0;
    }

    bool error(fl::string *msg = nullptr) FL_NOEXCEPT override {
        if (msg && mHasError) {
            *msg = mErrorMessage;
        }
        return mHasError;
    }

    audio::Sample read() FL_NOEXCEPT override {
        if (!mPdmContextOpt) {
            FL_WARN("PDM channel is not initialized");
            return audio::Sample();
        }

        esp_pdm::audio_sample_t buf[I2S_AUDIO_BUFFER_LEN];
        const esp_pdm::PDMContext &ctx = *mPdmContextOpt;
        size_t samples_read_size = esp_pdm::pdm_read_raw_samples(ctx, buf);
        int samples_read = static_cast<int>(samples_read_size);

        if (samples_read <= 0) {
            return audio::Sample();
        }

        // Invert signal if configured
        if (mPdmConfig.mInvert) {
            for (int i = 0; i < samples_read; ++i) {
                buf[i] = -buf[i];
            }
        }

        u32 timestamp_ms = static_cast<u32>((mTotalSamplesRead * 1000ULL) /
                                            mPdmConfig.mSampleRate);
        mTotalSamplesRead += samples_read;

        fl::span<const i16> data(buf, samples_read);
        return audio::Sample(data, timestamp_ms);
    }

  private:
    audio::ConfigPdm mPdmConfig;
    bool mHasError;
    fl::string mErrorMessage;
    fl::optional<PDMContext> mPdmContextOpt;
    u64 mTotalSamplesRead;
};

#endif // FASTLED_ESP32_PDM_SUPPORTED

} // namespace fl
