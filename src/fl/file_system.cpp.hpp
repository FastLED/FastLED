#include "fl/file_system.h"
#include "fl/codec/idecoder.h"
#include "fl/stl/has_include.h"
#include "fl/warn.h"
#include "fl/dbg.h"
#include "fl/codec/jpeg.h"
#include "fl/codec/mp3.h"
#include "fl/stl/vector.h"
#include "fl/math_macros.h"

#ifdef FASTLED_TESTING
// Test filesystem implementation that maps to real hard drive
// IWYU pragma: begin_keep
#include "platforms/stub/fs_stub.hpp" // ok platform headers
// IWYU pragma: end_keep
#define FASTLED_HAS_SDCARD 1
#elif defined(FL_IS_WASM)
// IWYU pragma: begin_keep
#include "platforms/wasm/fs_wasm.h" // ok platform headers
// IWYU pragma: end_keep
#define FASTLED_HAS_SDCARD 1
#elif FL_HAS_INCLUDE(<SD.h>) && FL_HAS_INCLUDE(<fs.h>)
// Include Arduino SD card implementation when SD library is available
#include "platforms/fs_sdcard_arduino.hpp"
#define FASTLED_HAS_SDCARD 1
#else
#define FASTLED_HAS_SDCARD 0
#endif

#include "fl/stl/json.h"
#include "fl/screenmap.h"
#include "fl/codec/mpeg1.h"
#include "fl/math_macros.h" // for min
#include "fl/stl/cstring.h"

namespace fl {

// filebuf that wraps MPEG1 decoder for seamless integration with Video system
// Non-seekable: decodes frames on demand and provides them as sequential bytes
class Mpeg1FileHandle : public filebuf {
private:
    IDecoderPtr mDecoder;
    fl::shared_ptr<Frame> mCurrentFrame;
    fl::size mFrameSize;
    fl::size mCurrentPos;
    fl::string mPath;
    bool mHasValidFrame;

    bool decodeNextFrameIfNeeded() {
        if (mCurrentPos >= mFrameSize || !mHasValidFrame) {
            if (!mDecoder->hasMoreFrames()) {
                mHasValidFrame = false;
                return false;
            }

            DecodeResult result = mDecoder->decode();
            if (result == DecodeResult::Success) {
                Frame decodedFrame = mDecoder->getCurrentFrame();
                mCurrentFrame = fl::make_shared<Frame>(decodedFrame);
                mCurrentPos = 0;
                mHasValidFrame = true;
                return true;
            } else {
                mHasValidFrame = false;
                return false;
            }
        }
        return mHasValidFrame;
    }

public:
    Mpeg1FileHandle(IDecoderPtr decoder, fl::size pixelsPerFrame, const char* path)
        : mDecoder(decoder), mCurrentFrame(nullptr), mFrameSize(pixelsPerFrame * 3), mCurrentPos(0),
          mPath(path), mHasValidFrame(false) {
        decodeNextFrameIfNeeded();
    }

    bool is_open() const override { return mDecoder != nullptr; }

    void close() override {
        if (mDecoder) {
            mDecoder->end();
        }
    }

    fl::size_t read(char* dst, fl::size_t bytesToRead) override {
        if (!mDecoder || !mHasValidFrame) {
            return 0;
        }

        fl::size totalRead = 0;

        while (bytesToRead > 0 && mHasValidFrame) {
            fl::size remainingInFrame = mFrameSize - mCurrentPos;

            if (remainingInFrame == 0) {
                if (!decodeNextFrameIfNeeded()) {
                    break;
                }
                remainingInFrame = mFrameSize - mCurrentPos;
            }

            fl::size toRead = fl::min(bytesToRead, remainingInFrame);
            if (toRead > 0 && mCurrentFrame && mCurrentFrame->rgb().data()) {
                fl::memcpy(dst + totalRead, (fl::u8*)mCurrentFrame->rgb().data() + mCurrentPos, toRead);
                mCurrentPos += toRead;
                totalRead += toRead;
                bytesToRead -= toRead;
            } else {
                break;
            }
        }

        return totalRead;
    }
    using filebuf::read; // u8 overload

    fl::size_t write(const char*, fl::size_t) override { return 0; } // Read-only
    fl::size_t tell() override { return 0; }
    bool seek(fl::size_t, seek_dir) override { return false; } // Non-seekable
    using filebuf::seek;
    fl::size_t size() const override { return 0; } // Unknown for streaming

    const char* path() const override { return mPath.c_str(); }

    bool is_eof() const override { return !mHasValidFrame && !mDecoder->hasMoreFrames(); }
    bool has_error() const override { return false; }
    void clear_error() override {}
    int error_code() const override { return 0; }
    const char* error_message() const override { return "No error"; }

    bool available() const override { return mHasValidFrame || mDecoder->hasMoreFrames(); }

