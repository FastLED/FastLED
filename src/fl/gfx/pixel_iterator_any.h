/// @file fl/pixel_iterator_any.h
/// @brief Adapter class that creates a PixelIterator from any color order

#pragma once

#include "fl/chipsets/encoders/pixel_iterator.h"
#include "pixel_controller.h"
#include "fl/stl/optional.h"
#include "fl/stl/vector.h"
#include "rgbw.h"
#include "fl/gfx/rgbww.h"

namespace fl {

// Forward declarations
class XYMap;



/// @brief Adapter class that creates a PixelIterator from any color order
///
/// This class takes a PixelController<RGB> and converts it to the requested
/// color order, creating a type-erased PixelIterator for non-templated code.
class PixelIteratorAny {
  public:
    /// @brief Construct adapter with color order conversion
    /// @param controller Source PixelController (always RGB order)
    /// @param newOrder Desired color order (RGB, RBG, GRB, GBR, BRG, BGR)
    /// @param rgbw RGBW conversion settings
    PixelIteratorAny(PixelController<RGB> &controller, EOrder newOrder, Rgbw rgbw,
                     Rgbww rgbww = RgbwwInvalid::value())
        : mController(controller), mOrder(newOrder), mRgbw(rgbw), mRgbww(rgbww) {
        bindIterator();
    }

    template<typename PIXEL_CONTROLLER>
    PixelIteratorAny(PIXEL_CONTROLLER &controller, EOrder newOrder, Rgbw rgbw,
                     Rgbww rgbww = RgbwwInvalid::value())
        : mController(controller),  // Normalize to RGB order (#2558).
          mOrder(newOrder), mRgbw(rgbw), mRgbww(rgbww) {
        bindIterator();
    }

    /// @brief Get the type-erased PixelIterator
    PixelIterator& get() { return *mPixelIterator; }
    const PixelIterator& get() const { return *mPixelIterator; }

    /// @brief Implicit conversion to PixelIterator reference
    operator PixelIterator&() { return *mPixelIterator; }

    /// @brief Set XYMap for pixel addressing
    /// @param xymap XYMap with embedded width/height (nullptr to disable)
    void setXYMap(const fl::shared_ptr<const XYMap>& xymap) {
        mXyMap = xymap;
    }

    /// @brief Copy: the iterator must be re-aimed, not copied.
    ///
    /// This class is self-referential -- `mPixelIterator` is a type-erased
    /// `PixelIterator` holding a `void*` into `mController`, a member of the
    /// same object. The compiler-generated copy carried that pointer across
    /// unchanged, so the copy's iterator kept reading the *source* object's
    /// controller. Where the source was a temporary, as in
    /// `ReorderingPixelIteratorAny`'s addressing branch, that was a read of
    /// dead stack on every addressed frame (#4201).
    PixelIteratorAny(const PixelIteratorAny& other) FL_NO_EXCEPT
        : mController(other.mController), mOrder(other.mOrder),
          mRgbw(other.mRgbw), mRgbww(other.mRgbww), mXyMap(other.mXyMap) {
        bindIterator();
    }

    PixelIteratorAny(PixelIteratorAny&& other) FL_NO_EXCEPT
        : mController(other.mController), mOrder(other.mOrder),
          mRgbw(other.mRgbw), mRgbww(other.mRgbww), mXyMap(fl::move(other.mXyMap)) {
        bindIterator();
    }

    PixelIteratorAny& operator=(const PixelIteratorAny& other) FL_NO_EXCEPT {
        if (this != &other) {
            mController = other.mController;
            mOrder = other.mOrder;
            mRgbw = other.mRgbw;
            mRgbww = other.mRgbww;
            mXyMap = other.mXyMap;
            bindIterator();
        }
        return *this;
    }

    PixelIteratorAny& operator=(PixelIteratorAny&& other) FL_NO_EXCEPT {
        if (this != &other) {
            mController = other.mController;
            mOrder = other.mOrder;
            mRgbw = other.mRgbw;
            mRgbww = other.mRgbww;
            mXyMap = fl::move(other.mXyMap);
            bindIterator();
        }
        return *this;
    }

  private:
    /// @brief Point `mPixelIterator` at this object's own controller.
    ///
    /// Every path that changes `mController` has to end here, because the
    /// iterator holds a raw pointer into it and nothing else fixes that up.
    ///
    /// One `PixelController<RGB>` for every order. This used to hold a
    /// `variant` of six `PixelController<EOrder>` instantiations and visit it
    /// -- six copies of a ~870 B vtable set plus a 127 B visit thunk each, of
    /// which a sketch uses one, measured at ~5.9 KB on an esp32dev Blink
    /// build. A colour order is a pure permutation of the three per-channel
    /// values, so the iterator applies it and the other five instantiations
    /// are never referenced (FastLED#4402).
    void bindIterator() {
        mPixelIterator.emplace(PixelIterator(&mController, mRgbw, mRgbww));
        mPixelIterator->setColorOrder(mOrder);
    }

    PixelController<RGB> mController;
    EOrder mOrder;

    // fl::optional used just as a way to defer construction.
    fl::Optional<PixelIterator> mPixelIterator;

    Rgbw mRgbw;
    Rgbww mRgbww;

    // XYMap for pixel addressing (optional) - contains embedded width/height
    fl::shared_ptr<const XYMap> mXyMap;
};

}  // namespace fl
