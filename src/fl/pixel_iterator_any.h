/// @file fl/pixel_iterator_any.h
/// @brief Adapter class that creates a PixelIterator from any color order

#pragma once

#include "fl/chipsets/encoders/pixel_iterator.h"
#include "pixel_controller.h"
#include "fl/stl/variant.h"
#include "fl/stl/optional.h"
#include "rgbw.h"

namespace fl {

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
    PixelIteratorAny(PixelController<RGB> &controller, EOrder newOrder, Rgbw rgbw): mRgbw(rgbw) {
        init(controller, newOrder);
    }

    template<typename PIXEL_CONTROLLER>
    PixelIteratorAny(PIXEL_CONTROLLER &controller, EOrder newOrder, Rgbw rgbw): mRgbw(rgbw) {
        PixelController<RGB> rgbController(controller);  // Normslize to RGB order.
        init(controller, newOrder);  // now this switch statement just has to resolve the other half.
    }

    /// @brief Get the type-erased PixelIterator
    PixelIterator& get() { return *mPixelIterator; }

    /// @brief Implicit conversion to PixelIterator reference
    operator PixelIterator&() { return *mPixelIterator; }

    /// @brief Initialize the adapter with color order conversion
    void init(PixelController<RGB> &controller, EOrder newOrder) {
        // Step 1: Create the appropriate PixelController variant based on color order
        switch (newOrder) {
        case RGB:
            mAnyController = controller;
            break;
        case RBG:
            mAnyController = PixelController<RBG>(controller);
            break;
        case GRB:
            mAnyController = PixelController<GRB>(controller);
            break;
        case GBR:
            mAnyController = PixelController<GBR>(controller);
            break;
        case BRG:
            mAnyController = PixelController<BRG>(controller);
            break;
        case BGR:
            mAnyController = PixelController<BGR>(controller);
            break;
        }

        // Step 2: Use visitor pattern to construct PixelIterator with correct pointer type
        // Note: fl::Optional::emplace takes a constructed object, not constructor args
        struct PixelIteratorInitVisitor {
            PixelIteratorInitVisitor(Rgbw rgbw) : rgbw(rgbw) {}
            fl::Optional<PixelIterator>* pixelIteratorPtr;
            Rgbw rgbw;

            // Need concrete overloads for each type in the variant
            void accept(PixelController<RGB>& controller) {
                pixelIteratorPtr->emplace(PixelIterator(&controller, rgbw));
            }
            void accept(PixelController<RBG>& controller) {
                pixelIteratorPtr->emplace(PixelIterator(&controller, rgbw));
            }
            void accept(PixelController<GRB>& controller) {
                pixelIteratorPtr->emplace(PixelIterator(&controller, rgbw));
            }
            void accept(PixelController<GBR>& controller) {
                pixelIteratorPtr->emplace(PixelIterator(&controller, rgbw));
            }
            void accept(PixelController<BRG>& controller) {
                pixelIteratorPtr->emplace(PixelIterator(&controller, rgbw));
            }
            void accept(PixelController<BGR>& controller) {
                pixelIteratorPtr->emplace(PixelIterator(&controller, rgbw));
            }
        };

        PixelIteratorInitVisitor visitor(mRgbw);
        visitor.pixelIteratorPtr = &mPixelIterator;
        mAnyController.visit(visitor);
    }

  private:
    fl::variant<PixelController<RGB>, PixelController<RBG>,
                PixelController<GRB>, PixelController<GBR>,
                PixelController<BRG>, PixelController<BGR>>
        mAnyController;

    // fl::optional used just as a way to defer construction.
    fl::Optional<PixelIterator> mPixelIterator;

    Rgbw mRgbw;
};

}  // namespace fl
