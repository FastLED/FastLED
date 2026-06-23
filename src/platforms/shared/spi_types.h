#pragma once

// IWYU pragma: private

#include "fl/stl/span.h"
#include "fl/stl/stdint.h"
#include "fl/stl/string.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Forward declaration for streaming support
class sstream;

// Forward declaration
struct DMABufferInternalData;

/// @brief Transmission mode for SPI operations
/// @note Most platforms use async DMA-based transmission and ignore SYNC mode
enum class TransmitMode : u8 {
    SYNC,   ///< Synchronous/blocking transmission (may not be supported on all platforms)
    ASYNC   ///< Asynchronous/non-blocking transmission (default, uses DMA)
};

/// @brief Error codes for SPI DMA buffer operations
enum class SPIError : u8 {
    NOT_INITIALIZED,    ///< SPI hardware not initialized
    BUFFER_TOO_LARGE,   ///< Requested buffer size exceeds platform maximum
    ALLOCATION_FAILED,  ///< Memory allocation failed
    BUSY,               ///< Previous transmission still in progress
    NOT_SUPPORTED,      ///< Operation not supported (e.g., RX on TX-only SPI)
    INVALID_PARAMETER   ///< Invalid parameter provided (e.g., lane size mismatch)
};

/// @brief Stream operator for SPIError (enables use with FL_DBG/FL_WARN macros)
/// @note Defined in spi_types.cpp
sstream& operator<<(sstream& s, SPIError err) FL_NOEXCEPT;

/// @brief Result type for DMA buffer acquisition
/// @details Returns either a valid buffer span or an error code
/// @note Buffer uses shared_ptr for lifetime management (important for async DMA)
/// @note Internally holds fl::vector with allocator_psram for efficient DMA operations
struct DMABuffer {
private:
    fl::shared_ptr<DMABufferInternalData> mInternal;  ///< Shared ownership of internal data
    SPIError error_code;
    bool is_ok;

public:
    /// @brief Default constructor (uninitialized error state)
    DMABuffer() FL_NOEXCEPT;

    /// @brief Construct successful result with buffer size
    /// @param size Size of buffer in bytes to allocate
    explicit DMABuffer(size_t size) FL_NOEXCEPT;

    /// @brief Construct successful result with buffer pointer and size (legacy compatibility)
    /// @param ptr Shared pointer to buffer data
    /// @param size Size of buffer in bytes
    /// @deprecated Use DMABuffer(size_t) constructor instead
    DMABuffer(fl::shared_ptr<u8> ptr, size_t size) FL_NOEXCEPT;

    /// @brief Construct error result
    explicit DMABuffer(SPIError err) FL_NOEXCEPT;

    /// @brief Check if buffer acquisition succeeded
    bool ok() const FL_NOEXCEPT;

    /// @brief Get the buffer span (only valid if ok() returns true)
    fl::span<u8> data() const FL_NOEXCEPT;

    /// @brief Get the error code (only meaningful if ok() returns false)
    SPIError error() const FL_NOEXCEPT;

    /// @brief Reset/clear the buffer (invalidates internal data)
    void reset() FL_NOEXCEPT;

    /// @brief Get the size of the buffer in bytes
    size_t size() const FL_NOEXCEPT;
};

/// @brief Request structure for SPI transmit operations
/// @details Contains the DMA buffer and transmission mode
/// @warning This structure is consumed by transmit(). After calling spi.transmit(&request),
///          the buffer's shared_ptr will be null and the span will be empty.
struct SPITransmitRequest {
    DMABuffer buffer;
    TransmitMode mode;

    /// @brief Default constructor
    SPITransmitRequest() FL_NOEXCEPT;

    /// @brief Construct with buffer and mode
    SPITransmitRequest(const DMABuffer& buf, TransmitMode m = TransmitMode::ASYNC) FL_NOEXCEPT;

    /// @brief Move the buffer out of this request (consumes the buffer)
    /// @details This is called internally by transmit() to take ownership
    /// @return The buffer with ownership transferred
    DMABuffer take_buffer() FL_NOEXCEPT;

    /// @brief Check if the request still has a valid buffer
    bool has_buffer() const FL_NOEXCEPT;
};

/// @brief Result structure for SPI transmit operations
/// @details Contains success/error status, error message, and error code
struct SPITransmitResult {
    bool is_ok;
    fl::string error_message;
    SPIError error_code;

    /// @brief Default constructor (successful transmission)
    SPITransmitResult() FL_NOEXCEPT;

    /// @brief Construct successful result
    static SPITransmitResult success() FL_NOEXCEPT;

    /// @brief Construct error result with code and message
    static SPITransmitResult error(SPIError err, const fl::string& msg) FL_NOEXCEPT;

    /// @brief Construct error result with code only
    static SPITransmitResult error(SPIError err) FL_NOEXCEPT;

    /// @brief Check if transmission succeeded
    bool ok() const FL_NOEXCEPT;

    /// @brief Get error code (only meaningful if ok() returns false)
    SPIError error() const FL_NOEXCEPT;

    /// @brief Get error message (only meaningful if ok() returns false)
    const fl::string& message() const FL_NOEXCEPT;
};

}  // namespace fl
