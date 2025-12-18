#pragma once

/// @file fl/chipsets/encoders/pixel_iterator_adapters.h
/// @brief Adapter layer bridging PixelIterator to encoder input iterators
///
/// This file provides adapters that convert PixelIterator (which handles
/// scaling, gamma, dithering) into input iterators yielding raw CRGB/Rgbw
/// pixels that can be consumed by the new encoder functions.
///
/// Key concepts:
/// - PixelIterator: Stateful iterator with scaling/gamma/dithering
/// - ScaledPixelIterator: STL-compliant input iterator wrapping PixelIterator
/// - Allows new encoders to work with both raw pixel arrays and PixelIterator

#include "fl/stl/stdint.h"
#include "fl/stl/array.h"
#include "fl/stl/pair.h"
#include "crgb.h"
#include "fl/rgbw.h"

namespace fl {

// Iterator tag for input iterators (STL compatibility)
struct input_iterator_tag {};

class PixelIterator;  // Forward declaration

namespace detail {

/// @brief Input iterator adapter for PixelIterator yielding 3-byte pixel data
///
/// Wraps a PixelIterator and presents an STL-compliant input iterator
/// interface that yields 3-byte pixel arrays with scaling/gamma/dithering applied.
///
/// @note Yields fl::array<u8, 3> representing bytes in wire order (color order already applied)
/// @note This is an input iterator (single-pass, read-only)
/// @note Each dereference consumes one pixel from the underlying PixelIterator
class ScaledPixelIteratorRGB {
public:
    // Iterator traits
    using iterator_category = input_iterator_tag;
    using value_type = array<u8, 3>;
    using difference_type = ptrdiff_t;
    using pointer = const array<u8, 3>*;
    using reference = const array<u8, 3>&;

    /// @brief Construct from PixelIterator
    /// @param pixels Pointer to PixelIterator (must outlive this adapter)
    explicit ScaledPixelIteratorRGB(PixelIterator* pixels)
        : mPixels(pixels), mCurrent(), mHasValue(false) {
        advance();  // Preload first pixel
    }

    /// @brief Sentinel constructor (end iterator)
    ScaledPixelIteratorRGB()
        : mPixels(nullptr), mCurrent(), mHasValue(false) {}

    /// @brief Dereference operator
    /// @return Current pixel value (3 bytes in wire order)
    const array<u8, 3>& operator*() const {
        return mCurrent;
    }

    /// @brief Arrow operator
    /// @return Pointer to current pixel
    const array<u8, 3>* operator->() const {
        return &mCurrent;
    }

    /// @brief Pre-increment operator
    /// @return Reference to this iterator after advancing
    ScaledPixelIteratorRGB& operator++() {
        advance();
        return *this;
    }

    /// @brief Post-increment operator
    /// @return Copy of iterator before advancing
    ScaledPixelIteratorRGB operator++(int) {
        ScaledPixelIteratorRGB tmp = *this;
        advance();
        return tmp;
    }

    /// @brief Equality comparison
    /// @param other Iterator to compare with
    /// @return true if both iterators are at the end or both valid
    bool operator==(const ScaledPixelIteratorRGB& other) const {
        // Two end iterators are equal
        if (!mHasValue && !other.mHasValue) {
            return true;
        }
        // End iterator vs valid iterator
        if (mHasValue != other.mHasValue) {
            return false;
        }
        // Two valid iterators - compare underlying pixel iterators
        return mPixels == other.mPixels;
    }

    /// @brief Inequality comparison
    /// @param other Iterator to compare with
    /// @return true if iterators are not equal
    bool operator!=(const ScaledPixelIteratorRGB& other) const {
        return !(*this == other);
    }

private:
    /// @brief Advance to next pixel (or mark as end)
    void advance();

    PixelIterator* mPixels;      ///< Underlying PixelIterator
    array<u8, 3> mCurrent;       ///< Current pixel value (cached, wire order)
    bool mHasValue;              ///< true if current pixel is valid
};

/// @brief Input iterator adapter for PixelIterator yielding 4-byte pixel data
///
/// Similar to ScaledPixelIteratorRGB but yields 4-byte pixel arrays (RGBW).
///
/// @note Yields fl::array<u8, 4> representing bytes in wire order (color order already applied)
class ScaledPixelIteratorRGBW {
public:
    // Iterator traits
    using iterator_category = input_iterator_tag;
    using value_type = array<u8, 4>;
    using difference_type = ptrdiff_t;
    using pointer = const array<u8, 4>*;
    using reference = const array<u8, 4>&;

