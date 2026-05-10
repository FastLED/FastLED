// ok no header
/// @file file_system_codecs.cpp.hpp
/// @brief FileSystem codec methods — compiled in fl.codec+ module.
/// Moved from file_system.cpp.hpp to break fl.system+ -> fl.codec+ dependency.
/// Contains: openMp3(), loadJpeg(), openMpeg1Video(), Mpeg1FileHandle

#include "fl/system/file_system.h"
#include "fl/codec/idecoder.h"
#include "fl/codec/jpeg.h"
#include "fl/codec/mp3.h"
#include "fl/codec/mpeg1.h"
#include "fl/log/log.h"
#include "fl/math/math.h"
#include "fl/stl/vector.h"
#include "fl/stl/cstring.h"

namespace fl {

// filebuf wrapper for MPEG1 decoded frame stream
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
            if (!mDecoder->hasMoreFrames()) { mHasValidFrame = false; return false; }
            DecodeResult result = mDecoder->decode();
            if (result == DecodeResult::Success) {
                Frame decodedFrame = mDecoder->getCurrentFrame();
                mCurrentFrame = fl::make_shared<Frame>(decodedFrame);
                mCurrentPos = 0; mHasValidFrame = true; return true;
            } else { mHasValidFrame = false; return false; }
        }
        return mHasValidFrame;
    }

public:
    Mpeg1FileHandle(IDecoderPtr decoder, fl::size pixelsPerFrame, const char* path)
        : mDecoder(decoder), mCurrentFrame(nullptr), mFrameSize(pixelsPerFrame * 3),
          mCurrentPos(0), mPath(path), mHasValidFrame(false) { decodeNextFrameIfNeeded(); }
    bool is_open() const override { return mDecoder != nullptr; }
    void close() override { if (mDecoder) mDecoder->end(); }
    fl::size_t read(char* dst, fl::size_t bytesToRead) override {
        if (!mDecoder || !mHasValidFrame) return 0;
        fl::size totalRead = 0;
        while (bytesToRead > 0 && mHasValidFrame) {
            fl::size rem = mFrameSize - mCurrentPos;
            if (rem == 0) { if (!decodeNextFrameIfNeeded()) break; rem = mFrameSize - mCurrentPos; }
            fl::size toRead = fl::min(bytesToRead, rem);
            if (toRead > 0 && mCurrentFrame && mCurrentFrame->rgb().data()) {
                fl::memcpy(dst + totalRead, (fl::u8*)mCurrentFrame->rgb().data() + mCurrentPos, toRead);
                mCurrentPos += toRead; totalRead += toRead; bytesToRead -= toRead;
            } else break;
        }
        return totalRead;
    }
    using filebuf::read;
    fl::size_t write(const char*, fl::size_t) override { return 0; }
    fl::size_t tell() override { return 0; }
    bool seek(fl::size_t, seek_dir) override { return false; }
    using filebuf::seek;
    fl::size_t size() const override { return 0; }
    const char* path() const override { return mPath.c_str(); }
    bool is_eof() const override { return !mHasValidFrame && !mDecoder->hasMoreFrames(); }
    bool has_error() const override { return false; }
    void clear_error() override {}
    int error_code() const override { return 0; }
    const char* error_message() const override { return "No error"; }
    bool available() const override { return mHasValidFrame || mDecoder->hasMoreFrames(); }
    fl::size_t bytes_left() const override {
        if (!mHasValidFrame) return 0;
        return (mCurrentPos < mFrameSize) ? (mFrameSize - mCurrentPos) : 0;
    }
};

Video FileSystem::openMpeg1Video(const char *path, fl::size pixelsPerFrame, float fps,
                                 fl::size nFrameHistory) {
    Video video(pixelsPerFrame, fps, nFrameHistory);
    fl::ifstream file = openRead(path);
    if (!file.is_open()) { video.setError(fl::string("Could not open MPEG1 file: ").append(path)); return video; }
    Mpeg1Config config;
    config.mode = Mpeg1Config::Streaming;
    config.targetFps = static_cast<fl::u16>(fps);
    config.looping = false;
    config.skipAudio = true;
    fl::string error_message;
    IDecoderPtr decoder = Mpeg1::createDecoder(config, &error_message);
    if (!decoder) { video.setError(fl::string("Failed to create MPEG1 decoder: ").append(error_message)); return video; }
    if (!decoder->begin(file.rdbuf())) {
        fl::string decoder_error; decoder->hasError(&decoder_error);
        video.setError(fl::string("Failed to initialize MPEG1 decoder: ").append(decoder_error)); return video;
    }
    fl::shared_ptr<Mpeg1FileHandle> mpeg1Stream = fl::make_shared<Mpeg1FileHandle>(decoder, pixelsPerFrame, path);
    if (!video.begin(mpeg1Stream)) { video.setError(fl::string("Failed to initialize video with MPEG1 stream")); return video; }
    return video;
}

FramePtr FileSystem::loadJpeg(const char *path, const JpegConfig &config,
                               fl::string *error_message) {
    fl::ifstream file = openRead(path);
    if (!file.is_open()) {
        if (error_message) { *error_message = "Failed to open file: "; error_message->append(path); }
        FL_WARN("Failed to open JPEG file: " << path); return FramePtr();
    }
    fl::size fileSize = file.size();
    if (fileSize == 0) {
        if (error_message) { *error_message = "File is empty: "; error_message->append(path); }
        file.close(); return FramePtr();
    }
    fl::vector<u8> buffer;
    buffer.reserve(fileSize); buffer.resize(fileSize);
    fl::size bytesRead = 0;
    while (bytesRead < fileSize && file.available()) {
        fl::size chunkSize = min<fl::size>(4096, fileSize - bytesRead);
        fl::size n = file.read(buffer.data() + bytesRead, chunkSize);
        if (n == 0) break;
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
        FL_WARN("Failed to read complete JPEG file: " << path); return FramePtr();
    }
    fl::span<const u8> jpegData(buffer.data(), buffer.size());
    FramePtr frame = Jpeg::decode(config, jpegData, error_message);
    if (!frame && error_message && error_message->empty()) {
        *error_message = "Failed to decode JPEG from file: "; error_message->append(path);
    }
    return frame;
}

fl::Mp3DecoderPtr FileSystem::openMp3(const char *path, fl::string *error_message) {
    fl::ifstream file = openRead(path);
    if (!file.is_open()) {
        if (error_message) { *error_message = "Failed to open file: "; error_message->append(path); }
        FL_WARN("Failed to open MP3 file: " << path); return fl::Mp3DecoderPtr();
    }
    fl::Mp3DecoderPtr decoder = fl::Mp3::createDecoder(error_message);
    if (!decoder->begin(file.rdbuf())) {
        fl::string decoder_error; decoder->hasError(&decoder_error);
        if (error_message) { *error_message = "Failed to initialize MP3 decoder: "; error_message->append(decoder_error); }
        FL_WARN("Failed to initialize MP3 decoder for: " << path); return fl::Mp3DecoderPtr();
    }
    return decoder;
}

} // namespace fl
