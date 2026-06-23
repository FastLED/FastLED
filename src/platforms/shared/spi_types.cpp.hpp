// IWYU pragma: private

#include "platforms/shared/spi_types.h"
#include "fl/stl/vector.h"
#include "fl/stl/allocator.h"
#include "fl/stl/string.h"
#include "fl/stl/strstream.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Define the internal data structure
struct DMABufferInternalData {
    fl::vector_psram<u8> buffer;

    DMABufferInternalData() FL_NOEXCEPT : buffer() {}

    explicit DMABufferInternalData(size_t size) FL_NOEXCEPT : buffer(size) {}
};

// DMABuffer implementations

DMABuffer::DMABuffer()
 FL_NOEXCEPT : mInternal(), error_code(SPIError::NOT_INITIALIZED), is_ok(false) {}

DMABuffer::DMABuffer(size_t size)
 FL_NOEXCEPT : mInternal(fl::make_shared<DMABufferInternalData>(size)),
      error_code(SPIError::NOT_INITIALIZED),
      is_ok(mInternal != nullptr) {
    if (!is_ok) {
        error_code = SPIError::ALLOCATION_FAILED;
    }
}

DMABuffer::DMABuffer(fl::shared_ptr<u8> ptr, size_t size)
 FL_NOEXCEPT : mInternal(fl::make_shared<DMABufferInternalData>()),
      error_code(SPIError::NOT_INITIALIZED),
      is_ok(true) {
    if (mInternal && ptr && size > 0) {
        // Copy data from external pointer into internal vector
        mInternal->buffer.resize(size);
        fl::memcpy(mInternal->buffer.data(), ptr.get(), size);
    } else {
        is_ok = false;
        error_code = SPIError::ALLOCATION_FAILED;
    }
}

DMABuffer::DMABuffer(SPIError err)
 FL_NOEXCEPT : mInternal(), error_code(err), is_ok(false) {}

bool DMABuffer::ok() const FL_NOEXCEPT {
    return is_ok && mInternal != nullptr;
}

fl::span<u8> DMABuffer::data() const FL_NOEXCEPT {
    if (!ok()) {
        return fl::span<u8>();
    }
    return mInternal->buffer;
}

SPIError DMABuffer::error() const FL_NOEXCEPT {
    return error_code;
}

void DMABuffer::reset() FL_NOEXCEPT {
    mInternal.reset();
    is_ok = false;
    error_code = SPIError::NOT_INITIALIZED;
}

size_t DMABuffer::size() const FL_NOEXCEPT {
    if (!ok()) {
        return 0;
    }
    return mInternal->buffer.size();
}

// SPITransmitRequest implementations

SPITransmitRequest::SPITransmitRequest()
 FL_NOEXCEPT : buffer(), mode(TransmitMode::ASYNC) {}

SPITransmitRequest::SPITransmitRequest(const DMABuffer& buf, TransmitMode m)
 FL_NOEXCEPT : buffer(buf), mode(m) {}

DMABuffer SPITransmitRequest::take_buffer() FL_NOEXCEPT {
    DMABuffer result = buffer;
    buffer.reset();  // Clear the buffer
    return result;
}

bool SPITransmitRequest::has_buffer() const FL_NOEXCEPT {
    return buffer.ok();
}

// SPITransmitResult implementations

SPITransmitResult::SPITransmitResult()
 FL_NOEXCEPT : is_ok(true), error_message(), error_code(SPIError::NOT_INITIALIZED) {}

SPITransmitResult SPITransmitResult::success() FL_NOEXCEPT {
    return SPITransmitResult();
}

SPITransmitResult SPITransmitResult::error(SPIError err, const fl::string& msg) FL_NOEXCEPT {
    SPITransmitResult result;
    result.is_ok = false;
    result.error_code = err;
    result.error_message = msg;
    return result;
}

SPITransmitResult SPITransmitResult::error(SPIError err) FL_NOEXCEPT {
    return error(err, fl::string());
}

bool SPITransmitResult::ok() const FL_NOEXCEPT {
    return is_ok;
}

SPIError SPITransmitResult::error() const FL_NOEXCEPT {
    return error_code;
}

const fl::string& SPITransmitResult::message() const FL_NOEXCEPT {
    return error_message;
}

// Stream operator for SPIError
sstream& operator<<(sstream& s, SPIError err) FL_NOEXCEPT {
    return s << static_cast<int>(err);
}

}  // namespace fl
