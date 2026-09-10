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
#include "fl/stl/iterator.h"
#include "fl/stl/noexcept.h"
#include "crgb.h"  // FASTLED_HD_COLOR_MIXING

namespace fl {

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
    explicit ScaledPixelIteratorRGB(PixelIterator* pixels) FL_NO_EXCEPT
        : mPixels(pixels), mCurrent(), mHasValue(false) {
        advance();  // Preload first pixel
    }

    /// @brief Sentinel constructor (end iterator)
    ScaledPixelIteratorRGB() FL_NO_EXCEPT
        : mPixels(nullptr), mCurrent(), mHasValue(false) {}

    /// @brief Dereference operator
    /// @return Current pixel value (3 bytes in wire order)
    const array<u8, 3>& operator*() const FL_NO_EXCEPT {
        return mCurrent;
    }

    /// @brief Arrow operator
    /// @return Pointer to current pixel
    const array<u8, 3>* operator->() const FL_NO_EXCEPT {
        return &mCurrent;
    }

    /// @brief Pre-increment operator
    /// @return Reference to this iterator after advancing
    ScaledPixelIteratorRGB& operator++() FL_NO_EXCEPT {
        advance();
        return *this;
    }

    /// @brief Post-increment operator
    /// @return Copy of iterator before advancing
    ScaledPixelIteratorRGB operator++(int) FL_NO_EXCEPT {
        ScaledPixelIteratorRGB tmp = *this;
        advance();
        return tmp;
    }

    /// @brief Equality comparison
    /// @param other Iterator to compare with
    /// @return true if both iterators are at the end or both valid
    bool operator==(const ScaledPixelIteratorRGB& other) const FL_NO_EXCEPT {
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
    bool operator!=(const ScaledPixelIteratorRGB& other) const FL_NO_EXCEPT {
        return !(*this == other);
    }

private:
    /// @brief Advance to next pixel (or mark as end)
    void advance() FL_NO_EXCEPT;

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
    explicit ScaledPixelIteratorRGBW(PixelIterator* pixels) FL_NO_EXCEPT
        : mPixels(pixels), mCurrent(), mHasValue(false) {
        advance();  // Preload first pixel
    }

    /// @brief Sentinel constructor (end iterator)
    ScaledPixelIteratorRGBW() FL_NO_EXCEPT
        : mPixels(nullptr), mCurrent(), mHasValue(false) {}

    /// @brief Dereference operator
    /// @return Current pixel value (4 bytes in wire order)
    const array<u8, 4>& operator*() const FL_NO_EXCEPT {
        return mCurrent;
    }

    /// @brief Arrow operator
    /// @return Pointer to current pixel
    const array<u8, 4>* operator->() const FL_NO_EXCEPT {
        return &mCurrent;
    }

    /// @brief Pre-increment operator
    ScaledPixelIteratorRGBW& operator++() FL_NO_EXCEPT {
        advance();
        return *this;
    }

    /// @brief Post-increment operator
    ScaledPixelIteratorRGBW operator++(int) FL_NO_EXCEPT {
        ScaledPixelIteratorRGBW tmp = *this;
        advance();
        return tmp;
    }

    /// @brief Equality comparison
    bool operator==(const ScaledPixelIteratorRGBW& other) const FL_NO_EXCEPT {
        if (!mHasValue && !other.mHasValue) {
            return true;
        }
        if (mHasValue != other.mHasValue) {
            return false;
        }
        return mPixels == other.mPixels;
    }

    /// @brief Inequality comparison
    bool operator!=(const ScaledPixelIteratorRGBW& other) const FL_NO_EXCEPT {
        return !(*this == other);
    }

private:
    /// @brief Advance to next pixel (or mark as end)
    void advance() FL_NO_EXCEPT;

    PixelIterator* mPixels;      ///< Underlying PixelIterator
    array<u8, 4> mCurrent;       ///< Current pixel value (cached, wire order)
    bool mHasValue;              ///< true if current pixel is valid
};

/// @brief Input iterator adapter yielding 5-byte RGBWW pixels (issue #2558).
///
/// Mirrors ScaledPixelIteratorRGBW but produces 5 bytes per pixel
/// (R, G, B, warm-W, cool-W in EOrder + EOrderWW wire order).
class ScaledPixelIteratorRGBWW {
public:
    using iterator_category = input_iterator_tag;
    using value_type = array<u8, 5>;
    using difference_type = ptrdiff_t;
    using pointer = const array<u8, 5>*;
    using reference = const array<u8, 5>&;

