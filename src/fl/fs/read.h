#pragma once

/// @file fl/fs/read.h
/// @brief Decoding files into FastLED types.
///
/// Separate from `fl/fs/fs.h` on purpose. Opening a file and decoding a
/// JPEG are unrelated concerns, and keeping them apart means a sketch that
/// only reads bytes does not compile the video and JPEG headers
/// (FastLED #4003, #4008).
///
/// @code
///     fl::FileSystem fs;
///     if (fs.begin(fl::getSdFs(5))) {
///         fl::FramePtr frame = fl::readJpeg(fs, "photo.jpg");
///     }
/// @endcode
///
/// These forward to the corresponding `FileSystem` members, which remain
/// for source compatibility. New code should prefer the free functions:
/// adding a format then costs one function here and no change to
/// `FileSystem`, where today it widens the class every time.

#include "fl/codec/jpeg.h"  // IWYU pragma: export
#include "fl/fs/fs.h"       // IWYU pragma: export
#include "fl/fx/video.h"    // IWYU pragma: export
#include "fl/stl/noexcept.h"

namespace fl {

class ScreenMap;
class Fled;

/// Decode a JPEG into a Frame. Null on failure; `error` receives why.
inline FramePtr readJpeg(FileSystem &fs, const char *path,
                         const JpegConfig &config = JpegConfig(),
                         string *error = nullptr) {
    return fs.loadJpeg(path, config, error);
}

/// Streaming MP3 decoder over a file. Null on failure.
inline Mp3DecoderPtr readMp3(FileSystem &fs, const char *path,
                             string *error = nullptr) {
    return fs.openMp3(path, error);
}

/// Raw-frame video. Null if the file could not be opened.
inline Video readVideo(FileSystem &fs, const char *path,
                       fl::size pixelsPerFrame, float fps = 30.0f,
                       fl::size nFrameHistory = 0) {
    return fs.openVideo(path, pixelsPerFrame, fps, nFrameHistory);
}

/// MPEG1 video. Null if the file could not be opened.
inline Video readMpeg1Video(FileSystem &fs, const char *path,
                            fl::size pixelsPerFrame, float fps = 30.0f,
                            fl::size nFrameHistory = 0) {
    return fs.openMpeg1Video(path, pixelsPerFrame, fps, nFrameHistory);
}

/// Whole file as text.
inline bool readText(FileSystem &fs, const char *path, string *out) {
    return fs.readText(path, out);
}

/// Parse a file as JSON.
inline bool readJson(FileSystem &fs, const char *path, json *doc) {
    return fs.readJson(path, doc);
}

/// One named screen map from a screenmap JSON file.
inline bool readScreenMap(FileSystem &fs, const char *path, const char *name,
                          ScreenMap *out, string *error = nullptr) {
    return fs.readScreenMap(path, name, out, error);
}

/// Every screen map in a screenmap JSON file, keyed by name.
inline bool readScreenMaps(FileSystem &fs, const char *path,
                           fl::flat_map<string, ScreenMap> *out,
                           string *error = nullptr) {
    return fs.readScreenMaps(path, out, error);
}

/// Load a `.fled` container. Returns a null Fled if absent or malformed —
/// the Fled type owns its own failure reporting.
inline Fled readFled(FileSystem &fs, const char *path) FL_NO_EXCEPT {
    return fs.loadFled(path);
}

} // namespace fl
