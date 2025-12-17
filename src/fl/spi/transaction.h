#pragma once

/// @file spi/transaction.h
/// @brief Transaction class for asynchronous SPI operations
///
/// This file contains the Transaction class which provides a handle for
/// asynchronous SPI operations with RAII semantics.

#include "fl/stl/stdint.h"
#include "fl/stl/unique_ptr.h"
#include "fl/result.h"
#include "fl/stl/optional.h"
#include "fl/promise.h"  // for fl::Error
#include "fl/numeric_limits.h"

namespace fl {

// Forward declaration for SPIError
enum class SPIError : uint8_t;

namespace spi {

// Forward declaration
class Device;

// Type alias for SPI-specific Result type (matches spi.h)
template<typename T>
using Result = fl::Result<T, SPIError>;

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
    bool wait(uint32_t timeout_ms = (fl::numeric_limits<uint32_t>::max)());

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
    fl::optional<fl::Error> getResult() const;

    /// @brief Destructor - automatically waits for completion
    /// @note Ensures transaction completes before destruction
    ~Transaction();

    // Move semantics
    Transaction(Transaction&& other) noexcept;
    Transaction& operator=(Transaction&& other) noexcept;

private:
    friend class Device;

    // Private constructor for Device to create transactions
    Transaction();

    struct Impl;
    fl::unique_ptr<Impl> pImpl;

    // Non-copyable
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
};

} // namespace spi
} // namespace fl