    explicit ScaledPixelIteratorRGBWW(PixelIterator* pixels) FL_NO_EXCEPT
        : mPixels(pixels), mCurrent(), mHasValue(false) {
        advance();
    }

    ScaledPixelIteratorRGBWW() FL_NO_EXCEPT
        : mPixels(nullptr), mCurrent(), mHasValue(false) {}

    const array<u8, 5>& operator*() const FL_NO_EXCEPT { return mCurrent; }
    const array<u8, 5>* operator->() const FL_NO_EXCEPT { return &mCurrent; }

    ScaledPixelIteratorRGBWW& operator++() FL_NO_EXCEPT { advance(); return *this; }
    ScaledPixelIteratorRGBWW operator++(int) FL_NO_EXCEPT {
        ScaledPixelIteratorRGBWW tmp = *this;
        advance();
        return tmp;
    }

    bool operator==(const ScaledPixelIteratorRGBWW& other) const FL_NO_EXCEPT {
        if (!mHasValue && !other.mHasValue) return true;
        if (mHasValue != other.mHasValue) return false;
        return mPixels == other.mPixels;
    }

    bool operator!=(const ScaledPixelIteratorRGBWW& other) const FL_NO_EXCEPT {
        return !(*this == other);
    }

private:
    void advance() FL_NO_EXCEPT;

    PixelIterator* mPixels;
    array<u8, 5> mCurrent;
    bool mHasValue;
};

#if FASTLED_HD_COLOR_MIXING
/// @brief Input iterator adapter for PixelIterator yielding brightness values
///
/// Yields the strip's brightness for the HD encoders, which pair it with a
/// `ScaledPixelIteratorRGB` over the *same* `PixelIterator`.
///
/// It must never move that shared cursor. It used to: `advance()` ended with
/// `stepDithering()` + `advanceData()` exactly as the RGB adapter's does, so
/// with both stepped in lockstep by the encoder two source pixels were eaten
/// per emitted LED and the strip came out half length, showing every other
/// pixel (#4321). Under `FASTLED_HD_COLOR_MIXING` the value is a per-strip
/// constant -- `ColorAdjustment::brightness` -- so there is nothing to walk
/// for in the first place.
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
    explicit ScaledPixelIteratorBrightness(PixelIterator* pixels) FL_NO_EXCEPT
        : mPixels(pixels), mCurrent(0) {
        load();
    }

    /// @brief Dereference operator
    u8 operator*() const FL_NO_EXCEPT {
        return mCurrent;
    }

    /// @brief Pre-increment operator
    ScaledPixelIteratorBrightness& operator++() FL_NO_EXCEPT {
        load();
        return *this;
    }

    /// @brief Post-increment operator
    ScaledPixelIteratorBrightness operator++(int) FL_NO_EXCEPT {
        ScaledPixelIteratorBrightness tmp = *this;
        load();
        return tmp;
    }

    // No sentinel constructor and no comparisons, on purpose. This iterator
    // advances nothing, so it can never reach an end -- any loop written
    // against a sentinel here would spin forever. The encoders stop on the
    // RGB range they walk alongside, which is the only real termination.

private:
    /// @brief Re-read the strip's brightness. Moves no cursor.
    void load() FL_NO_EXCEPT;