    /// @brief Construct from PixelIterator
    /// @param pixels Pointer to PixelIterator (must outlive this adapter)
    explicit ScaledPixelIteratorRGBW(PixelIterator* pixels)
        : mPixels(pixels), mCurrent(), mHasValue(false) {
        advance();  // Preload first pixel
    }

    /// @brief Sentinel constructor (end iterator)
    ScaledPixelIteratorRGBW()
        : mPixels(nullptr), mCurrent(), mHasValue(false) {}

    /// @brief Dereference operator
    /// @return Current pixel value (4 bytes in wire order)
    const array<u8, 4>& operator*() const {
        return mCurrent;
    }

    /// @brief Arrow operator
    /// @return Pointer to current pixel
    const array<u8, 4>* operator->() const {
        return &mCurrent;
    }

    /// @brief Pre-increment operator
    ScaledPixelIteratorRGBW& operator++() {
        advance();
        return *this;
    }

    /// @brief Post-increment operator
    ScaledPixelIteratorRGBW operator++(int) {
        ScaledPixelIteratorRGBW tmp = *this;
        advance();
        return tmp;
    }

    /// @brief Equality comparison
    bool operator==(const ScaledPixelIteratorRGBW& other) const {
        if (!mHasValue && !other.mHasValue) {
            return true;
        }
        if (mHasValue != other.mHasValue) {
            return false;
        }
        return mPixels == other.mPixels;
    }

    /// @brief Inequality comparison
    bool operator!=(const ScaledPixelIteratorRGBW& other) const {
        return !(*this == other);
    }

private:
    /// @brief Advance to next pixel (or mark as end)
    void advance();

    PixelIterator* mPixels;      ///< Underlying PixelIterator
    array<u8, 4> mCurrent;       ///< Current pixel value (cached, wire order)
    bool mHasValue;              ///< true if current pixel is valid
};

/// @brief Input iterator adapter for PixelIterator yielding brightness values
///
/// Extracts per-LED brightness values from PixelIterator for HD encoders.
class ScaledPixelIteratorBrightness {
public:
    // Iterator traits
    using iterator_category = input_iterator_tag;
    using value_type = u8;
    using difference_type = ptrdiff_t;
    using pointer = const u8*;
    using reference = u8;

    /// @brief Construct from PixelIterator
    /// @param pixels Pointer to PixelIterator (must outlive this adapter)
    explicit ScaledPixelIteratorBrightness(PixelIterator* pixels)
        : mPixels(pixels), mCurrent(0), mHasValue(false) {
        advance();  // Preload first brightness
    }

    /// @brief Sentinel constructor (end iterator)
    ScaledPixelIteratorBrightness()
        : mPixels(nullptr), mCurrent(0), mHasValue(false) {}

    /// @brief Dereference operator
    u8 operator*() const {
        return mCurrent;
    }

    /// @brief Pre-increment operator
    ScaledPixelIteratorBrightness& operator++() {
        advance();
        return *this;
    }

    /// @brief Post-increment operator
    ScaledPixelIteratorBrightness operator++(int) {
        ScaledPixelIteratorBrightness tmp = *this;
        advance();
        return tmp;
    }

    /// @brief Equality comparison
    bool operator==(const ScaledPixelIteratorBrightness& other) const {
        if (!mHasValue && !other.mHasValue) {
            return true;
        }
        if (mHasValue != other.mHasValue) {
            return false;
        }
        return mPixels == other.mPixels;
    }

    /// @brief Inequality comparison
    bool operator!=(const ScaledPixelIteratorBrightness& other) const {
        return !(*this == other);
    }

private:
    /// @brief Advance to next brightness value (or mark as end)
    void advance();

    PixelIterator* mPixels;  ///< Underlying PixelIterator
    u8 mCurrent;             ///< Current brightness value (cached)
    bool mHasValue;          ///< true if current value is valid
};

/// @brief Input iterator adapter for PixelIterator yielding 16-bit RGB pixel data
///
/// Similar to ScaledPixelIteratorRGB but yields 16-bit channel values for HD chipsets.
/// Handles 8→16 bit mapping, color correction, and brightness scaling.
///
/// @note Yields fl::array<u16, 3> representing 16-bit channels in wire order (color order already applied)
/// @note Handles FASTLED_HD_COLOR_MIXING conditional internally
class ScaledPixelIteratorRGB16 {
public:
    // Iterator traits
    using iterator_category = input_iterator_tag;
    using value_type = array<u16, 3>;
    using difference_type = ptrdiff_t;
    using pointer = const array<u16, 3>*;
    using reference = const array<u16, 3>&;

    /// @brief Construct from PixelIterator
    /// @param pixels Pointer to PixelIterator (must outlive this adapter)
    explicit ScaledPixelIteratorRGB16(PixelIterator* pixels)
        : mPixels(pixels), mCurrent(), mHasValue(false) {
        advance();  // Preload first pixel
    }

