// @filter: (memory is large)

/// @file    FxSdCard.ino
/// @brief   Play a mapped video off an SD card.
/// @author  Zach Vorhies
///
/// This sketch supports two on-disk formats:
///
///   1. **FLED v1 container** (`*.fled`) — preferred. A single self-
///      describing file that carries both the raw RGB frames and the
///      ScreenMap that maps them onto the physical LED layout. No
///      sidecar `.json` needed; the mapping travels in the file's
///      12-byte header. Spec:
///      https://github.com/zackees/ledmapper/blob/main/docs/fled-format.md
///
///   2. **Legacy headerless `.rgb`** + sidecar `screenmap.json` — kept
///      working for backward compatibility. If no `.fled` file is on the
///      SD card, the sketch falls back to the old pairing.
///
/// Compatible with the FastLED web compiler:
///   1. `pip install fastled`
///   2. `cd` into this example dir
///   3. `fastled` at the repo root opens a browser-based runner.

#include "FastLED.h"
#include "Arduino.h"

#include "fl/fx/2d/noisepalette.h"
// #include "fl/fx/2d/animartrix.hpp"
#include "fl/fx/fx_engine.h"
#include "fl/fx/video.h"
#include "fl/system/file_system.h"
#include "fl/ui/ui.h"
#include "fl/math/screenmap.h"
#include "fl/system/file_system.h"




#define LED_PIN 2
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define FPS 60
#define CHIP_SELECT_PIN 5



#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 32
#define NUM_VIDEO_FRAMES 2  // enables interpolation with > 1 frame.


#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)
#define IS_SERPINTINE true


fl::UITitle title("SDCard Demo - Mapped fl::Video");
fl::UIDescription description("fl::Video data is streamed off of a SD card and displayed on a LED strip. The video data is mapped to the LED strip using a fl::ScreenMap embedded in the FLED file (with a sidecar screenmap.json fallback for legacy .rgb files).");


fl::CRGB leds[NUM_LEDS];
fl::ScreenMap screenMap;

fl::FileSystem filesystem;
fl::Video video;
fl::Video video2;

fl::UISlider videoSpeed("fl::Video Speed", 1.0f, -1, 2.0f, 0.01f);
UINumberField whichVideo("Which fl::Video", 0, 0, 1);


bool gError = false;

// Open `<base>.fled` if present, otherwise fall back to `<base>.rgb`.
// Returns the resolved Video; sets gError + warns on hard failure.
//
// The screenMap-out param is populated from EITHER the FLED file's
// embedded JSON OR a sidecar screenmap.json — whichever is available.
// On legacy .rgb the caller's `sidecarScreenmapPath` is loaded; for FLED
// the embedded JSON wins and the sidecar path is ignored.
// Reject any ScreenMap whose LED count disagrees with NUM_LEDS. A silent
// mismatch corrupts the mapping in addLeds().setScreenMap() — there is no
// runtime fix once frames start flowing, so fail loud at setup time.
static bool validateScreenMapSize(const fl::ScreenMap &m, const char *source) {
    if (m.getLength() != NUM_LEDS) {
        FL_WARN("Screenmap from " << source << " has "
            << m.getLength() << " LEDs but NUM_LEDS=" << NUM_LEDS
            << " — mapping would silently corrupt playback.");
        gError = true;
        return false;
    }
    return true;
}

fl::Video openVideoEitherFormat(const char *fledPath,
                                const char *rgbPath,
                                const char *sidecarScreenmapPath,
                                fl::ScreenMap *outScreenMap) {
    // Try FLED first.
    fl::Video v = filesystem.openVideo(fledPath, NUM_LEDS, FPS, NUM_VIDEO_FRAMES);
    if (v && v.hasEmbeddedScreenMap()) {
        const fl::string &json = v.embeddedScreenMapJson();
        fl::string parseErr;
        if (!fl::ScreenMap::ParseJson(json.c_str(), "strip1", outScreenMap, &parseErr)) {
            FL_WARN("FLED embedded screenmap parse failed: " << parseErr.c_str());
            gError = true;
            return v;
        }
        validateScreenMapSize(*outScreenMap, fledPath);
        return v;
    }
    // No .fled (or it lacks an embedded map). Fall back to legacy .rgb.
    fl::Video legacy = filesystem.openVideo(rgbPath, NUM_LEDS, FPS, NUM_VIDEO_FRAMES);
    if (!legacy) {
        FL_WARN("Failed to open " << rgbPath << " (and no " << fledPath << " either)");
        gError = true;
        return legacy;
    }
    if (!filesystem.readScreenMap(sidecarScreenmapPath, "strip1", outScreenMap)) {
        FL_WARN("Failed to read sidecar " << sidecarScreenmapPath);
        gError = true;
        return legacy;
    }
    validateScreenMapSize(*outScreenMap, sidecarScreenmapPath);
    return legacy;
}

void setup() {
    Serial.begin(115200);
    Serial.println("Sketch setup");
    // Initialize the file system and check for errors
    if (!filesystem.beginSd(CHIP_SELECT_PIN)) {
        Serial.println("Failed to initialize file system.");
    }

    // Open both videos. FLED first (carries its own ScreenMap), legacy
    // .rgb + sidecar screenmap.json as fallback. Both videos use the same
    // physical map, so the second screenMap parse is harmless overwrite.
    video = openVideoEitherFormat(
        "data/video.fled", "data/video.rgb",
        "data/screenmap.json", &screenMap);
    if (gError) return;

    video2 = openVideoEitherFormat(
        "data/color_line_bubbles.fled", "data/color_line_bubbles.rgb",
        "data/screenmap.json", &screenMap);
    if (gError) return;

    // Configure FastLED with the LED type, pin, and color order
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip)
        .setScreenMap(screenMap);
    FastLED.setBrightness(96);
    Serial.println("FastLED setup done");
}

void loop() {
    static bool s_first = true;
    if (s_first) {
      s_first = false;
      Serial.println("First loop.");
    }
    if (gError) {
      // If an error occurred, print a warning every second
      EVERY_N_SECONDS(1) {
        FL_WARN("No loop because an error occured.");
      }
      return;
    }

    // Select the video to play based on the UI input
    fl::Video& vid = !bool(whichVideo.value()) ? video : video2;
    vid.setTimeScale(videoSpeed);

    // Get the current time and draw the video frame
    uint32_t now = fl::millis();
    vid.draw(now, leds);
    FastLED.show();
}