    PixelIterator* mPixels;  ///< Underlying PixelIterator
    u8 mCurrent;             ///< Current brightness value (cached)
};
#endif  // FASTLED_HD_COLOR_MIXING

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
    explicit ScaledPixelIteratorRGB16(PixelIterator* pixels) FL_NO_EXCEPT
        : mPixels(pixels), mCurrent(), mHasValue(false) {
        advance();  // Preload first pixel
    }

    /// @brief Sentinel constructor (end iterator)
    ScaledPixelIteratorRGB16() FL_NO_EXCEPT
        : mPixels(nullptr), mCurrent(), mHasValue(false) {}

    /// @brief Dereference operator
    /// @return Current pixel value (3x 16-bit channels in wire order)
    const array<u16, 3>& operator*() const FL_NO_EXCEPT {
        return mCurrent;
    }

    /// @brief Arrow operator
    /// @return Pointer to current pixel
    const array<u16, 3>* operator->() const FL_NO_EXCEPT {
        return &mCurrent;
    }

    /// @brief Pre-increment operator
    /// @return Reference to this iterator after advancing
    ScaledPixelIteratorRGB16& operator++() FL_NO_EXCEPT {
        advance();
        return *this;
    }

    /// @brief Post-increment operator
    /// @return Copy of iterator before advancing
    ScaledPixelIteratorRGB16 operator++(int) FL_NO_EXCEPT {
        ScaledPixelIteratorRGB16 tmp = *this;
        advance();
        return tmp;
    }

    /// @brief Equality comparison
    /// @param other Iterator to compare with
    /// @return true if both iterators are at the end or both valid
    bool operator==(const ScaledPixelIteratorRGB16& other) const FL_NO_EXCEPT {
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
    bool operator!=(const ScaledPixelIteratorRGB16& other) const FL_NO_EXCEPT {
        return !(*this == other);
    }

private:
    /// @brief Advance to next pixel (or mark as end)
    void advance() FL_NO_EXCEPT;

    PixelIterator* mPixels;      ///< Underlying PixelIterator
    array<u16, 3> mCurrent;      ///< Current pixel value (cached, wire order, 16-bit)
    bool mHasValue;              ///< true if current pixel is valid
};

} // namespace detail

/// @brief Create RGB input iterator range from PixelIterator
/// @param pixels PixelIterator to wrap
/// @return Pair of begin/end iterators
inline pair<detail::ScaledPixelIteratorRGB, detail::ScaledPixelIteratorRGB>
makeScaledPixelRangeRGB(PixelIterator* pixels) FL_NO_EXCEPT {
    return make_pair(
        detail::ScaledPixelIteratorRGB(pixels),
        detail::ScaledPixelIteratorRGB()  // End sentinel
    );
}

/// @brief Create RGBW input iterator range from PixelIterator
/// @param pixels PixelIterator to wrap
/// @return Pair of begin/end iterators
inline pair<detail::ScaledPixelIteratorRGBW, detail::ScaledPixelIteratorRGBW>
makeScaledPixelRangeRGBW(PixelIterator* pixels) FL_NO_EXCEPT {
    return make_pair(
        detail::ScaledPixelIteratorRGBW(pixels),
        detail::ScaledPixelIteratorRGBW()  // End sentinel
    );
}

/// @brief Create RGBWW input iterator range from PixelIterator (issue #2558)
inline pair<detail::ScaledPixelIteratorRGBWW, detail::ScaledPixelIteratorRGBWW>
makeScaledPixelRangeRGBWW(PixelIterator* pixels) FL_NO_EXCEPT {
    return make_pair(
        detail::ScaledPixelIteratorRGBWW(pixels),
        detail::ScaledPixelIteratorRGBWW()  // End sentinel
    );
}

#if FASTLED_HD_COLOR_MIXING
/// @brief Create the brightness input iterator for the HD encoders
/// @param pixels PixelIterator to wrap
/// @return A single iterator, deliberately not a range
///
/// This used to return a begin/end pair, and it should not have. The iterator
/// yields a per-strip constant and must not move the shared `PixelIterator`
/// cursor -- the `ScaledPixelIteratorRGB` it is paired with owns that (#4321).
/// An iterator that advances nothing can never reach an end sentinel, so a
/// standalone `for (it = r.first; it != r.second; ++it)` over the old pair
/// could not terminate whatever the sentinel checked. Making `mHasValue`
/// track `has(1)` would not have fixed it either: with nothing advancing the
/// cursor, `has(1)` never becomes false on its own.
///
/// So there is no end to hand out. The HD encoders take this iterator
/// alongside the RGB range and stop on the RGB range, which is the only
/// termination that was ever real. All callers used `.first`.
inline detail::ScaledPixelIteratorBrightness
makeScaledBrightness(PixelIterator* pixels) FL_NO_EXCEPT {
    return detail::ScaledPixelIteratorBrightness(pixels);
}
#endif  // FASTLED_HD_COLOR_MIXING

/// @brief Create 16-bit RGB input iterator range from PixelIterator
/// @param pixels PixelIterator to wrap
/// @return Pair of begin/end iterators
inline pair<detail::ScaledPixelIteratorRGB16, detail::ScaledPixelIteratorRGB16>
makeScaledPixelRangeRGB16(PixelIterator* pixels) FL_NO_EXCEPT {
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
