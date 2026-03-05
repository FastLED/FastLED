#pragma once

#include "fl/stl/detail/file_handle.h"
#include "fl/stl/circular_buffer.h"
#include "fl/stl/span.h"
#include "fl/math_macros.h"

namespace fl {
namespace detail {

// In-memory file handle backed by a circular buffer.
// Replaces ByteStreamMemory. Non-seekable (pipe/socket-like).
// Write pushes to buffer, read pops from buffer.
class MemoryFileHandle : public fl::FileHandle {
public:
    explicit MemoryFileHandle(fl::u32 capacity)
        : mBuffer(capacity), mTotalWritten(0) {}

    ~MemoryFileHandle() override = default;

    bool is_open() const override { return true; }

    void close() override { mBuffer.clear(); }

    fl::size_t read(char* buffer, fl::size_t count) override {
        if (!buffer || count == 0) return 0;
        fl::size_t actual = FL_MIN(count, mBuffer.size());
        for (fl::size_t i = 0; i < actual; ++i) {
            fl::u8 b;
            mBuffer.pop_front(&b);
            buffer[i] = static_cast<char>(b);
        }
        return actual;
    }
    using FileHandle::read; // u8 overload

    fl::size_t write(const char* data, fl::size_t count) override {
        if (!data || count == 0 || mBuffer.capacity() == 0) return 0;
        fl::size_t written = 0;
        for (fl::size_t i = 0; i < count; ++i) {
            if (mBuffer.full()) break;
            mBuffer.push_back(static_cast<fl::u8>(data[i]));
            ++written;
        }
        mTotalWritten += written;
        return written;
    }

    // Convenience: write u8 data
    fl::size_t write(fl::span<const fl::u8> data) {
        return write(reinterpret_cast<const char*>(data.data()), data.size()); // ok reinterpret cast
    }

    // Convenience: write CRGB pixels
    fl::size_t writeCRGB(const CRGB* src, fl::size_t n) {
        fl::size_t bytes_written = write(reinterpret_cast<const char*>(src), n * 3); // ok reinterpret cast
        return bytes_written / 3;
    }

    fl::size_t tell() override { return 0; } // Not meaningful for circular buffer

    bool seek(fl::size_t, seek_dir) override { return false; } // Non-seekable
    using FileHandle::seek;

    fl::size_t size() const override { return mBuffer.size(); }

    const char* path() const override { return "MemoryFileHandle"; }

    bool is_eof() const override { return mBuffer.empty(); }

    bool has_error() const override { return false; }
    void clear_error() override {}
    int error_code() const override { return 0; }
    const char* error_message() const override { return "No error"; }

    bool available() const override { return !mBuffer.empty(); }

    fl::size_t bytes_left() const override { return mBuffer.size(); }

    void clear() { mBuffer.clear(); }

    fl::size_t capacity() const { return mBuffer.capacity(); }

private:
    circular_buffer<fl::u8> mBuffer;
    fl::size_t mTotalWritten;
};

} // namespace detail

// Convenience alias
using MemoryFileHandle = detail::MemoryFileHandle;
FASTLED_SHARED_PTR_NO_FWD(MemoryFileHandle);

} // namespace fl
