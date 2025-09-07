// I2S Audio Example for ESP32
// This example demonstrates using I2S audio input to control FastLED strips
// Based on audio levels from microphone or line input
//
// This example uses the extremely popular (as of 2025-September) INMP441 microphone.
// Notes:
//   - Connect L/R to PWR so it's recognized as a right channel microphone.

#include <Arduino.h>
#include <FastLED.h>

#include "fl/sstream.h"
#include "fl/type_traits.h"

#include "platforms/esp/32/audio/audio.h"
#include "platforms/esp/32/audio/sound_util.h"

using fl::i16;

// I2S Configuration
#define I2S_WS_PIN 7  // Word Select (LRCLK)
#define I2S_SD_PIN 8  // Serial Data (DIN)
#define I2S_CLK_PIN 4 // Serial Clock (BCLK)
#define I2S_CHANNEL fl::Right

fl::AudioConfig config = fl::AudioConfig::CreateInmp441(I2S_WS_PIN, I2S_SD_PIN, I2S_CLK_PIN, I2S_CHANNEL);
fl::shared_ptr<fl::IAudioInput> audioSource;
fl::vector_inlined<fl::i16, I2S_AUDIO_BUFFER_LEN> audioBuffer;

void setup() {
    Serial.begin(115200);

    Serial.println("I2S Audio FastLED Example");
    Serial.println("Waiting 5000ms for audio device to stdout initialization...");
    delay(5000);

    // Initialize I2S Audio
    fl::string errorMsg;
    audioSource = fl::IAudioInput::create(config, &errorMsg);

    if (!audioSource) {
        Serial.print("Failed to create audio source: ");
        Serial.println(errorMsg.c_str());
        return;
    }


    // Initialize and start audio capture
    Serial.println("Initializing audio capture...");
    audioSource->init();

    // Check for initialization errors
    fl::string initErrorMsg;
    if (audioSource->error(&initErrorMsg)) {
        Serial.print("Audio init error: ");
        Serial.println(initErrorMsg.c_str());
        return;
    }

    Serial.println("Starting audio capture...");
    audioSource->start();

    // Check for start errors
    fl::string startErrorMsg;
    if (audioSource->error(&startErrorMsg)) {
        Serial.print("Audio start error: ");
        Serial.println(startErrorMsg.c_str());
        return;
    }

    Serial.println("Audio capture started!");
}


void loop() {
    EVERY_N_MILLIS(1000) { Serial.println("loop active."); }
    // Check if audio source is valid
    if (!audioSource) {
        Serial.println("Audio source is null!");
        delay(1000);
        return;
    }

    // Check for audio errors
    fl::string errorMsg;
    if (audioSource->error(&errorMsg)) {
        Serial.print("Audio error: ");
        Serial.println(errorMsg.c_str());
        delay(100);
        return;
    }

    // Read audio data
    int bytesRead = audioSource->read(&audioBuffer);

    if (bytesRead > 0) {
        EVERY_N_MILLIS(100) {
            i16* max_sample = fl::max_element(audioBuffer.begin(), audioBuffer.end());
            i16* min_sample = fl::min_element(audioBuffer.begin(), audioBuffer.end());
            fl::sstream ss;
            ss << "\nRead " << bytesRead << " bytes\n";
            ss << "Max sample: " << *max_sample << "\n";
            ss << "Min sample: " << *min_sample << "\n";
            FL_WARN(ss.str());
        }
    }
}
