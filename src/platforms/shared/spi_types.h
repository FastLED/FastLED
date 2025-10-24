#pragma once

#include "fl/span.h"
#include "fl/stdint.h"

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
struct DMABufferResult {
    fl::span<uint8_t> buffer_data;
    SPIError error_code;
    bool is_ok;

    /// @brief Default constructor (uninitialized error state)
    DMABufferResult()
        : buffer_data(), error_code(SPIError::NOT_INITIALIZED), is_ok(false) {}

    /// @brief Construct successful result with buffer
    DMABufferResult(fl::span<uint8_t> buf)
        : buffer_data(buf), error_code(SPIError::NOT_INITIALIZED), is_ok(true) {}

    /// @brief Construct error result
    DMABufferResult(SPIError err)
        : buffer_data(), error_code(err), is_ok(false) {}

    /// @brief Check if buffer acquisition succeeded
    bool ok() const { return is_ok; }

    /// @brief Get the buffer (only valid if ok() returns true)
    fl::span<uint8_t> buffer() const { return buffer_data; }

    /// @brief Get the error code (only meaningful if ok() returns false)
    SPIError error() const { return error_code; }
};

}  // namespace fl
