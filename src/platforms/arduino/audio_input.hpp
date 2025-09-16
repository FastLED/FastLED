#pragma once

#include "fl/audio_input.h"
#include "fl/warn.h"
#include "fl/assert.h"
#include "fl/vector.h"
#include "fl/span.h"
#include "fl/shared_ptr.h"
#include "fl/has_include.h"
#include "fl/unused.h"

#if !FL_HAS_INCLUDE(<Arduino.h>)
  #error "This implementation requires Arduino.h - compile with Arduino framework"
#endif


#include <Arduino.h>  // ok include

// Check for Arduino I2S library availability and completeness
#if defined(__SAMD21G18A__) || defined(__SAMD21J18A__) || defined(__SAMD21E17A__) || defined(__SAMD21E18A__)
#define ARDUINO_I2S_FULLY_SUPPORTED 0
#define ARDUINO_I2S_BROKEN_REASON "I2S not supported on SAMD21"
#elif FL_HAS_INCLUDE(<I2S.h>)
#include <I2S.h>

// Define ARDUINO_I2S_FULLY_SUPPORTED only when ALL I2S components are present and functional
#if defined(ARDUINO_UNOR4_WIFI) || defined(ARDUINO_UNOR4_MINIMA) || \
    defined(ARDUINO_ARCH_RENESAS) || defined(ARDUINO_ARCH_RENESAS_UNO) || \
    defined(_RENESAS_RA_) || defined(ARDUINO_FSP)
    // Known broken: Renesas RA platforms with incomplete FSP I2S support
    #define ARDUINO_I2S_FULLY_SUPPORTED 0
    #define ARDUINO_I2S_BROKEN_REASON "Renesas FSP missing r_i2s_api.h header"
#elif !defined(I2S_PHILIPS_MODE) || !defined(I2S_LEFT_JUSTIFIED_MODE) || !defined(I2S_RIGHT_JUSTIFIED_MODE)
    // Missing essential I2S mode constants - incomplete library implementation
    #define ARDUINO_I2S_FULLY_SUPPORTED 0
    #define ARDUINO_I2S_BROKEN_REASON "Missing I2S mode constants (incomplete library)"
#else
    // All required I2S components are present and platform is not known-broken
    #define ARDUINO_I2S_FULLY_SUPPORTED 1
#endif

#else
// No I2S.h header found - cannot support I2S audio input
#define ARDUINO_I2S_FULLY_SUPPORTED 0
#define ARDUINO_I2S_BROKEN_REASON "I2S.h header not available"
#endif

// Legacy compatibility define
#define ARDUINO_I2S_SUPPORTED ARDUINO_I2S_FULLY_SUPPORTED

namespace fl {

#if ARDUINO_I2S_FULLY_SUPPORTED

class Arduino_I2S_Audio : public IAudioInput {
public:
    Arduino_I2S_Audio(const AudioConfigI2S &config)
        : mConfig(config), mHasError(false), mTotalSamplesRead(0), mInitialized(false) {}

    ~Arduino_I2S_Audio() {
        stop();
    }

    void start() override {
        if (mInitialized) {
            FL_WARN("Arduino I2S is already initialized");
            return;
        }

        // Initialize Arduino I2S library using standard API
        // WARNING: Arduino I2S uses board-specific pins, not the pins from mConfig!
        // For Arduino Zero: WS=0, CLK=1, SD=9
        // For MKR boards: WS=3, CLK=2, SD=A6
        // Pin configuration in mConfig is ignored on Arduino platforms
        int i2s_mode = convertCommFormatToMode(mConfig.mCommFormat);
        bool success = I2S.begin(
            i2s_mode,
            static_cast<long>(mConfig.mSampleRate),
            static_cast<int>(mConfig.mBitResolution)
        );

        if (!success) {
            mHasError = true;
            mErrorMessage = "Failed to initialize Arduino I2S";
            FL_WARN(mErrorMessage.c_str());
            return;
        }

        mInitialized = true;
        mTotalSamplesRead = 0;
        FL_WARN("Arduino I2S audio input started successfully");
    }

    void stop() override {
        if (!mInitialized) {
            return;
        }

        I2S.end();
        mInitialized = false;
        mTotalSamplesRead = 0;
        FL_WARN("Arduino I2S audio input stopped");
    }

    bool error(fl::string *msg = nullptr) override {
        if (msg && mHasError) {
            *msg = mErrorMessage;
        }
        return mHasError;
    }

