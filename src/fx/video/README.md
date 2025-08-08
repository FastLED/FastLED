# FastLED FX: Video Subsystem

This folder implements a simple video playback pipeline for LED arrays. It reads frames from a file or stream, buffers them, and interpolates between them to produce smooth animation at your desired output rate.

### Building blocks
- **`PixelStream` (`pixel_stream.h`)**: Reads pixel data as bytes from either a file (`FileHandle`) or a live `ByteStream`. Knows the `bytesPerFrame`, can read by pixel, by frame, or at an absolute frame number. Reports availability and end-of-stream.
- **`FrameTracker` (`frame_tracker.h`)**: Converts wall‑clock time to frame numbers (current and next) at a fixed FPS. Also exposes exact timestamps and frame interval in microseconds.
- **`FrameInterpolator` (`frame_interpolator.h`)**: Holds a small history of frames and, given the current time, blends the nearest two frames to produce an in‑between result. Supports non‑monotonic time (e.g., pause/rewind, audio sync).
- **`VideoImpl` (`video_impl.h`)**: High‑level orchestrator. Owns a `PixelStream` and a `FrameInterpolator`, manages fade‑in/out, time scaling, pause/resume, and draws into either a `Frame` or your `CRGB*` buffer.

### Typical flow
1. Create `VideoImpl` with your `pixelsPerFrame` and the source FPS.
2. Call `begin(...)` with a `FileHandle` or `ByteStream`.
3. Each frame, call `video.draw(now, leds)`.
   - Internally maintains a buffer of recent frames.
   - Interpolates between frames to match your output timing.
4. Use `setFade(...)`, `pause(...)`, `resume(...)`, and `setTimeScale(...)` as needed.
5. Call `end()` or `rewind()` to manage lifecycle.

### Notes
- Interpolation makes lower‑FPS content look smooth on higher‑FPS refresh loops.
- For streaming sources, some random access features (e.g., `rewind`) may be limited.
- `durationMicros()` reports the full duration for file sources, and `-1` for streams.

This subsystem is optional, intended for MCUs with adequate RAM and I/O throughput.

### Examples
- Memory stream video (from `examples/FxGfx2Video/FxGfx2Video.ino`):
  ```cpp
  #include <FastLED.h>
  #include "fl/bytestreammemory.h"
  #include "fx/video.h"
  using namespace fl;
  #define W 22
  #define H 22
  #define NUM_LEDS (W*H)
  CRGB leds[NUM_LEDS];
  ByteStreamMemoryPtr stream = fl::make_shared<ByteStreamMemory>(3*NUM_LEDS*2);
  Video video(NUM_LEDS, 2.0f); // 2 fps source
  void setup(){ FastLED.addLeds<WS2811,2,GRB>(leds, NUM_LEDS); video.beginStream(stream); }
  void loop(){ video.draw(millis(), leds); FastLED.show(); }
  ```
- SD card video (from `examples/FxSdCard/FxSdCard.ino`):
  ```cpp
  #include <FastLED.h>
  #include "fx/video.h"
  #include "fl/file_system.h"
  using namespace fl;
  #define W 32
  #define H 32
  #define NUM_LEDS (W*H)
  CRGB leds[NUM_LEDS];
  FileSystem fs;
  Video video;
  void setup(){
    FastLED.addLeds<WS2811,2,GRB>(leds, NUM_LEDS);
    video = fs.openVideo("data/video.rgb", NUM_LEDS, 60, 2);
  }
  void loop(){ video.draw(millis(), leds); FastLED.show(); }
  ```
