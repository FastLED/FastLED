/// @file data.h
/// @brief Channel transmission data - lightweight DTO for engine transmission

#pragma once

#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/function.h"
#include "fl/stl/span.h"
#include "fl/channels/config.h"

namespace fl {

class ChannelData;
FASTLED_SHARED_PTR(ChannelData);

/// @brief Padding generator function type
///
/// Called by writeWithPadding() to write source data with padding to destination buffer.
/// The function receives the original encoded data (src) and writes to the destination (dst)
/// with any necessary padding applied (e.g., inserting zero bytes after a preamble for block alignment).
///
/// Default behavior (if no generator set): Left-pad with zeros, then memcopy data.
/// Layout: [PADDING (zeros)][LED DATA] - padding bytes transmit to non-existent pixels first.
///
/// @param src Source encoded data (read-only)
/// @param dst Destination buffer to write to (dst.size() >= src.size())
using PaddingGenerator = fl::function<void(fl::span<const uint8_t> src, fl::span<uint8_t> dst)>;

/// @brief Transmission data for a single LED channel
///
/// This lightweight data transfer object holds everything the engine needs
/// to transmit LED data: pin number, timing configuration, and encoded bytes.
/// Separated from Channel to allow concurrent transmission while channels
/// prepare next frame.
class ChannelData {
public:
    /// @brief Create channel transmission data
    /// @param pin GPIO pin number for this channel
    /// @param timing Chipset timing configuration (T0H, T1H, T0L, reset)
    /// @param encodedData Encoded byte stream ready for transmission (defaults to empty)
    static ChannelDataPtr create(
        int pin,
        const ChipsetTimingConfig& timing,
        fl::vector_psram<uint8_t>&& encodedData = fl::vector_psram<uint8_t>()
    );

    /// @brief Get the GPIO pin number
    int getPin() const { return mPin; }

    /// @brief Get the timing configuration
    const ChipsetTimingConfig& getTiming() const { return mTiming; }

    /// @brief Get the encoded transmission data
    const fl::vector_psram<uint8_t>& getData() const { return mEncodedData; }

    /// @brief Get the encoded transmission data (mutable)
    fl::vector_psram<uint8_t>& getData() { return mEncodedData; }

    /// @brief Get the data size in bytes
    size_t getSize() const { return mEncodedData.size(); }

    /// @brief Check if channel data is currently in use by the engine
    /// @return true if engine is transmitting this data, false otherwise
    bool isInUse() const { return mInUse; }

    /// @brief Mark channel data as in use by the engine
    /// @param inUse true to mark as in use, false to mark as available
    void setInUse(bool inUse) { mInUse = inUse; }

    /// @brief Set the padding generator for this channel
    /// @param generator Function that writes data with padding to destination (nullptr for default left-padding)
    void setPaddingGenerator(PaddingGenerator generator) {
        mPaddingGenerator = fl::move(generator);
    }

    /// @brief Write encoded data with padding to destination buffer
    ///
    /// This method separates the concern of data preparation from memory format.
    /// It writes the encoded data with padding applied to a caller-provided span,
    /// allowing the caller to control the destination memory type (DRAM, DMA, etc.)
    ///
    /// @param dst Destination buffer to write to (size determines target padding size)
    ///
    /// The destination buffer size must be >= current data size. If a padding
    /// generator is configured, it will be used to extend the data to fill the
    /// entire destination buffer.
    void writeWithPadding(fl::span<uint8_t> dst);

    /// @brief Calculate the size needed for writeWithPadding() without allocating
    ///
    /// Returns the current data size without applying padding. The actual size
    /// after writeWithPadding() will be dst.size() (fills entire destination).
    ///
    /// @return Current size of encoded data (minimum required dst size)
    size_t getMinimumSize() const { return mEncodedData.size(); }

    /// @brief Destructor with debug logging
    ~ChannelData();

private:
    /// @brief Friend declaration for make_shared to access private constructor
    template<typename T, typename... Args>
    friend fl::shared_ptr<T> fl::make_shared(Args&&... args);

    /// @brief Private constructor (use create() factory method)
    ChannelData(
        int pin,
        const ChipsetTimingConfig& timing,
        fl::vector_psram<uint8_t>&& encodedData
    );

    // Non-copyable (move-only via shared_ptr)
    ChannelData(const ChannelData&) = delete;
    ChannelData& operator=(const ChannelData&) = delete;

    const int mPin;                         ///< GPIO pin number
    const ChipsetTimingConfig mTiming;      ///< Chipset timing (T0H, T1H, T0L, reset)
    PaddingGenerator mPaddingGenerator;     ///< Optional padding generator for block-size alignment
    fl::vector_psram<uint8_t> mEncodedData; ///< Encoded transmission bytes (PSRAM)
    volatile bool mInUse = false;           ///< Engine is transmitting this data (prevents creator updates)
};

}  // namespace fl
