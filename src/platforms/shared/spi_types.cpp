#include "spi_types.h"
#include "fl/stl/vector.h"
#include "fl/stl/allocator.h"
#include "fl/stl/string.h"
#include "fl/stl/strstream.h"

namespace fl {

// Define the internal data structure
struct DMABufferInternalData {
    fl::vector<uint8_t, allocator_psram<uint8_t>> buffer;

    DMABufferInternalData() : buffer() {}

    explicit DMABufferInternalData(size_t size) : buffer(size) {}
};

// DMABuffer implementations

DMABuffer::DMABuffer()
    : m_internal(), error_code(SPIError::NOT_INITIALIZED), is_ok(false) {}

DMABuffer::DMABuffer(size_t size)
    : m_internal(fl::make_shared<DMABufferInternalData>(size)),
      error_code(SPIError::NOT_INITIALIZED),
      is_ok(m_internal != nullptr) {
    if (!is_ok) {
        error_code = SPIError::ALLOCATION_FAILED;
    }
}

DMABuffer::DMABuffer(fl::shared_ptr<uint8_t> ptr, size_t size)
    : m_internal(fl::make_shared<DMABufferInternalData>()),
      error_code(SPIError::NOT_INITIALIZED),
      is_ok(true) {
    if (m_internal && ptr && size > 0) {
        // Copy data from external pointer into internal vector
        m_internal->buffer.resize(size);
        fl::memcpy(m_internal->buffer.data(), ptr.get(), size);
    } else {
        is_ok = false;
        error_code = SPIError::ALLOCATION_FAILED;
    }
}

DMABuffer::DMABuffer(SPIError err)
    : m_internal(), error_code(err), is_ok(false) {}

bool DMABuffer::ok() const {
    return is_ok && m_internal != nullptr;
}

fl::span<uint8_t> DMABuffer::data() const {
    if (!ok()) {
        return fl::span<uint8_t>();
    }
    return fl::span<uint8_t>(m_internal->buffer.data(), m_internal->buffer.size());
}

SPIError DMABuffer::error() const {
    return error_code;
}

void DMABuffer::reset() {
    m_internal.reset();
    is_ok = false;
    error_code = SPIError::NOT_INITIALIZED;
}

size_t DMABuffer::size() const {
    if (!ok()) {
        return 0;
    }
    return m_internal->buffer.size();
}

// SPITransmitRequest implementations

SPITransmitRequest::SPITransmitRequest()
    : buffer(), mode(TransmitMode::ASYNC) {}

SPITransmitRequest::SPITransmitRequest(const DMABuffer& buf, TransmitMode m)
    : buffer(buf), mode(m) {}

DMABuffer SPITransmitRequest::take_buffer() {
    DMABuffer result = buffer;
    buffer.reset();  // Clear the buffer
    return result;
}

bool SPITransmitRequest::has_buffer() const {
    return buffer.ok();
}

// SPITransmitResult implementations

SPITransmitResult::SPITransmitResult()
    : is_ok(true), error_message(), error_code(SPIError::NOT_INITIALIZED) {}

SPITransmitResult SPITransmitResult::success() {
    return SPITransmitResult();
}

SPITransmitResult SPITransmitResult::error(SPIError err, const fl::string& msg) {
    SPITransmitResult result;
    result.is_ok = false;
    result.error_code = err;
    result.error_message = msg;
    return result;
}

SPITransmitResult SPITransmitResult::error(SPIError err) {
    return error(err, fl::string());
}

bool SPITransmitResult::ok() const {
    return is_ok;
}

SPIError SPITransmitResult::error() const {
    return error_code;
}

const fl::string& SPITransmitResult::message() const {
    return error_message;
}

// Stream operator for SPIError
StrStream& operator<<(StrStream& s, SPIError err) {
    return s << static_cast<int>(err);
}

}  // namespace fl