    fl::size_t bytes_left() const override {
        if (!mHasValidFrame) return 0;
        fl::size inFrame = (mCurrentPos < mFrameSize) ? (mFrameSize - mCurrentPos) : 0;
        return inFrame;
    }
};

class NullFileHandle : public filebuf {
  public:
    NullFileHandle() = default;
    ~NullFileHandle() override {}

    bool is_open() const override { return false; }
    fl::size_t size() const override { return 0; }
    fl::size_t read(char *dst, fl::size_t bytesToRead) override {
        FASTLED_UNUSED(dst);
        FASTLED_UNUSED(bytesToRead);
        return 0;
    }
    fl::size_t write(const char *data, fl::size_t count) override {
        FASTLED_UNUSED(data);
        FASTLED_UNUSED(count);
        return 0;
    }
    fl::size_t tell() override { return 0; }
    const char *path() const override { return "nullptr filebuf"; }
    bool seek(fl::size_t pos, seek_dir dir) override {
        FASTLED_UNUSED(pos);
        FASTLED_UNUSED(dir);
        return false;
    }
    using filebuf::seek; // single-arg overload
    void close() override {}
    bool is_eof() const override { return true; }
    bool has_error() const override { return false; }
    void clear_error() override {}
    int error_code() const override { return 0; }
    const char *error_message() const override { return "NullFileHandle"; }
};

class NullFileSystem : public FsImpl {
  public:
    NullFileSystem() {
        FASTLED_WARN("NullFileSystem instantiated as a placeholder, please "
                     "implement a file system for your platform.");
    }
    ~NullFileSystem() override {}

    bool begin() override { return true; }
    void end() override {}

