// @filter: (platform is teensy) or (platform is esp32)

// I2S Audio Input Example
// This example demonstrates using I2S audio input to control FastLED strips
// Supports ESP32 (INMP441 microphone) and Teensy (Teensy Audio Library)

// FastLED.h must be included first to trigger precompiled headers for FastLED's build system
#include "FastLED.h"

#include "fl/audio_input.h"
#include "fl/stl/sstream.h"


#if defined(FL_IS_TEENSY)
#include "TeensyAudioInput.h"
#else
#include "ESP32AudioInput.h"
#endif


using fl::i16;

// Global audio source (initialized in setup)
fl::shared_ptr<fl::IAudioInput> audioSource;

void setup() {
    Serial.begin(115200);

    Serial.println(PLATFORM_NAME);
    Serial.println("================================");
    printPlatformInfo();
    Serial.println();

    Serial.print("Waiting ");
    Serial.print(PLATFORM_INIT_DELAY_MS);
    Serial.println("ms for platform initialization...");
    delay(PLATFORM_INIT_DELAY_MS);

    // Create platform-specific audio configuration
    fl::AudioConfig config = createAudioConfig();

    // Initialize I2S Audio
    Serial.println("Initializing audio input...");
    fl::string errorMsg;
    audioSource = fl::IAudioInput::create(config, &errorMsg);

    if (!audioSource) {
        Serial.print("Failed to create audio source: ");
        Serial.println(errorMsg.c_str());
        return;
    }

    // Start audio capture
    Serial.println("Starting audio capture...");
    audioSource->start();

    // Check for start errors
    fl::string startErrorMsg;
    if (audioSource->error(&startErrorMsg)) {
        Serial.print("Audio start error: ");
        Serial.println(startErrorMsg.c_str());
        return;
    }

    Serial.println("Audio capture started successfully!");
    Serial.println("Reading audio data...");
    Serial.println();
}

void loop() {
    EVERY_N_MILLIS(1000) {
        Serial.println("Loop active...");
    }

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
    fl::AudioSample sample = audioSource->read();

    if (sample.isValid()) {
        EVERY_N_MILLIS(100) {
            const auto& audioBuffer = sample.pcm();
            const i16* max_sample = fl::max_element(audioBuffer.begin(), audioBuffer.end());
            const i16* min_sample = fl::min_element(audioBuffer.begin(), audioBuffer.end());

            fl::sstream ss;
            ss << "\n--- Audio Sample ---\n";
            ss << "Samples: " << audioBuffer.size() << "\n";
            ss << "Timestamp: " << sample.timestamp() << " ms\n";
            ss << "Max: " << *max_sample << "\n";
            ss << "Min: " << *min_sample << "\n";
            ss << "RMS: " << sample.rms() << "\n";
            ss << "Zero Crossing Frequency: " << sample.zcf() << " Hz\n";
            FL_WARN(ss.str());
        }
    }
}
