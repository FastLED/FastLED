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
///         fl::FramePtr frame = fl::readJpeg(fs, "photo.jpg") FL_NO_EXCEPT;
///     }
/// @endcode
///
/// Bodies live in `read.cpp.hpp`. These forward to the corresponding
/// `FileSystem` members, which remain for source compatibility. New code should prefer the free functions:
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
FramePtr readJpeg(FileSystem &fs, const char *path,
                  const JpegConfig &config = JpegConfig(),
                  string *error = nullptr) FL_NO_EXCEPT;

/// Streaming MP3 decoder over a file. Null on failure.
Mp3DecoderPtr readMp3(FileSystem &fs, const char *path,
                             string *error = nullptr) FL_NO_EXCEPT;

/// Raw-frame video. Null if the file could not be opened.
Video readVideo(FileSystem &fs, const char *path,
                       fl::size pixelsPerFrame, float fps = 30.0f,
                       fl::size nFrameHistory = 0) FL_NO_EXCEPT;

/// MPEG1 video. Null if the file could not be opened.
Video readMpeg1Video(FileSystem &fs, const char *path,
                            fl::size pixelsPerFrame, float fps = 30.0f,
                            fl::size nFrameHistory = 0) FL_NO_EXCEPT;

/// Whole file as text.
bool readText(FileSystem &fs, const char *path, string *out) FL_NO_EXCEPT;

/// Parse a file as JSON.
bool readJson(FileSystem &fs, const char *path, json *doc) FL_NO_EXCEPT;

/// One named screen map from a screenmap JSON file.
bool readScreenMap(FileSystem &fs, const char *path, const char *name,
                          ScreenMap *out, string *error = nullptr) FL_NO_EXCEPT;

/// Every screen map in a screenmap JSON file, keyed by name.
bool readScreenMaps(FileSystem &fs, const char *path,
                           fl::flat_map<string, ScreenMap> *out,
                           string *error = nullptr) FL_NO_EXCEPT;

/// Load a `.fled` container. Returns a null Fled if absent or malformed --
/// the Fled type owns its own failure reporting.
Fled readFled(FileSystem &fs, const char *path) FL_NO_EXCEPT;

} // namespace fl
