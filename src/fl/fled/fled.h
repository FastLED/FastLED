#pragma once

// fl::Fled - value-type handle to a parsed .fled v1 container.
// PIMPL; cheap to copy. Default-constructed instances are in a null state
// and evaluate to false. See FLED_FORMAT.md for the on-disk format.
//
// What Fled exposes:
//   - Header info: version(), pixelFormat().
//   - Raw JSON envelope: json(), sectionCount().
//   - Raw byte ranges: blob(name, &outLen) for the post-JSON frame payload.
//   - Fully constructed objects: screenMap(), channels().
//
// What Fled deliberately does NOT expose (purged from v1):
//   - No video() accessor. Video access is via blob("frame_payload", &n) plus
//     json()["video"] for metadata. No Video object is reconstructed here.
//   - No script() accessor. Scripting is a future-v2 surface (docs only).

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"

namespace fl {

class FileSystem;
class json;
class ScreenMap;
struct MultiChannelConfig;

namespace fled {
class FledImpl;
}

class Fled {
  public:
    // Load from filesystem. Returns a null Fled on any failure (open fail,
    // truncated, bad magic/version, oversized json_length, parse error).
    static Fled load(FileSystem &fs, const char *path) FL_NO_EXCEPT;

    // Zero-copy load from a static byte span. Span must outlive the Fled
    // and any of its copies (this constructor does NOT copy the bytes).
    static Fled loadFromStatic(fl::span<const fl::u8> bytes) FL_NO_EXCEPT;

    // Move-load from a caller-owned vector. The vector's storage is moved
    // into the impl; no extra copy.
    static Fled loadFromVector(fl::vector<fl::u8> &&bytes) FL_NO_EXCEPT;

    // Default ctor produces a null Fled (operator bool == false).
    Fled() FL_NO_EXCEPT;

    // True iff load succeeded and produced a valid v1 envelope.
    explicit operator bool() const FL_NO_EXCEPT;

    // Header version byte. Returns 0 for null Fled, 1 for valid v1.
    fl::u8 version() const FL_NO_EXCEPT;

    // Header pixel-format byte (offset 5). 0x00 = rgb8. Returns 0 for null.
    fl::u8 pixelFormat() const FL_NO_EXCEPT;

    // Bytes-per-LED for the configured pixel_format, per FLED_FORMAT.md.
    // Returns 0 for unknown / reserved pixel formats (0x05 - 0xff) - the
    // FLED_FORMAT.md spec says consumers should reject before reading
    // frame bytes in that case.
    fl::u8 bytesPerLed() const FL_NO_EXCEPT;

    // Length of the frame_payload byte range (everything after the JSON
    // envelope). Returns 0 for null Fled.
    fl::size payloadBytes() const FL_NO_EXCEPT;

    // Derived frame count per FLED_FORMAT.md:
    //   frame_count = payload_bytes / (led_count * bytes_per_led)
    // The caller supplies led_count (typically from screenMap()->getLength()).
    // Returns 0 if led_count is 0, bytes-per-LED is 0, or the bundle is null.
    fl::size frameCount(fl::size ledCount) const FL_NO_EXCEPT;

    // Reads video.fps from the JSON envelope, falling back to defaultFps
    // (30 by default) if the key is absent or not a number. FLED_FORMAT.md:
    // "If video.fps is absent, consumers may use an application default,
    // sketch parameter, or external playback setting."
    float videoFps(float defaultFps = 30.0f) const FL_NO_EXCEPT;

    // Parsed JSON envelope. For null Fled, returns a reference to a static
    // empty json (safe to chain into).
    const fl::json &json() const FL_NO_EXCEPT;

    // Number of top-level keys in the parsed JSON envelope. 0 for null.
    fl::size sectionCount() const FL_NO_EXCEPT;

    // Returns a shared pointer aliased to the underlying byte storage for
    // the named section. Recognizes "frame_payload" (alias "payload"),
    // which maps to the raw bytes after the JSON envelope. Any other name
    // returns nullptr and writes 0 to *outLen (if non-null).
    //
    // Lifetime: the returned shared_ptr extends FledImpl's lifetime, but
    // does NOT extend the lifetime of bytes loaded via loadFromStatic -
    // those still depend on the caller's span outliving every use.
    fl::shared_ptr<const fl::u8> blob(const char *sectionName,
                                      fl::size *outLen) const FL_NO_EXCEPT;

    // Typed section accessors. Each returns nullptr if the bundle has no
    // section of that type. Construction touches NO global state (no
    // controller registration, no EngineEvents broadcast, no scheduler).
    fl::shared_ptr<ScreenMap>          screenMap() const FL_NO_EXCEPT;
    fl::shared_ptr<MultiChannelConfig> channels()  const FL_NO_EXCEPT;

  private:
    explicit Fled(fl::shared_ptr<fled::FledImpl> impl) FL_NO_EXCEPT;
    fl::shared_ptr<fled::FledImpl> mImpl;
};

} // namespace fl