    filebuf_ptr openRead(const char *_path) override {
        FASTLED_UNUSED(_path);
        fl::shared_ptr<NullFileHandle> ptr = fl::make_shared<NullFileHandle>();
        filebuf_ptr out = ptr;
        return out;
    }
};


bool FileSystem::beginSd(int cs_pin) {
    mFs = make_sdcard_filesystem(cs_pin);
    if (!mFs) {
        return false;
    }
    mFs->begin();
    return true;
}

bool FileSystem::begin(FsImplPtr platform_filesystem) {
    mFs = platform_filesystem;
    if (!mFs) {
        return false;
    }
    mFs->begin();
    return true;
}

FileSystem::FileSystem() : mFs() {}

void FileSystem::end() {
    if (mFs) {
        mFs->end();
    }
}

bool FileSystem::readJson(const char *path, json *doc) {
    string text;
    if (!readText(path, &text)) {
        return false;
    }
    
    // Parse using the new json class
    *doc = fl::json::parse(text);
    return !doc->is_null();
}

bool FileSystem::readScreenMaps(const char *path,
                                fl::map<string, ScreenMap> *out, string *error) {
    string text;
    if (!readText(path, &text)) {
        FASTLED_WARN("Failed to read file: " << path);
        if (error) {
            *error = "Failed to read file: ";
            error->append(path);
        }
        return false;
    }
    string err;
    bool ok = ScreenMap::ParseJson(text.c_str(), out, &err);
    if (!ok) {
        FASTLED_WARN("Failed to parse screen map: " << err.c_str());
        *error = err;
        return false;
    }
    return true;
}

bool FileSystem::readScreenMap(const char *path, const char *name,
                               ScreenMap *out, string *error) {
    string text;
    if (!readText(path, &text)) {
        FASTLED_WARN("Failed to read file: " << path);
        if (error) {
            *error = "Failed to read file: ";
            error->append(path);
        }
        return false;
    }
    string err;
    bool ok = ScreenMap::ParseJson(text.c_str(), name, out, &err);
    if (!ok) {
        FASTLED_WARN("Failed to parse screen map: " << err.c_str());
        *error = err;
        return false;
    }
    return true;
}

fl::ifstream FileSystem::openRead(const char *path) {
    return fl::ifstream(mFs->openRead(path));
}
Video FileSystem::openVideo(const char *path, fl::size pixelsPerFrame, float fps,
                            fl::size nFrameHistory) {
    Video video(pixelsPerFrame, fps, nFrameHistory);
    fl::ifstream file = openRead(path);
    if (!file.is_open()) {
        video.setError(fl::string("Could not open file: ").append(path));
        return video;
    }
    video.begin(file.rdbuf());
    return video;
}

Video FileSystem::openMpeg1Video(const char *path, fl::size pixelsPerFrame, float fps,
                                 fl::size nFrameHistory) {
    Video video(pixelsPerFrame, fps, nFrameHistory);

    // Open the MPEG1 file from SD card
    fl::ifstream file = openRead(path);
    if (!file.is_open()) {
        video.setError(fl::string("Could not open MPEG1 file: ").append(path));
        return video;
    }

    // Create MPEG1 decoder configuration
    Mpeg1Config config;
    config.mode = Mpeg1Config::Streaming;
    config.targetFps = static_cast<fl::u16>(fps);
    config.looping = false; // Can be made configurable later
    config.skipAudio = true; // Skip audio for LED applications

    // Create MPEG1 decoder
    fl::string error_message;
    IDecoderPtr decoder = Mpeg1::createDecoder(config, &error_message);
    if (!decoder) {
        video.setError(fl::string("Failed to create MPEG1 decoder: ").append(error_message));
        return video;
    }

    // Initialize decoder with filebuf
    if (!decoder->begin(file.rdbuf())) {
        fl::string decoder_error;
        decoder->hasError(&decoder_error);
        video.setError(fl::string("Failed to initialize MPEG1 decoder: ").append(decoder_error));
        return video;
    }

    // Create filebuf that provides decoded frames as sequential bytes
    fl::shared_ptr<Mpeg1FileHandle> mpeg1Stream =
        fl::make_shared<Mpeg1FileHandle>(decoder, pixelsPerFrame, path);

    // Initialize video with the MPEG1 decoded stream
    if (!video.begin(mpeg1Stream)) {
        video.setError(fl::string("Failed to initialize video with MPEG1 stream"));
        return video;
    }

    return video;
}

bool FileSystem::readText(const char *path, fl::string *out) {
    fl::ifstream file = openRead(path);
    if (!file.is_open()) {
        FASTLED_WARN("Failed to open file: " << path);
        return false;
    }
    fl::size size = file.size();
    out->reserve(size + out->size());
    bool wrote = false;
    while (file.available()) {
        u8 buf[64];
        fl::size n = file.read(buf, sizeof(buf));
        out->append((const char *)buf, n);
        wrote = true;
    }
    file.close();
    FASTLED_DBG_IF(!wrote, "Failed to write any data to the output string.");
    return wrote;
}

FramePtr FileSystem::loadJpeg(const char *path, const JpegConfig &config,
                               fl::string *error_message) {
    // Open the JPEG file
    fl::ifstream file = openRead(path);
    if (!file.is_open()) {
        if (error_message) {
            *error_message = "Failed to open file: ";
            error_message->append(path);
        }
        FASTLED_WARN("Failed to open JPEG file: " << path);
        return FramePtr();
    }

    // Get file size
    fl::size fileSize = file.size();
    if (fileSize == 0) {
        if (error_message) {
            *error_message = "File is empty: ";
            error_message->append(path);
        }
        file.close();
        return FramePtr();
    }

    // Read the entire file into memory
    // For small files, use stack allocation; for larger files use heap
    fl::vector<u8> buffer;
    buffer.reserve(fileSize);
    buffer.resize(fileSize);

    fl::size bytesRead = 0;
    while (bytesRead < fileSize && file.available()) {
        fl::size chunkSize = min<fl::size>(4096, fileSize - bytesRead);
        fl::size n = file.read(buffer.data() + bytesRead, chunkSize);
        if (n == 0) {
            break; // No more data to read
        }
        bytesRead += n;
    }

    file.close();

    if (bytesRead != fileSize) {
        if (error_message) {
            *error_message = "Failed to read complete file. Expected ";
            error_message->append(static_cast<u32>(fileSize));
            error_message->append(" bytes, got ");
            error_message->append(static_cast<u32>(bytesRead));
        }
        FASTLED_WARN("Failed to read complete JPEG file: " << path);
        return FramePtr();
    }

    // Create a span from the buffer
    fl::span<const u8> jpegData(buffer.data(), buffer.size());

    // Decode the JPEG
    FramePtr frame = Jpeg::decode(config, jpegData, error_message);

    if (!frame && error_message && error_message->empty()) {
        *error_message = "Failed to decode JPEG from file: ";
        error_message->append(path);
    }

    return frame;
}

fl::Mp3DecoderPtr FileSystem::openMp3(const char *path,
                                      fl::string *error_message) {
    // Open the MP3 file
    fl::ifstream file = openRead(path);
    if (!file.is_open()) {
        if (error_message) {
            *error_message = "Failed to open file: ";
            error_message->append(path);
        }
        FASTLED_WARN("Failed to open MP3 file: " << path);
        return fl::Mp3DecoderPtr();
    }

    // Create MP3 stream decoder using the public API
    fl::Mp3DecoderPtr decoder = fl::Mp3::createDecoder(error_message);

    if (!decoder->begin(file.rdbuf())) {
        fl::string decoder_error;
        decoder->hasError(&decoder_error);
        if (error_message) {
            *error_message = "Failed to initialize MP3 decoder: ";
            error_message->append(decoder_error);
        }
        FASTLED_WARN("Failed to initialize MP3 decoder for: " << path);
        return fl::Mp3DecoderPtr();
    }

    return decoder;
}

} // namespace fl

namespace fl {

#if !FASTLED_HAS_SDCARD
// Weak fallback implementation when SD library is not available
FL_LINK_WEAK FsImplPtr make_sdcard_filesystem(int cs_pin) {
    FASTLED_UNUSED(cs_pin);
    fl::shared_ptr<NullFileSystem> ptr = fl::make_shared<NullFileSystem>();
    FsImplPtr out = ptr;
    return out;
}
#endif

} // namespace fl
