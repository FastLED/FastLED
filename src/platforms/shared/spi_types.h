#pragma once

#include "fl/span.h"
#include "fl/stdint.h"
#include "fl/string.h"
#include "fl/shared_ptr.h"
#include "fl/vector.h"
#include "fl/allocator.h"

namespace fl {

/// @brief Transmission mode for SPI operations
/// @note Most platforms use async DMA-based transmission and ignore SYNC mode
enum class TransmitMode : uint8_t {
    SYNC,   ///< Synchronous/blocking transmission (may not be supported on all platforms)
    ASYNC   ///< Asynchronous/non-blocking transmission (default, uses DMA)
};

/// @brief Error codes for SPI DMA buffer operations
enum class SPIError : uint8_t {
    NOT_INITIALIZED,    ///< SPI hardware not initialized
    BUFFER_TOO_LARGE,   ///< Requested buffer size exceeds platform maximum
    ALLOCATION_FAILED,  ///< Memory allocation failed
    BUSY                ///< Previous transmission still in progress
};

/// @brief Result type for DMA buffer acquisition
/// @details Returns either a valid buffer span or an error code
/// @note Buffer uses shared_ptr for lifetime management (important for async DMA)
/// @note Internally holds fl::vector with allocator_psram for efficient DMA operations
struct DMABuffer {
private:
    /// @brief Internal data structure holding the actual buffer
    /// @details Uses fl::vector with PSRAM allocator for DMA-compatible memory
    struct InternalData {
        fl::vector<uint8_t, allocator_psram<uint8_t>> buffer;

        InternalData() : buffer() {}

        explicit InternalData(size_t size) : buffer(size) {}
    };

    fl::shared_ptr<InternalData> m_internal;  ///< Shared ownership of internal data
    SPIError error_code;
    bool is_ok;

public:
    /// @brief Default constructor (uninitialized error state)
    DMABuffer()
        : m_internal(), error_code(SPIError::NOT_INITIALIZED), is_ok(false) {}

    /// @brief Construct successful result with buffer size
    /// @param size Size of buffer in bytes to allocate
    explicit DMABuffer(size_t size)
        : m_internal(fl::make_shared<InternalData>(size)),
          error_code(SPIError::NOT_INITIALIZED),
          is_ok(m_internal != nullptr) {
        if (!is_ok) {
            error_code = SPIError::ALLOCATION_FAILED;
        }
    }

    /// @brief Construct successful result with buffer pointer and size (legacy compatibility)
    /// @param ptr Shared pointer to buffer data
    /// @param size Size of buffer in bytes
    /// @deprecated Use DMABuffer(size_t) constructor instead
    DMABuffer(fl::shared_ptr<uint8_t> ptr, size_t size)
        : m_internal(fl::make_shared<InternalData>()),
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

    /// @brief Construct error result
    explicit DMABuffer(SPIError err)
        : m_internal(), error_code(err), is_ok(false) {}

    /// @brief Check if buffer acquisition succeeded
    bool ok() const { return is_ok && m_internal != nullptr; }

    /// @brief Get the buffer span (only valid if ok() returns true)
    fl::span<uint8_t> data() const {
        if (!ok()) {
            return fl::span<uint8_t>();
        }
        return fl::span<uint8_t>(m_internal->buffer.data(), m_internal->buffer.size());
    }

    /// @brief Get the buffer span (only valid if ok() returns true)
    /// @note Alias for data() for compatibility
    fl::span<uint8_t> buffer() const { return data(); }

    /// @brief Get the shared pointer to buffer data (for legacy compatibility)
    /// @deprecated Direct pointer access is discouraged; use data() span instead
    /// @note This returns nullptr as the internal buffer is managed by InternalData
    fl::shared_ptr<uint8_t> ptr() const {
        // Legacy compatibility - return nullptr as buffer is internally managed
        return fl::shared_ptr<uint8_t>();
    }

    /// @brief Get the error code (only meaningful if ok() returns false)
    SPIError error() const { return error_code; }

    /// @brief Reset/clear the buffer (invalidates internal data)
    void reset() {
        m_internal.reset();
        is_ok = false;
        error_code = SPIError::NOT_INITIALIZED;
    }

    /// @brief Get the size of the buffer in bytes
    size_t size() const {
        if (!ok()) {
            return 0;
        }
        return m_internal->buffer.size();
    }
};

/// @brief Request structure for SPI transmit operations
/// @details Contains the DMA buffer and transmission mode
/// @warning This structure is consumed by transmit(). After calling spi.transmit(&request),
///          the buffer's shared_ptr will be null and the span will be empty.
struct SPITransmitRequest {
    DMABuffer buffer;
    TransmitMode mode;

    /// @brief Default constructor
    SPITransmitRequest()
        : buffer(), mode(TransmitMode::ASYNC) {}

    /// @brief Construct with buffer and mode
    SPITransmitRequest(const DMABuffer& buf, TransmitMode m = TransmitMode::ASYNC)
        : buffer(buf), mode(m) {}

    /// @brief Move the buffer out of this request (consumes the buffer)
    /// @details This is called internally by transmit() to take ownership
    /// @return The buffer with ownership transferred
    DMABuffer take_buffer() {
        DMABuffer result = buffer;
        buffer.reset();  // Clear the buffer
        return result;
    }

    /// @brief Check if the request still has a valid buffer
    bool has_buffer() const {
        return buffer.ok() && buffer.ptr() != nullptr;
    }
};

/// @brief Result structure for SPI transmit operations
/// @details Contains success/error status, error message, and error code
struct SPITransmitResult {
    bool is_ok;
    fl::string error_message;
    SPIError error_code;

    /// @brief Default constructor (successful transmission)
    SPITransmitResult()
        : is_ok(true), error_message(), error_code(SPIError::NOT_INITIALIZED) {}

    /// @brief Construct successful result
    static SPITransmitResult success() {
        return SPITransmitResult();
    }

    /// @brief Construct error result with code and message
    static SPITransmitResult error(SPIError err, const fl::string& msg) {
        SPITransmitResult result;
        result.is_ok = false;
        result.error_code = err;
        result.error_message = msg;
        return result;
    }

    /// @brief Construct error result with code only
    static SPITransmitResult error(SPIError err) {
        return error(err, fl::string());
    }

    /// @brief Check if transmission succeeded
    bool ok() const { return is_ok; }

    /// @brief Get error code (only meaningful if ok() returns false)
    SPIError error() const { return error_code; }

    /// @brief Get error message (only meaningful if ok() returns false)
    const fl::string& message() const { return error_message; }
};

}  // namespace fl
