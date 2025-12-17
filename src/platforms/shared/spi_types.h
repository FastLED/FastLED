#pragma once

#include "fl/stl/span.h"
#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "fl/stl/shared_ptr.h"

namespace fl {

// Forward declaration for streaming support
class StrStream;

// Forward declaration
struct DMABufferInternalData;

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
    BUSY,               ///< Previous transmission still in progress
    NOT_SUPPORTED,      ///< Operation not supported (e.g., RX on TX-only SPI)
    INVALID_PARAMETER   ///< Invalid parameter provided (e.g., lane size mismatch)
};

/// @brief Stream operator for SPIError (enables use with FL_DBG/FL_WARN macros)
/// @note Defined in spi_types.cpp
StrStream& operator<<(StrStream& s, SPIError err);

/// @brief Result type for DMA buffer acquisition
/// @details Returns either a valid buffer span or an error code
/// @note Buffer uses shared_ptr for lifetime management (important for async DMA)
/// @note Internally holds fl::vector with allocator_psram for efficient DMA operations
struct DMABuffer {
private:
    fl::shared_ptr<DMABufferInternalData> m_internal;  ///< Shared ownership of internal data
    SPIError error_code;
    bool is_ok;

public:
    /// @brief Default constructor (uninitialized error state)
    DMABuffer();

    /// @brief Construct successful result with buffer size
    /// @param size Size of buffer in bytes to allocate
    explicit DMABuffer(size_t size);

    /// @brief Construct successful result with buffer pointer and size (legacy compatibility)
    /// @param ptr Shared pointer to buffer data
    /// @param size Size of buffer in bytes
    /// @deprecated Use DMABuffer(size_t) constructor instead
    DMABuffer(fl::shared_ptr<uint8_t> ptr, size_t size);

    /// @brief Construct error result
    explicit DMABuffer(SPIError err);

    /// @brief Check if buffer acquisition succeeded
    bool ok() const;

    /// @brief Get the buffer span (only valid if ok() returns true)
    fl::span<uint8_t> data() const;

    /// @brief Get the error code (only meaningful if ok() returns false)
    SPIError error() const;

    /// @brief Reset/clear the buffer (invalidates internal data)
    void reset();

    /// @brief Get the size of the buffer in bytes
    size_t size() const;
};

/// @brief Request structure for SPI transmit operations
/// @details Contains the DMA buffer and transmission mode
/// @warning This structure is consumed by transmit(). After calling spi.transmit(&request),
///          the buffer's shared_ptr will be null and the span will be empty.
struct SPITransmitRequest {
    DMABuffer buffer;
    TransmitMode mode;

    /// @brief Default constructor
    SPITransmitRequest();

    /// @brief Construct with buffer and mode
    SPITransmitRequest(const DMABuffer& buf, TransmitMode m = TransmitMode::ASYNC);

    /// @brief Move the buffer out of this request (consumes the buffer)
    /// @details This is called internally by transmit() to take ownership
    /// @return The buffer with ownership transferred
    DMABuffer take_buffer();

    /// @brief Check if the request still has a valid buffer
    bool has_buffer() const;
};

/// @brief Result structure for SPI transmit operations
/// @details Contains success/error status, error message, and error code
struct SPITransmitResult {
    bool is_ok;
    fl::string error_message;
    SPIError error_code;

    /// @brief Default constructor (successful transmission)
    SPITransmitResult();

    /// @brief Construct successful result
    static SPITransmitResult success();

    /// @brief Construct error result with code and message
    static SPITransmitResult error(SPIError err, const fl::string& msg);

    /// @brief Construct error result with code only
    static SPITransmitResult error(SPIError err);

    /// @brief Check if transmission succeeded
    bool ok() const;

    /// @brief Get error code (only meaningful if ok() returns false)
    SPIError error() const;

    /// @brief Get error message (only meaningful if ok() returns false)
    const fl::string& message() const;
};

}  // namespace fl