    /// @brief Sentinel constructor (end iterator)
    ScaledPixelIteratorRGB16()
        : mPixels(nullptr), mCurrent(), mHasValue(false) {}

    /// @brief Dereference operator
    /// @return Current pixel value (3x 16-bit channels in wire order)
    const array<u16, 3>& operator*() const {
        return mCurrent;
    }

    /// @brief Arrow operator
    /// @return Pointer to current pixel
    const array<u16, 3>* operator->() const {
        return &mCurrent;
    }

    /// @brief Pre-increment operator
    /// @return Reference to this iterator after advancing
    ScaledPixelIteratorRGB16& operator++() {
        advance();
        return *this;
    }

    /// @brief Post-increment operator
    /// @return Copy of iterator before advancing
    ScaledPixelIteratorRGB16 operator++(int) {
        ScaledPixelIteratorRGB16 tmp = *this;
        advance();
        return tmp;
    }

    /// @brief Equality comparison
    /// @param other Iterator to compare with
    /// @return true if both iterators are at the end or both valid
    bool operator==(const ScaledPixelIteratorRGB16& other) const {
        // Two end iterators are equal
        if (!mHasValue && !other.mHasValue) {
            return true;
        }
        // End iterator vs valid iterator
        if (mHasValue != other.mHasValue) {
            return false;
        }
        // Two valid iterators - compare underlying pixel iterators
        return mPixels == other.mPixels;
    }

    /// @brief Inequality comparison
    /// @param other Iterator to compare with
    /// @return true if iterators are not equal
    bool operator!=(const ScaledPixelIteratorRGB16& other) const {
        return !(*this == other);
    }

private:
    /// @brief Advance to next pixel (or mark as end)
    void advance();

    PixelIterator* mPixels;      ///< Underlying PixelIterator
    array<u16, 3> mCurrent;      ///< Current pixel value (cached, wire order, 16-bit)
    bool mHasValue;              ///< true if current pixel is valid
};

} // namespace detail

/// @brief Create RGB input iterator range from PixelIterator
/// @param pixels PixelIterator to wrap
/// @return Pair of begin/end iterators
inline pair<detail::ScaledPixelIteratorRGB, detail::ScaledPixelIteratorRGB>
makeScaledPixelRangeRGB(PixelIterator* pixels) {
    return make_pair(
        detail::ScaledPixelIteratorRGB(pixels),
        detail::ScaledPixelIteratorRGB()  // End sentinel
    );
}

/// @brief Create RGBW input iterator range from PixelIterator
/// @param pixels PixelIterator to wrap
/// @return Pair of begin/end iterators
inline pair<detail::ScaledPixelIteratorRGBW, detail::ScaledPixelIteratorRGBW>
makeScaledPixelRangeRGBW(PixelIterator* pixels) {
    return make_pair(
        detail::ScaledPixelIteratorRGBW(pixels),
        detail::ScaledPixelIteratorRGBW()  // End sentinel
    );
}

/// @brief Create brightness input iterator range from PixelIterator
/// @param pixels PixelIterator to wrap
/// @return Pair of begin/end iterators
inline pair<detail::ScaledPixelIteratorBrightness, detail::ScaledPixelIteratorBrightness>
makeScaledBrightnessRange(PixelIterator* pixels) {
    return make_pair(
        detail::ScaledPixelIteratorBrightness(pixels),
        detail::ScaledPixelIteratorBrightness()  // End sentinel
    );
}

/// @brief Create 16-bit RGB input iterator range from PixelIterator
/// @param pixels PixelIterator to wrap
/// @return Pair of begin/end iterators
inline pair<detail::ScaledPixelIteratorRGB16, detail::ScaledPixelIteratorRGB16>
makeScaledPixelRangeRGB16(PixelIterator* pixels) {
    return make_pair(
        detail::ScaledPixelIteratorRGB16(pixels),
        detail::ScaledPixelIteratorRGB16()  // End sentinel
    );
}

// NOTE: For APA102 HD mode, the chipset-specific gamma correction (five_bit_hd_gamma_bitshift)
// needs to be applied. Since this is chipset-specific and not a general iterator adapter concern,
// the APA102 controller applies it inline in the showPixelsGammaBitShift() method.
// The controller uses getRawPixelData() to get raw RGB, applies gamma with loadRGBScaleAndBrightness(),
// then uses loadAndScaleRGB() to get wire-ordered output.

} // namespace fl

// ===========================================================================
// Implementation of adapter advance() methods
// ===========================================================================
// NOTE: These implementations are defined in pixel_iterator.h after the
//       PixelIterator class is fully defined, as they require complete type.
