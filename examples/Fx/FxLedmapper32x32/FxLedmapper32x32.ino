// @filter: (memory is large)

/// @file    FxLedmapper32x32.ino
/// @brief   Plays a mapped 32x32 raw RGB video asset for FASTLED4 preview and hardware output.
/// @example FxLedmapper32x32.ino
///
/// This sketch mirrors the SD-backed video flow in FASTLED4 and points it at a
/// ledmapper.com-style 32x32 asset at `data/video1.rgb`.
///
/// `video1.rgb` is NOT committed to this repo. It is hosted in FastLED/assets and
/// referenced by the sidecar `data/video1.rgb.lnk` (fbuild JSON descriptor with
/// url/sha256/size) resolved at build time. See `data/readme.txt` for the
/// fetch command (`fbuild lnk pull`).
///
/// The preview screen map is loaded from `data/screenmap.json`, sourced from the
/// ledmapper 32x32 quad serpentine preset, and is kept local because it is small.

#include "FastLED.h"
#include "Arduino.h"

#include "fl/fx/video.h"
#include "fl/math/screenmap.h"
#include "fl/system/file_system.h"
#include "fl/ui/ui.h"

#ifndef LED_PIN
#define LED_PIN 2
#endif

#ifndef CHIP_SELECT_PIN
#define CHIP_SELECT_PIN 5
#endif

#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 32
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

#define VIDEO_FPS 30
#define VIDEO_FRAME_HISTORY 2
#define DEFAULT_BRIGHTNESS 96

fl::CRGB leds[NUM_LEDS];

fl::FileSystem filesystem;
fl::Video video;
fl::ScreenMap screenMap;

fl::UITitle title("Ledmapper 32x32 Video");
fl::UIDescription description(
    "Streams data/video1.rgb as a 32x32 mapped raw RGB asset. "
    "Change VIDEO_FPS if the export was authored at a different frame rate.");
fl::UISlider brightness("Brightness", DEFAULT_BRIGHTNESS, 0, 255, 1);
fl::UISlider playbackSpeed("Playback Speed", 1.0f, -2.0f, 2.0f, 0.01f);

bool gError = false;

void setup() {
    Serial.begin(115200);
    Serial.println("FxLedmapper32x32 setup");

    if (!filesystem.beginSd(CHIP_SELECT_PIN)) {
        Serial.println("Failed to initialize file system.");
        gError = true;
        return;
    }

    if (!filesystem.readScreenMap("data/screenmap.json", "strip1", &screenMap)) {
        Serial.println("Failed to read data/screenmap.json");
        gError = true;
        return;
    }

    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenMap(screenMap);
    FastLED.setBrightness(DEFAULT_BRIGHTNESS);

    video = filesystem.openVideo("data/video1.rgb", NUM_LEDS, VIDEO_FPS,
                                 VIDEO_FRAME_HISTORY);
    if (!video) {
        FL_WARN("Failed to open data/video1.rgb");
        gError = true;
        return;
    }

    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    Serial.println("FxLedmapper32x32 ready");
}

void loop() {
    if (gError) {
        EVERY_N_SECONDS(1) {
            FL_WARN("FxLedmapper32x32 halted due to setup error.");
        }
        return;
    }

    FastLED.setBrightness(static_cast<uint8_t>(brightness.value()));
    video.setTimeScale(playbackSpeed.value());
    video.draw(fl::millis(), leds);
    FastLED.show();
}
