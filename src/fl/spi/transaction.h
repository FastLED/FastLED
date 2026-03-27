#pragma once

/// @file spi/transaction.h
/// @brief Transaction class for asynchronous SPI operations
///
/// This file contains the Transaction class which provides a handle for
/// asynchronous SPI operations with RAII semantics.

#include "fl/stl/limits.h"
#include "fl/stl/stdint.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/result.h"
#include "fl/stl/optional.h"
#include "fl/task/promise.h"  // for fl::task::Error
#include "fl/stl/noexcept.h"

namespace fl {

// Forward declaration for SPIError
enum class SPIError : u8;

namespace spi {

// Forward declaration
class Device;

// Type alias for SPI-specific Result type (matches spi.h)
template<typename T>
using Result = fl::result<T, SPIError>;

// ============================================================================
// Transaction Class (Async Operations)
// ============================================================================

/// @brief Handle for asynchronous SPI operations
/// @details RAII wrapper that automatically waits on destruction if not completed
/// @note Move-only type (non-copyable)
class Transaction {
public:
    /// @brief Wait for transaction to complete
    /// @param timeout_ms Maximum time to wait
    /// @returns true if completed, false on timeout
    bool wait(u32 timeout_ms = (fl::numeric_limits<u32>::max)());

    /// @brief Check if transaction is complete
    /// @returns true if done, false if still in progress
    bool isDone() const;

    /// @brief Check if transaction is still in progress
    /// @returns true if pending, false if complete
    bool isPending() const;

    /// @brief Cancel pending transaction (if supported by platform)
    /// @returns true if cancelled, false if not cancellable
    /// @note Not all platforms support cancellation
    bool cancel();

    /// @brief Get result of completed transaction
    /// @returns Result indicating success or error
    /// @warning Only valid after isDone() returns true
    fl::optional<fl::task::Error> getResult() const;

    /// @brief Destructor - automatically waits for completion
    /// @note Ensures transaction completes before destruction
    ~Transaction();

    // Move semantics
    Transaction(Transaction&& other) FL_NOEXCEPT;
    Transaction& operator=(Transaction&& other) FL_NOEXCEPT;

private:
    friend class Device;

    // Private constructor for Device to create transactions
    Transaction() FL_NOEXCEPT;

    struct Impl;
    fl::unique_ptr<Impl> pImpl;

    // Non-copyable
    Transaction(const Transaction&) FL_NOEXCEPT = delete;
    Transaction& operator=(const Transaction&) FL_NOEXCEPT = delete;
};

} // namespace spi
} // namespace fl
