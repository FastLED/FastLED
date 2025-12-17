#pragma once

/// @file spi/lane.h
/// @brief Lane class for multi-lane SPI devices

#include "fl/stl/stdint.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"

namespace fl {
namespace spi {

// Forward declaration
class MultiLaneDevice;

// ============================================================================
// Lane - Single lane in a multi-lane SPI device
// ============================================================================

/// @brief Single lane in a multi-lane SPI device
/// @details Provides buffer access for one independent data stream in a
///          multi-lane SPI configuration (Dual/Quad/Octal)
/// @note Lane data is buffered until flush() is called on parent device
class Lane {
public:
    /// @brief Write data to this lane's buffer
    /// @param data Data to transmit (copied into internal buffer)
    /// @param size Number of bytes
    /// @note Data is buffered, not transmitted until parent device's flush() is called
    void write(const uint8_t* data, size_t size);

    /// @brief Get direct buffer access for zero-copy writes
    /// @param size Number of bytes needed
    /// @returns Span to write into (valid until flush())
    /// @note Resizes internal buffer to requested size
    fl::span<uint8_t> getBuffer(size_t size);

    /// @brief Get lane ID
    /// @returns Lane index (0-7 for Dual/Quad/Octal)
    size_t id() const { return mLaneId; }

    /// @brief Get current buffer size
    /// @returns Number of bytes currently buffered
    size_t bufferSize() const { return mBuffer.size(); }

    /// @brief Clear the lane's buffer
    void clear() { mBuffer.clear(); }

private:
    friend class MultiLaneDevice;

    /// @brief Construct lane (called by MultiLaneDevice)
    /// @param lane_id Lane index
    /// @param parent Pointer to parent MultiLaneDevice
    Lane(size_t lane_id, MultiLaneDevice* parent);

    /// @brief Get const access to buffer data
    /// @returns Const span of buffered data
    fl::span<const uint8_t> data() const;

    size_t mLaneId;
    fl::vector<uint8_t> mBuffer;  // Buffered data for this lane
};

} // namespace spi
} // namespace fl
