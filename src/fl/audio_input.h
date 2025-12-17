#pragma once


#pragma once

#include "fl/stl/stdint.h"
#include "fl/int.h"
#include "fl/stl/vector.h"
#include "fl/stl/variant.h"
#include "fl/stl/shared_ptr.h"
#include "fl/audio.h"
#include "fl/compiler_control.h"
#include "platforms/audio.h"

#ifndef FASTLED_HAS_AUDIO_INPUT
#error "platforms/audio.h must define FASTLED_HAS_AUDIO_INPUT"
#endif


#define I2S_AUDIO_BUFFER_LEN 512
#define AUDIO_DEFAULT_SAMPLE_RATE 44100ul
#define AUDIO_DEFAULT_BIT_RESOLUTION 16
#define AUDIO_DMA_BUFFER_COUNT 8

namespace fl {

// Note: Right now these are esp specific, but they are designed to migrate to a common api.

enum AudioChannel {
    Left = 0,
    Right = 1,
    Both = 2,  // Two microphones can be used to capture both channels with one AudioSource.
};


enum I2SCommFormat {
    Philips = 0X01,  // I2S communication I2S Philips standard, data launch at second BCK
    MSB = 0X02,  // I2S communication MSB alignment standard, data launch at first BCK
    PCMShort = 0x04,  // PCM Short standard, also known as DSP mode. The period of synchronization signal (WS) is 1 bck cycle.
    PCMLong = 0x0C,  // PCM Long standard. The period of synchronization signal (WS) is channel_bit*bck cycles.
    Max = 0x0F,  // standard max
};

struct AudioConfigI2S {
    int mPinWs;
    int mPinSd;
    int mPinClk;
    int mI2sNum;
    AudioChannel mAudioChannel;
    u16 mSampleRate;
    u8 mBitResolution;
    I2SCommFormat mCommFormat;
    bool mInvert;
    AudioConfigI2S(
        int pin_ws,
        int pin_sd,
        int pin_clk,
        int i2s_num,
        AudioChannel mic_channel,
        u16 sample_rate,
        u8 bit_resolution,
        I2SCommFormat comm_format = Philips,
        bool invert = false
    )
        : mPinWs(pin_ws), mPinSd(pin_sd), mPinClk(pin_clk),
          mI2sNum(i2s_num), mAudioChannel(mic_channel),
          mSampleRate(sample_rate), mBitResolution(bit_resolution), mCommFormat(comm_format), mInvert(invert) {}
};

struct AudioConfigPdm {
    int mPinDin;
    int mPinClk;
    int mI2sNum;
    u16 mSampleRate;
    bool mInvert = false;

    AudioConfigPdm(int pin_din, int pin_clk, int i2s_num, u16 sample_rate = AUDIO_DEFAULT_SAMPLE_RATE, bool invert = false)
        : mPinDin(pin_din), mPinClk(pin_clk), mI2sNum(i2s_num), mSampleRate(sample_rate), mInvert(invert) {}
};

// Teensy Audio Library configuration helpers
// Note: Teensy uses FIXED hardware pins that cannot be changed in software.
// Pin assignments vary by board model - see Teensy Audio Library documentation.
//
// Teensy 3.x I2S pins (only I2S1 available):
//   BCLK=9, MCLK=11, RX=13, LRCLK=23
//
// Teensy 4.x I2S1 pins:
//   BCLK=21, MCLK=23, RX=8, LRCLK=20
//
// Teensy 4.x I2S2 pins:
//   BCLK=4, MCLK=33, RX=5, LRCLK=3
namespace TeensyI2S {
    enum I2SPort {
        I2S1 = 0,  // Primary I2S (available on all Teensy 3.x and 4.x)
        I2S2 = 1   // Secondary I2S (Teensy 4.x only)
    };

    // Get LRCLK (WS) pin for given I2S port
    constexpr int getPinWS(I2SPort port) {
#if defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
        // Teensy 3.x - only I2S1 available
        return (port == I2S1) ? 23 : -1;
#elif defined(__IMXRT1062__) || defined(__IMXRT1052__)
        // Teensy 4.x - I2S1 and I2S2 available
        return (port == I2S1) ? 20 : 3;
#else
        // Unknown platform - evaluate parameter to avoid unused warning
        return (port == I2S1 || port == I2S2) ? -1 : -1;
#endif
    }

    // Get RX (SD) pin for given I2S port
    constexpr int getPinSD(I2SPort port) {
#if defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
        // Teensy 3.x
        return (port == I2S1) ? 13 : -1;
#elif defined(__IMXRT1062__) || defined(__IMXRT1052__)
        // Teensy 4.x
        return (port == I2S1) ? 8 : 5;
#else
        // Unknown platform - evaluate parameter to avoid unused warning
        return (port == I2S1 || port == I2S2) ? -1 : -1;
#endif
    }