    AudioSample read() override {
        if (!mInitialized) {
            FL_WARN("Arduino I2S is not initialized");
            return AudioSample();  // Invalid sample
        }

        fl::i16 buffer[I2S_AUDIO_BUFFER_LEN];
        size_t samples_read = 0;

        // Read samples using standard Arduino I2S API
        int available = I2S.available();
        if (available <= 0) {
            return AudioSample();  // No data available
        }

        size_t bytes_to_read = fl::min(static_cast<size_t>(available),
                                      sizeof(buffer));

        int bytes_read = I2S.read(buffer, bytes_to_read);
        if (bytes_read <= 0) {
            return AudioSample();  // No data read
        }

        samples_read = bytes_read / sizeof(fl::i16);

        if (samples_read == 0) {
            return AudioSample();  // No samples read
        }

        // Handle channel selection for mono output
        if (mConfig.mAudioChannel == AudioChannel::Left || mConfig.mAudioChannel == AudioChannel::Right) {
            // For stereo input with single channel selection, extract the desired channel
            size_t mono_samples = samples_read / 2;
            size_t channel_offset = (mConfig.mAudioChannel == AudioChannel::Right) ? 1 : 0;

            for (size_t i = 0; i < mono_samples; i++) {
                buffer[i] = buffer[i * 2 + channel_offset];
            }
            samples_read = mono_samples;
        }

        // Apply inversion if requested
        if (mConfig.mInvert) {
            for (size_t i = 0; i < samples_read; i++) {
                buffer[i] = -buffer[i];
            }
        }

        // Calculate timestamp based on sample rate and total samples read
        fl::u32 timestamp_ms = static_cast<fl::u32>((mTotalSamplesRead * 1000ULL) / mConfig.mSampleRate);

        // Update total samples counter
        mTotalSamplesRead += samples_read;

        fl::span<const fl::i16> data(buffer, samples_read);
        return AudioSample(data, timestamp_ms);
    }

private:
    AudioConfigI2S mConfig;
    bool mHasError;
    fl::string mErrorMessage;
    bool mInitialized;
    fl::u64 mTotalSamplesRead;

    int convertCommFormatToMode(I2SCommFormat format) {
        // Map FastLED I2S formats to Arduino I2S modes
        switch (format) {
            case I2SCommFormat::Philips:
                return I2S_PHILIPS_MODE;
            case I2SCommFormat::MSB:
                return I2S_LEFT_JUSTIFIED_MODE;
            case I2SCommFormat::PCMShort:
            case I2SCommFormat::PCMLong:
                return I2S_RIGHT_JUSTIFIED_MODE;
            default:
                return I2S_PHILIPS_MODE;  // Default to Philips standard
        }
    }
};

// Platform-specific audio input creation function for Arduino
fl::shared_ptr<IAudioInput> arduino_create_audio_input(const AudioConfig& config, fl::string* error_message = nullptr) {
    if (config.is<AudioConfigI2S>()) {
        FL_WARN("Creating Arduino I2S audio source");
        AudioConfigI2S i2s_config = config.get<AudioConfigI2S>();
        return fl::make_shared<Arduino_I2S_Audio>(i2s_config);
    } else if (config.is<AudioConfigPdm>()) {
        const char* ERROR_MESSAGE = "PDM audio not supported in Arduino I2S implementation";
        FL_WARN(ERROR_MESSAGE);
        if (error_message) {
            *error_message = ERROR_MESSAGE;
        }
        return fl::shared_ptr<IAudioInput>();  // Return null
    }

    const char* ERROR_MESSAGE = "Unsupported audio configuration for Arduino";
    FL_WARN(ERROR_MESSAGE);
    if (error_message) {
        *error_message = ERROR_MESSAGE;
    }
    return fl::shared_ptr<IAudioInput>();  // Return null
}

#else // !ARDUINO_I2S_FULLY_SUPPORTED

// Null audio implementation fallback for platforms without complete I2S support
fl::shared_ptr<IAudioInput> arduino_create_audio_input(const AudioConfig& config, fl::string* error_message = nullptr) {
    FL_UNUSED(config);
#ifdef ARDUINO_I2S_BROKEN_REASON
    const char* ERROR_MESSAGE = "Arduino I2S not supported: " ARDUINO_I2S_BROKEN_REASON;
#else
    const char* ERROR_MESSAGE = "Arduino I2S library not available - please install I2S library";
#endif
    FL_WARN(ERROR_MESSAGE);
    if (error_message) {
        *error_message = ERROR_MESSAGE;
    }
    return fl::shared_ptr<IAudioInput>();  // Return null
}

#endif // ARDUINO_I2S_FULLY_SUPPORTED

} // namespace fl
