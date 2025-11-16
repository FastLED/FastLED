/// @file channel_data.h
/// @brief Channel transmission data - lightweight DTO for engine transmission

#pragma once

#include "ftl/vector.h"
#include "ftl/stdint.h"
#include "ftl/shared_ptr.h"
#include "ftl/function.h"
#include "channel_config.h"

namespace fl {

class ChannelData;
FASTLED_SHARED_PTR(ChannelData);

/// @brief Padding generator function type
///
/// Called by the engine to extend the encoded data buffer to an exact target size.
/// The function should modify the buffer in-place to reach the target size
/// (e.g., inserting zero bytes after a preamble for block alignment).
///
/// @param buffer Reference to the encoded data buffer to modify
/// @param targetSize Exact size the buffer must reach (always >= current buffer size)
using PaddingGenerator = fl::function<void(fl::vector_psram<uint8_t>&, size_t targetSize)>;

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
    /// @param generator Function that extends buffer to exact target size (nullptr to disable)
    void setPaddingGenerator(PaddingGenerator generator) {
        mPaddingGenerator = fl::move(generator);
    }

    /// @brief Apply padding to the encoded data buffer to reach target size
    ///
    /// If a padding generator is configured, it will be called to extend
    /// the buffer to the exact target size. Otherwise, this is a no-op.
    ///
    /// @param targetSize Exact size the buffer must reach (must be >= current size)
    void applyPadding(size_t targetSize) {
        if (mPaddingGenerator) {
            mPaddingGenerator(mEncodedData, targetSize);
        }
    }

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
    fl::vector_psram<uint8_t> mEncodedData; ///< Encoded transmission bytes (PSRAM)
    volatile bool mInUse = false;           ///< Engine is transmitting this data (prevents creator updates)
    PaddingGenerator mPaddingGenerator;     ///< Optional padding generator for block-size alignment
};

}  // namespace fl
