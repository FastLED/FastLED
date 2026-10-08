#pragma once

#include "fl/fs/file_handle.h"
#include "fl/stl/circular_buffer.h"
#include "fl/stl/span.h"
#include "fl/math/math.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

// In-memory file buffer backed by a circular buffer.
// Non-seekable (pipe/socket-like). Write pushes to buffer, read pops from buffer.
// Analogous to std::stringbuf but for circular byte streams.
class memorybuf : public fl::filebuf {
public:
    explicit memorybuf(fl::u32 capacity) FL_NO_EXCEPT
        : mBuffer(capacity), mTotalWritten(0) {}

    ~memorybuf() FL_NO_EXCEPT override = default;

    bool is_open() const FL_NO_EXCEPT override { return true; }

    void close() FL_NO_EXCEPT override { mBuffer.clear(); }

    fl::size_t read(char* buffer, fl::size_t count) FL_NO_EXCEPT override {
        if (!buffer || count == 0) return 0;
        fl::size_t actual = FL_MIN(count, mBuffer.size());
        for (fl::size_t i = 0; i < actual; ++i) {
            fl::u8 b = 0;
            mBuffer.pop_front(&b);
            buffer[i] = static_cast<char>(b);
        }
        return actual;
    }
    using filebuf::read; // u8 overload

    fl::size_t write(const char* data, fl::size_t count) FL_NO_EXCEPT override {
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
    fl::size_t write(fl::span<const fl::u8> data) FL_NO_EXCEPT {
        return write(reinterpret_cast<const char*>(data.data()), data.size()); // ok reinterpret cast
    }

    // Convenience: write CRGB pixels
    fl::size_t writeCRGB(const CRGB* src, fl::size_t n) FL_NO_EXCEPT {
        fl::size_t bytes_written = write(reinterpret_cast<const char*>(src), n * 3); // ok reinterpret cast
        return bytes_written / 3;
    }

    fl::size_t tell() FL_NO_EXCEPT override { return 0; } // Not meaningful for circular buffer

    bool seek(fl::size_t, seek_dir) FL_NO_EXCEPT override { return false; } // Non-seekable
    using filebuf::seek;

    fl::size_t size() const FL_NO_EXCEPT override { return mBuffer.size(); }

    const char* path() const FL_NO_EXCEPT override { return "memorybuf"; }

    bool is_eof() const FL_NO_EXCEPT override { return mBuffer.empty(); }

    bool has_error() const FL_NO_EXCEPT override { return false; }
    void clear_error() FL_NO_EXCEPT override {}
    int error_code() const FL_NO_EXCEPT override { return 0; }
    const char* error_message() const FL_NO_EXCEPT override { return "No error"; }

    bool available() const FL_NO_EXCEPT override { return !mBuffer.empty(); }

    fl::size_t bytes_left() const FL_NO_EXCEPT override { return mBuffer.size(); }

    void clear() FL_NO_EXCEPT { mBuffer.clear(); }

    fl::size_t capacity() const FL_NO_EXCEPT { return mBuffer.capacity(); }

private:
    circular_buffer<fl::u8> mBuffer;
    fl::size_t mTotalWritten;
};

} // namespace detail

// Public alias
using memorybuf = detail::memorybuf;
FASTLED_SHARED_PTR_NO_FWD(memorybuf);

} // namespace fl
