/// @file channel.cpp
/// @brief LED channel implementation

#include "channel.h"
#include "channel_config.h"
#include "channel_manager.h"
#include "fl/atomic.h"
#include "fl/variant.h"
#include "pixel_controller.h"

namespace fl {


int32_t Channel::nextId() {
    static fl::atomic<int32_t> gNextChannelId(0);
    return gNextChannelId.fetch_add(1);
}

ChannelPtr Channel::create(const ChannelConfig &config,
                           IChannelEngine *engine) {
    return fl::make_shared<Channel>(config, engine);
}

Channel::Channel(const ChannelConfig &config, IChannelEngine *engine)
    : mConfig(config), mEngine(engine), mManager(NULL), mId(nextId()) {}

Channel::~Channel() {}

Channel &Channel::setCorrection(CRGB correction) {
    CLEDController::setCorrection(correction);
    return *this;
}

Channel &Channel::setTemperature(CRGB temperature) {
    CLEDController::setTemperature(temperature);
    return *this;
}

Channel &Channel::setDither(fl::u8 ditherMode) {
    CLEDController::setDither(ditherMode);
    return *this;
}

Channel &Channel::setRgbw(const Rgbw &rgbw) {
    CLEDController::setRgbw(rgbw);
    return *this;
}

void Channel::dispose() {
    // Remove this channel from the CLEDController linked list
    CLEDController::removeFromList(this);
}


// Adapter class for showPixels().
class PixelIteratorForAnyColorOrder {
  public:
    PixelIteratorForAnyColorOrder(PixelController<RGB> &controller, EOrder newOrder, Rgbw rgbw): mRgbw(rgbw) {
        init(controller, newOrder);
    }

    PixelIterator& get() { return *mPixelIterator; }

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

            // Need concrete overloads for each type in the Variant
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
        visitor.rgbw = RgbwDefault::value();
        mAnyController.visit(visitor);
    }

    fl::Variant<PixelController<RGB>, PixelController<RBG>,
                PixelController<GRB>, PixelController<GBR>,
                PixelController<BRG>, PixelController<BGR>>
        mAnyController;

    // fl::optional used just as a way to defer constructions.
    fl::Optional<PixelIterator> mPixelIterator;

    Rgbw mRgbw;
};


/* the problem here is that the CLED controller sub class represents two things

  * read only link to the frame buffer
  * Encoder: RGB8 -> uint8_t[]
  * sw/hw bit banging

Now we are running into the real trifecta of the challenges fastled always had to face
the problem of naming things.

It's hard to name things in classic FastLED speak because what something did was fuzzy.

It's easier to name things now because we are  seperating them into their functional components.
  * Channel
  * Encoder
  * Controller

  */

void Channel::showPixels(PixelController<RGB, 1, 0xFFFFFFFF> &pixels) {
    // BIG TODO: CHANNEL NEEDS AN ENCODER:
    // Convert pixels to channel data using the configured color order and RGBW settings
    PixelIteratorForAnyColorOrder tmp(pixels, mConfig.rgb_order, mConfig.rgbw);
    PixelIterator& pixelIterator = tmp.get();
    // FUTURE WORK: This is where we put the encoder
    pixelIterator.writeWS2812(mConfig.rgbw, &mChannelData);
    
}

void Channel::init() {
    // TODO: Implement initialization
}

} // namespace fl