    // Get BCLK pin for given I2S port
    constexpr int getPinCLK(I2SPort port) {
#if defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
        // Teensy 3.x
        return (port == I2S1) ? 9 : -1;
#elif defined(__IMXRT1062__) || defined(__IMXRT1052__)
        // Teensy 4.x
        return (port == I2S1) ? 21 : 4;
#else
        // Unknown platform - evaluate parameter to avoid unused warning
        return (port == I2S1 || port == I2S2) ? -1 : -1;
#endif
    }
}


class AudioConfig : public fl::variant<AudioConfigI2S, AudioConfigPdm> {
public:
    // The most common microphone on Amazon as of 2025-September.
    static AudioConfig CreateInmp441(int pin_ws, int pin_sd, int pin_clk, AudioChannel channel, u16 sample_rate = 44100ul, int i2s_num = 0) {
        AudioConfigI2S config(pin_ws, pin_sd, pin_clk, i2s_num, channel, sample_rate, 16);
        return AudioConfig(config);
    }

    // Factory method for Teensy I2S microphones (INMP441, ICS43432, SPH0645LM4H, etc.)
    // Teensy uses fixed hardware pins - see TeensyI2S namespace for pin assignments.
    // Example: auto config = AudioConfig::CreateTeensyI2S(TeensyI2S::I2S1, Right, 44100);
    static AudioConfig CreateTeensyI2S(
        TeensyI2S::I2SPort port = TeensyI2S::I2S1,
        AudioChannel channel = Right,
        u16 sample_rate = AUDIO_DEFAULT_SAMPLE_RATE,
        u8 bit_resolution = AUDIO_DEFAULT_BIT_RESOLUTION
    ) {
        AudioConfigI2S config(
            TeensyI2S::getPinWS(port),     // pin_ws (LRCLK)
            TeensyI2S::getPinSD(port),     // pin_sd (RX)
            TeensyI2S::getPinCLK(port),    // pin_clk (BCLK)
            static_cast<int>(port),        // i2s_num
            channel,
            sample_rate,
            bit_resolution,
            Philips,                       // comm_format (Teensy uses I2S Philips)
            false                          // invert
        );
        return AudioConfig(config);
    }

    AudioConfig(const AudioConfigI2S& config) : fl::variant<AudioConfigI2S, AudioConfigPdm>(config) {}
    AudioConfig(const AudioConfigPdm& config) : fl::variant<AudioConfigI2S, AudioConfigPdm>(config) {}
};

class IAudioInput {
public:
    // This is the single factory function for creating the audio source. If the creation was successful, then
    // the return value will be non-null. If the creation was not successful, then the return value will be null
    // and the error_message will be set to a non-empty string.
    // Keep in mind that the AudioConfig is a variant type. Many esp types do not support all the types in the variant.
    // For example, the AudioConfigPdm is not supported on the ESP32-C3 and in this case it will return a null pointer
    // and the error_message will be set to a non-empty string.
    // Implimentation notes:
    //   It's very important that the implimentation uses a esp task / interrupt to fill in the buffer. The reason is that
    //   there will be looooong delays during FastLED show() on some esp platforms, for example idf 4.4. If we do
    //   poll only, then audio buffers can be dropped. However if using a task then the audio buffers will be
    //   set internally via an interrupt / queue and then they can just be popped off the queue.
    static fl::shared_ptr<IAudioInput> create(const AudioConfig& config, fl::string* error_message = nullptr);


    virtual ~IAudioInput() = default;
    // Starts the audio source.
    virtual void start() = 0;
    // Stops the audio source, call this before light sleep.
    virtual void stop() = 0;

    virtual bool error(fl::string* msg = nullptr) = 0;  // if an error occured then query it here.
    // Read audio data and return as AudioSample with calculated timestamp.
    // Returns invalid AudioSample on error or when no data is available.
    virtual AudioSample read() = 0;

    // Read all available audio data and return as AudioSample. All AudioSamples
    // returned by this will be valid.
    size_t readAll(fl::vector_inlined<AudioSample, 16> *out) {
        size_t count = 0;
        while (true) {
            AudioSample sample = read();
            if (sample.isValid()) {
                out->push_back(sample);
                count++;
            } else {
                break;
            }
        }
        return count;
    }

};


// Free function for audio input creation - can be overridden by platform-specific implementations
fl::shared_ptr<IAudioInput> platform_create_audio_input(const AudioConfig& config, fl::string* error_message = nullptr) FL_LINK_WEAK;

}  // namespace fl
