#pragma once

/// @file spi/write_result.h
/// @brief Result type for SPI write operations

#include "fl/str.h"
#include "fl/stl/stdint.h"

namespace fl {

/// @brief Result of a write operation
/// @details Indicates success or failure of write operation. Use fl::Spi::wait() to block until complete.
struct WriteResult {
    bool ok;                ///< True if write succeeded, false if error
    fl::string error;       ///< Error message (empty if ok == true)

    WriteResult() : ok(true) {}
    explicit WriteResult(const char* err) : ok(false), error(err) {}
    explicit WriteResult(const fl::string& err) : ok(false), error(err) {}

    /// @brief Implicit conversion to bool for easy checking
    operator bool() const { return ok; }
};

} // namespace fl
