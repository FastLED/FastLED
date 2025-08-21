#pragma once

/// @file pixel_controller.h
/// Low level pixel data writing class

// Note that new code should use the PixelIterator concrete object to write out
// led data.
// Using this class deep in driver code is deprecated because it's templates will
// infact everything it touches. PixelIterator is concrete and doesn't have these
// problems. See PixelController::as_iterator() for how to create a PixelIterator.


#include "lib8tion/intmap.h"

#include "rgbw.h"
#include "fl/five_bit_hd_gamma.h"
#include "fl/force_inline.h"
#include "lib8tion/scale8.h"
#include "fl/namespace.h"
#include "eorder.h"
#include "dither_mode.h"
#include "pixel_iterator.h"
#include "crgb.h"
#include "fl/compiler_control.h"


#include "FastLED.h"  // Problematic.


FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_SIGN_CONVERSION
FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION
FL_DISABLE_WARNING_FLOAT_CONVERSION


FASTLED_NAMESPACE_BEGIN


/// Gets the assigned color channel for a byte's position in the output,
/// using the color order (EOrder) template parameter from the
/// LED controller
/// @param X the byte's position in the output (0-2)
/// @returns the color channel for that byte (0 = red, 1 = green, 2 = blue)
/// @see EOrder
#define RO(X) RGB_BYTE(RGB_ORDER, X)

/// Gets the assigned color channel for a byte's position in the output,
/// using a passed RGB color order
/// @param RO the RGB color order
/// @param X the byte's position in the output (0-2)
/// @returns the color channel for that byte (0 = red, 1 = green, 2 = blue)
/// @see EOrder
#define RGB_BYTE(RO,X) (((RO)>>(3*(2-(X)))) & 0x3)

/// Gets the color channel for byte 0.
/// @see RGB_BYTE(RO,X)
#define RGB_BYTE0(RO) ((RO>>6) & 0x3)
/// Gets the color channel for byte 1.
/// @see RGB_BYTE(RO,X)
#define RGB_BYTE1(RO) ((RO>>3) & 0x3)
/// Gets the color channel for byte 2.
/// @see RGB_BYTE(RO,X)
#define RGB_BYTE2(RO) ((RO) & 0x3)

// operator byte *(struct CRGB[] arr) { return (byte*)arr; }

struct ColorAdjustment {
    CRGB premixed;       /// the per-channel scale values premixed with brightness.
    #if FASTLED_HD_COLOR_MIXING
    CRGB color;          /// the per-channel scale values assuming full brightness.
    uint8_t brightness;  /// the global brightness value
    #endif
};


/// Pixel controller class.  This is the class that we use to centralize pixel access in a block of data, including
/// support for things like RGB reordering, scaling, dithering, skipping (for ARGB data), and eventually, we will
/// centralize 8/12/16 conversions here as well.
/// @tparam RGB_ORDER the rgb ordering for the LEDs (e.g. what order red, green, and blue data is written out in)
/// @tparam LANES how many parallel lanes of output to write
/// @tparam MASK bitmask for the output lanes
template<EOrder RGB_ORDER, int LANES=1, uint32_t MASK=0xFFFFFFFF>
struct PixelController {
    const uint8_t *mData;    ///< pointer to the underlying LED data
    int mLen;                ///< number of LEDs in the data for one lane
    int mLenRemaining;       ///< counter for the number of LEDs left to process
    uint8_t d[3];            ///< values for the scaled dither signal @see init_binary_dithering()
    uint8_t e[3];            ///< values for the unscaled dither signal @see init_binary_dithering()
    int8_t mAdvance;         ///< how many bytes to advance the pointer by each time. For CRGB this is 3.
    int mOffsets[LANES];     ///< the number of bytes to offset each lane from the starting pointer @see initOffsets()
    ColorAdjustment mColorAdjustment;

    enum {
        kLanes = LANES,
        kMask = MASK
    };

    FASTLED_FORCE_INLINE fl::PixelIterator as_iterator(const Rgbw& rgbw) {
        return fl::PixelIterator(this, rgbw);
    }

    void disableColorAdjustment() {
        #if FASTLED_HD_COLOR_MIXING
        mColorAdjustment.premixed = CRGB(mColorAdjustment.brightness, mColorAdjustment.brightness, mColorAdjustment.brightness);
        mColorAdjustment.color = CRGB(0xff, 0xff, 0xff);
        #endif
    }

    /// Copy constructor
    /// @param other the object to copy 
    PixelController(const PixelController & other) {
        copy(other);
    }

    template<EOrder RGB_ORDER_OTHER>
    PixelController(const PixelController<RGB_ORDER_OTHER, LANES, MASK> & other) {
        copy(other);
    }

    template<typename PixelControllerT>
    void copy(const PixelControllerT& other) {
        static_assert(int(kLanes) == int(PixelControllerT::kLanes), "PixelController lanes must match or mOffsets will be wrong");
        static_assert(int(kMask) == int(PixelControllerT::kMask), "PixelController mask must match or else one or the other controls different lanes");
        d[0] = other.d[0];
        d[1] = other.d[1];
        d[2] = other.d[2];
        e[0] = other.e[0];
        e[1] = other.e[1];
        e[2] = other.e[2];
        mData = other.mData;
        mColorAdjustment = other.mColorAdjustment;
        mAdvance = other.mAdvance;
        mLenRemaining = mLen = other.mLen;
        for(int i = 0; i < LANES; ++i) { mOffsets[i] = other.mOffsets[i]; }
    }

    /// Initialize the PixelController::mOffsets array based on the length of the strip
    /// @param len the number of LEDs in one lane of the strip
    void initOffsets(int len) {
        int nOffset = 0;
        for(int i = 0; i < LANES; ++i) {
            mOffsets[i] = nOffset;
            if((1<<i) & MASK) { nOffset += (len * mAdvance); }
        }
    }

    /// Constructor
    /// @param d pointer to LED data
    /// @param len length of the LED data
    /// @param color_adjustment LED scale values
    /// @param dither dither setting for the LEDs
    /// @param advance whether the pointer (d) should advance per LED
    /// @param skip if the pointer is advancing, how many bytes to skip in addition to 3
    PixelController(
            const uint8_t *d, int len, ColorAdjustment color_adjustment,
            EDitherMode dither, bool advance, uint8_t skip)
                : mData(d), mLen(len), mLenRemaining(len), mColorAdjustment(color_adjustment) {
        enable_dithering(dither);
        mData += skip;
        mAdvance = (advance) ? 3+skip : 0;
        initOffsets(len);
    }

    /// Constructor
    /// @param d pointer to LED data
    /// @param len length of the LED data
    /// @param color_adjustment LED scale values
    /// @param dither dither setting for the LEDs
    PixelController(
            const CRGB *d, int len, ColorAdjustment color_adjustment,
            EDitherMode dither)
                : mData((const uint8_t*)d), mLen(len), mLenRemaining(len), mColorAdjustment(color_adjustment) {
        enable_dithering(dither);
        mAdvance = 3;
        initOffsets(len);
    }

    /// Constructor
    /// @param d pointer to LED data
    /// @param len length of the LED data
    /// @param color_adjustment LED scale values
    /// @param dither dither setting for the LEDs
    PixelController(
            const CRGB &d, int len, ColorAdjustment color_adjustment, EDitherMode dither)
                : mData((const uint8_t*)&d), mLen(len), mLenRemaining(len), mColorAdjustment(color_adjustment) {
        enable_dithering(dither);
        mAdvance = 0;
        initOffsets(len);
    }

    #if FASTLED_HD_COLOR_MIXING
    uint8_t global_brightness() const {
        return mColorAdjustment.brightness;
    }
    #endif


#if !defined(NO_DITHERING) || (NO_DITHERING != 1)

/// Predicted max update rate, in Hertz
#define MAX_LIKELY_UPDATE_RATE_HZ     400

/// Minimum acceptable dithering rate, in Hertz
#define MIN_ACCEPTABLE_DITHER_RATE_HZ  50

/// The number of updates in a single dither cycle
#define UPDATES_PER_FULL_DITHER_CYCLE (MAX_LIKELY_UPDATE_RATE_HZ / MIN_ACCEPTABLE_DITHER_RATE_HZ)

/// Set "virtual bits" of dithering to the highest level
/// that is not likely to cause excessive flickering at
/// low brightness levels + low update rates. 
/// These pre-set values are a little ambitious, since
/// a 400Hz update rate for WS2811-family LEDs is only
/// possible with 85 pixels or fewer.
/// Once we have a "number of milliseconds since last update"
/// value available here, we can quickly calculate the correct
/// number of "virtual bits" on the fly with a couple of "if"
/// statements -- no division required.  At this point,
/// the division is done at compile time, so there's no runtime
/// cost, but the values are still hard-coded.
/// @todo Can these macros be replaced with constants scoped to PixelController::init_binary_dithering()?
#define RECOMMENDED_VIRTUAL_BITS ((UPDATES_PER_FULL_DITHER_CYCLE>1) + \
                                  (UPDATES_PER_FULL_DITHER_CYCLE>2) + \
                                  (UPDATES_PER_FULL_DITHER_CYCLE>4) + \
                                  (UPDATES_PER_FULL_DITHER_CYCLE>8) + \
                                  (UPDATES_PER_FULL_DITHER_CYCLE>16) + \
                                  (UPDATES_PER_FULL_DITHER_CYCLE>32) + \
                                  (UPDATES_PER_FULL_DITHER_CYCLE>64) + \
                                  (UPDATES_PER_FULL_DITHER_CYCLE>128) )

/// Alias for RECOMMENDED_VIRTUAL_BITS
#define VIRTUAL_BITS RECOMMENDED_VIRTUAL_BITS

#endif


    /// Set up the values for binary dithering
    void init_binary_dithering() {
#if !defined(NO_DITHERING) || (NO_DITHERING != 1)
        // R is the digther signal 'counter'.
        static uint8_t R = 0;
        ++R;

        // R is wrapped around at 2^ditherBits,
        // so if ditherBits is 2, R will cycle through (0,1,2,3)
        uint8_t ditherBits = VIRTUAL_BITS;
        R &= (0x01 << ditherBits) - 1;

        // Q is the "unscaled dither signal" itself.
        // It's initialized to the reversed bits of R.
        // If 'ditherBits' is 2, Q here will cycle through (0,128,64,192)
        uint8_t Q = 0;

        // Reverse bits in a byte
        {
            if(R & 0x01) { Q |= 0x80; }
            if(R & 0x02) { Q |= 0x40; }
            if(R & 0x04) { Q |= 0x20; }
            if(R & 0x08) { Q |= 0x10; }
            if(R & 0x10) { Q |= 0x08; }
            if(R & 0x20) { Q |= 0x04; }
            if(R & 0x40) { Q |= 0x02; }
            if(R & 0x80) { Q |= 0x01; }
        }

        // Now we adjust Q to fall in the center of each range,
        // instead of at the start of the range.
        // If ditherBits is 2, Q will be (0, 128, 64, 192) at first,
        // and this adjustment makes it (31, 159, 95, 223).
        if( ditherBits < 8) {
            Q += 0x01 << (7 - ditherBits);
        }

        // D and E form the "scaled dither signal"
        // which is added to pixel values to affect the
        // actual dithering.

        // Setup the initial D and E values
        for(int i = 0; i < 3; ++i) {
                uint8_t s = mColorAdjustment.premixed.raw[i];
                e[i] = s ? (256/s) + 1 : 0;
                d[i] = scale8(Q, e[i]);
#if (FASTLED_SCALE8_FIXED == 1)
                if(d[i]) (--d[i]);
#endif
                if(e[i]) --e[i];
        }
#endif
    }

    /// Do we have n pixels left to process?
    /// @param n the number to check against
    /// @returns 'true' if there are more than n pixels left to process
    FASTLED_FORCE_INLINE bool has(int n) {
        return mLenRemaining >= n;
    }

    /// Toggle dithering enable
    /// If dithering is set to enabled, this will re-init the dithering values
    /// (init_binary_dithering()). Otherwise it will clear the stored dithering
    /// data.
    /// @param dither the dither setting
    void enable_dithering(EDitherMode dither) {
        switch(dither) {
            case BINARY_DITHER: init_binary_dithering(); break;
            default: d[0]=d[1]=d[2]=e[0]=e[1]=e[2]=0; break;
        }
    }

    /// Get the length of the LED strip
    /// @returns PixelController::mLen
    FASTLED_FORCE_INLINE int size() { return mLen; }

    /// Get the number of lanes of the Controller
    /// @returns LANES from template
    FASTLED_FORCE_INLINE int lanes() { return LANES; }

    /// Get the amount to advance the pointer by
    /// @returns PixelController::mAdvance
    FASTLED_FORCE_INLINE int advanceBy() { return mAdvance; }

    /// Advance the data pointer forward, adjust position counter
    FASTLED_FORCE_INLINE void advanceData() { mData += mAdvance; --mLenRemaining;}

    /// Step the dithering forward
    /// @note If updating here, be sure to update the asm version in clockless_trinket.h!
    FASTLED_FORCE_INLINE void stepDithering() {
            // IF UPDATING HERE, BE SURE TO UPDATE THE ASM VERSION IN
            // clockless_trinket.h!
            d[0] = e[0] - d[0];
            d[1] = e[1] - d[1];
            d[2] = e[2] - d[2];
    }

    /// Some chipsets pre-cycle the first byte, which means we want to cycle byte 0's dithering separately
    FASTLED_FORCE_INLINE void preStepFirstByteDithering() {
        d[RO(0)] = e[RO(0)] - d[RO(0)];
    }

    /// @name Template'd static functions for output
    /// These functions are used for retrieving LED data for the LED chipset output controllers.
    /// @{
    
    /// Read a byte of LED data
    /// @tparam SLOT The data slot in the output stream. This is used to select which byte of the output stream is being processed.
    /// @param pc reference to the pixel controller
    template<int SLOT>  FASTLED_FORCE_INLINE static uint8_t loadByte(PixelController & pc) { return pc.mData[RO(SLOT)]; }

    /// Read a byte of LED data for parallel output
    /// @tparam SLOT The data slot in the output stream. This is used to select which byte of the output stream is being processed.
    /// @param pc reference to the pixel controller
    /// @param lane the parallel output lane to read the byte for
    template<int SLOT>  FASTLED_FORCE_INLINE static uint8_t loadByte(PixelController & pc, int lane) { return pc.mData[pc.mOffsets[lane] + RO(SLOT)]; }

    /// Calculate a dither value using the per-channel dither data
    /// @tparam SLOT The data slot in the output stream. This is used to select which byte of the output stream is being processed.
    /// @param pc reference to the pixel controller
    /// @param b the color byte to dither
    /// @see PixelController::d
    template<int SLOT>  FASTLED_FORCE_INLINE static uint8_t dither(PixelController & pc, uint8_t b) { return b ? qadd8(b, pc.d[RO(SLOT)]) : 0; }
    
    /// Calculate a dither value
    /// @tparam SLOT The data slot in the output stream. This is used to select which byte of the output stream is being processed.
    /// @param b the color byte to dither
    /// @param d dither data
    template<int SLOT>  FASTLED_FORCE_INLINE static uint8_t dither(PixelController & , uint8_t b, uint8_t d) { return b ? qadd8(b,d) : 0; }

    /// Scale a value using the per-channel scale data
    /// @tparam SLOT The data slot in the output stream. This is used to select which byte of the output stream is being processed.
    /// @param pc reference to the pixel controller
    /// @param b the color byte to scale
    /// @see PixelController::mScale
    template<int SLOT>  FASTLED_FORCE_INLINE static uint8_t scale(PixelController & pc, uint8_t b) { return scale8(b, pc.mColorAdjustment.premixed.raw[RO(SLOT)]); }
    
    /// Scale a value
    /// @tparam SLOT The data slot in the output stream. This is used to select which byte of the output stream is being processed.
    /// @param b the byte to scale
    /// @param scale the scale value
    template<int SLOT>  FASTLED_FORCE_INLINE static uint8_t scale(PixelController & , uint8_t b, uint8_t scale) { return scale8(b, scale); }

    /// @name Composite shortcut functions for loading, dithering, and scaling
    /// These composite functions will load color data, dither it, and scale it
    /// all at once so that it's ready for the output controller to send to the
    /// LEDs.
    /// @{


    /// Loads, dithers, and scales a single byte for a given output slot, using class dither and scale values
    /// @tparam SLOT The data slot in the output stream. This is used to select which byte of the output stream is being processed.
    /// @param pc reference to the pixel controller
    template<int SLOT>  FASTLED_FORCE_INLINE static uint8_t loadAndScale(PixelController & pc) { return scale<SLOT>(pc, pc.dither<SLOT>(pc, pc.loadByte<SLOT>(pc))); }

    /// Loads, dithers, and scales a single byte for a given output slot and lane, using class dither and scale values
    /// @tparam SLOT The data slot in the output stream. This is used to select which byte of the output stream is being processed.
    /// @param pc reference to the pixel controller
    /// @param lane the parallel output lane to read the byte for
    template<int SLOT>  FASTLED_FORCE_INLINE static uint8_t loadAndScale(PixelController & pc, int lane) { return scale<SLOT>(pc, pc.dither<SLOT>(pc, pc.loadByte<SLOT>(pc, lane))); }

    /// Loads, dithers, and scales a single byte for a given output slot and lane
    /// @tparam SLOT The data slot in the output stream. This is used to select which byte of the output stream is being processed.
    /// @param pc reference to the pixel controller
    /// @param lane the parallel output lane to read the byte for
    /// @param d the dither data for the byte
    /// @param scale the scale data for the byte
    template<int SLOT>  FASTLED_FORCE_INLINE static uint8_t loadAndScale(PixelController & pc, int lane, uint8_t d, uint8_t scale) { return scale8(pc.dither<SLOT>(pc, pc.loadByte<SLOT>(pc, lane), d), scale); }

    /// Loads and scales a single byte for a given output slot and lane
    /// @tparam SLOT The data slot in the output stream. This is used to select which byte of the output stream is being processed.
    /// @param pc reference to the pixel controller
    /// @param lane the parallel output lane to read the byte for
    /// @param scale the scale data for the byte
    template<int SLOT>  FASTLED_FORCE_INLINE static uint8_t loadAndScale(PixelController & pc, int lane, uint8_t scale) { return scale8(pc.loadByte<SLOT>(pc, lane), scale); }


    /// A version of loadAndScale() that advances the output data pointer
    /// @param pc reference to the pixel controller
    template<int SLOT>  FASTLED_FORCE_INLINE static uint8_t advanceAndLoadAndScale(PixelController & pc) { pc.advanceData(); return pc.loadAndScale<SLOT>(pc); }

    /// A version of loadAndScale() that advances the output data pointer
    /// @param pc reference to the pixel controller
    /// @param lane the parallel output lane to read the byte for
    template<int SLOT>  FASTLED_FORCE_INLINE static uint8_t advanceAndLoadAndScale(PixelController & pc, int lane) { pc.advanceData(); return pc.loadAndScale<SLOT>(pc, lane); }

    /// A version of loadAndScale() that advances the output data pointer without dithering
    /// @param pc reference to the pixel controller
    /// @param lane the parallel output lane to read the byte for
    /// @param scale the scale data for the byte
    template<int SLOT>  FASTLED_FORCE_INLINE static uint8_t advanceAndLoadAndScale(PixelController & pc, int lane, uint8_t scale) { pc.advanceData(); return pc.loadAndScale<SLOT>(pc, lane, scale); }

    /// @} Composite shortcut functions


    /// @name Data retrieval functions
    /// These functions retrieve channel-specific data from the class,
    /// arranged in output order.
    /// @{

    /// Gets the dithering data for the provided output slot
    /// @tparam SLOT The data slot in the output stream. This is used to select which byte of the output stream is being processed.
    /// @param pc reference to the pixel controller
    /// @returns dithering data for the given channel
    /// @see PixelController::d
    template<int SLOT>  FASTLED_FORCE_INLINE static uint8_t getd(PixelController & pc) { return pc.d[RO(SLOT)]; }

    /// Gets the scale data for the provided output slot
    /// @tparam SLOT The data slot in the output stream. This is used to select which byte of the output stream is being processed.
    /// @param pc reference to the pixel controller
    /// @returns scale data for the given channel
    /// @see PixelController::mScale
    template<int SLOT>  FASTLED_FORCE_INLINE static uint8_t getscale(PixelController & pc) { return pc.mColorAdjustment.premixed.raw[RO(SLOT)]; }

    /// @} Data retrieval functions


    /// @} Template'd static functions for output

    // Helper functions to get around gcc stupidities
    FASTLED_FORCE_INLINE uint8_t loadAndScale0(int lane, uint8_t scale) { return loadAndScale<0>(*this, lane, scale); }  ///< non-template alias of loadAndScale<0>()
    FASTLED_FORCE_INLINE uint8_t loadAndScale1(int lane, uint8_t scale) { return loadAndScale<1>(*this, lane, scale); }  ///< non-template alias of loadAndScale<1>()
    FASTLED_FORCE_INLINE uint8_t loadAndScale2(int lane, uint8_t scale) { return loadAndScale<2>(*this, lane, scale); }  ///< non-template alias of loadAndScale<2>()
    FASTLED_FORCE_INLINE uint8_t advanceAndLoadAndScale0(int lane, uint8_t scale) { return advanceAndLoadAndScale<0>(*this, lane, scale); }  ///< non-template alias of advanceAndLoadAndScale<0>()
    FASTLED_FORCE_INLINE uint8_t stepAdvanceAndLoadAndScale0(int lane, uint8_t scale) { stepDithering(); return advanceAndLoadAndScale<0>(*this, lane, scale); }  ///< stepDithering() and advanceAndLoadAndScale0()

    FASTLED_FORCE_INLINE uint8_t loadAndScale0(int lane) { return loadAndScale<0>(*this, lane); }  ///< @copydoc loadAndScale0(int, uint8_t)
    FASTLED_FORCE_INLINE uint8_t loadAndScale1(int lane) { return loadAndScale<1>(*this, lane); }  ///< @copydoc loadAndScale1(int, uint8_t)
    FASTLED_FORCE_INLINE uint8_t loadAndScale2(int lane) { return loadAndScale<2>(*this, lane); }  ///< @copydoc loadAndScale2(int, uint8_t)
    FASTLED_FORCE_INLINE uint8_t advanceAndLoadAndScale0(int lane) { return advanceAndLoadAndScale<0>(*this, lane); }  ///< @copydoc advanceAndLoadAndScale0(int, uint8_t)
    FASTLED_FORCE_INLINE uint8_t stepAdvanceAndLoadAndScale0(int lane) { stepDithering(); return advanceAndLoadAndScale<0>(*this, lane); }  ///< @copydoc stepAdvanceAndLoadAndScale0(int, uint8_t)

    // LoadAndScale0 loads the pixel data in the order specified by RGB_ORDER and then scales it by the color correction values
    // For example in color order GRB, loadAndScale0() will return the green channel scaled by the color correction value for green.
    FASTLED_FORCE_INLINE uint8_t loadAndScale0() { return loadAndScale<0>(*this); }  ///< @copydoc loadAndScale0(int, uint8_t)
    FASTLED_FORCE_INLINE uint8_t loadAndScale1() { return loadAndScale<1>(*this); }  ///< @copydoc loadAndScale1(int, uint8_t)
    FASTLED_FORCE_INLINE uint8_t loadAndScale2() { return loadAndScale<2>(*this); }  ///< @copydoc loadAndScale2(int, uint8_t)
    FASTLED_FORCE_INLINE uint8_t advanceAndLoadAndScale0() { return advanceAndLoadAndScale<0>(*this); }  ///< @copydoc advanceAndLoadAndScale0(int, uint8_t)
    FASTLED_FORCE_INLINE uint8_t stepAdvanceAndLoadAndScale0() { stepDithering(); return advanceAndLoadAndScale<0>(*this); }  ///< @copydoc stepAdvanceAndLoadAndScale0(int, uint8_t)

    FASTLED_FORCE_INLINE uint8_t getScale0() { return getscale<0>(*this); }  ///< non-template alias of getscale<0>()
    FASTLED_FORCE_INLINE uint8_t getScale1() { return getscale<1>(*this); }  ///< non-template alias of getscale<1>()
    FASTLED_FORCE_INLINE uint8_t getScale2() { return getscale<2>(*this); }  ///< non-template alias of getscale<2>()

    #if FASTLED_HD_COLOR_MIXING
    template<int SLOT>  FASTLED_FORCE_INLINE static uint8_t getScaleFullBrightness(PixelController & pc) { return pc.mColorAdjustment.color.raw[RO(SLOT)]; }
    // Gets the color corection and also the brightness as seperate values.
    // This is needed for the higher precision chipsets like the APA102.
    FASTLED_FORCE_INLINE void getHdScale(uint8_t* c0, uint8_t* c1, uint8_t* c2, uint8_t* brightness) {
        *c0 = getScaleFullBrightness<0>(*this);
        *c1 = getScaleFullBrightness<1>(*this);
        *c2 = getScaleFullBrightness<2>(*this);
        *brightness = mColorAdjustment.brightness;
    }
    #endif


    FASTLED_FORCE_INLINE void loadAndScale_APA102_HD(uint8_t *b0_out, uint8_t *b1_out,
                                                     uint8_t *b2_out,
                                                     uint8_t *brightness_out) {
        CRGB rgb = CRGB(mData[0], mData[1], mData[2]);
        uint8_t brightness = 0;
        if (rgb) {
            #if FASTLED_HD_COLOR_MIXING
            brightness = mColorAdjustment.brightness;
            CRGB scale = mColorAdjustment.color;
            #else
            brightness = 255;
            CRGB scale = mColorAdjustment.premixed;
            #endif
            fl::five_bit_hd_gamma_bitshift(
                rgb,
                scale,
                brightness,
                &rgb,
                &brightness);
        }
        const uint8_t b0_index = RGB_BYTE0(RGB_ORDER);
        const uint8_t b1_index = RGB_BYTE1(RGB_ORDER);
        const uint8_t b2_index = RGB_BYTE2(RGB_ORDER);
        *b0_out = rgb.raw[b0_index];
        *b1_out = rgb.raw[b1_index];
        *b2_out = rgb.raw[b2_index];
        *brightness_out = brightness;
    }

    FASTLED_FORCE_INLINE void loadAndScaleRGB(uint8_t *b0_out, uint8_t *b1_out,
                                              uint8_t *b2_out) {
        *b0_out = loadAndScale0();
        *b1_out = loadAndScale1();
        *b2_out = loadAndScale2();
    }

    // WS2816B has native 16 bit/channel color and internal 4 bit gamma correction.
    // So we don't do gamma here, and we don't bother with dithering.
    FASTLED_FORCE_INLINE void loadAndScale_WS2816_HD(uint16_t *s0_out, uint16_t *s1_out, uint16_t *s2_out) {
        // Note that the WS2816 has a 4 bit gamma correction built in. To improve things this algorithm may
        // change in the future with a partial gamma correction that is completed by the chipset gamma
        // correction.
        uint16_t r16 = map8_to_16(mData[0]);
        uint16_t g16 = map8_to_16(mData[1]);
        uint16_t b16 = map8_to_16(mData[2]);
        if (r16 || g16 || b16) {
    #if FASTLED_HD_COLOR_MIXING
            uint8_t brightness = mColorAdjustment.brightness;
            CRGB scale = mColorAdjustment.color;
    #else
            uint8_t brightness = 255;
            CRGB scale = mColorAdjustment.premixed;
    #endif
            if (scale[0] != 255) {
                r16 = scale16by8(r16, scale[0]);
            }
            if (scale[1] != 255) {
                g16 = scale16by8(g16, scale[1]);
            }
            if (scale[2] != 255) {
                b16 = scale16by8(b16, scale[2]);
            }
            if (brightness != 255) {
                r16 = scale16by8(r16, brightness);
                g16 = scale16by8(g16, brightness);
                b16 = scale16by8(b16, brightness);
            }
        }
        uint16_t rgb16[3] = {r16, g16, b16};
        const uint8_t s0_index = RGB_BYTE0(RGB_ORDER);
        const uint8_t s1_index = RGB_BYTE1(RGB_ORDER);
        const uint8_t s2_index = RGB_BYTE2(RGB_ORDER);
        *s0_out = rgb16[s0_index];
        *s1_out = rgb16[s1_index];
        *s2_out = rgb16[s2_index];
    }

    FASTLED_FORCE_INLINE void loadAndScaleRGBW(Rgbw rgbw, uint8_t *b0_out, uint8_t *b1_out,
                                               uint8_t *b2_out, uint8_t *b3_out) {
#ifdef __AVR__
        // Don't do RGBW conversion for AVR, just set the W pixel to black.
        uint8_t out[4] = {
            // Get the pixels in native order.
            loadAndScale0(),
            loadAndScale1(),
            loadAndScale2(),
            0,
        };
        EOrderW w_placement = rgbw.w_placement;
        // Apply w-component insertion.
        fl::rgbw_partial_reorder(
            w_placement, out[0], out[1], out[2],
            0, // Pre-ordered RGB data with a 0 white component.
            b0_out, b1_out, b2_out, b3_out);
#else
        const uint8_t b0_index = RGB_BYTE0(RGB_ORDER);  // Needed to re-order RGB back into led native order.
        const uint8_t b1_index = RGB_BYTE1(RGB_ORDER);
        const uint8_t b2_index = RGB_BYTE2(RGB_ORDER);
        // Get the naive RGB data order in r,g,b order.
        CRGB rgb(mData[0], mData[1], mData[2]);
        uint8_t w = 0;
        fl::rgb_2_rgbw(rgbw.rgbw_mode,
                   rgbw.white_color_temp,
                   rgb.r, rgb.g, rgb.b,  // Input colors
                   mColorAdjustment.premixed.r, mColorAdjustment.premixed.g, mColorAdjustment.premixed.b,  // How these colors are scaled for color balance.
                   &rgb.r, &rgb.g, &rgb.b, &w);
        // Now finish the ordering so that the output is in the native led order for all of RGBW.
        fl::rgbw_partial_reorder(
            rgbw.w_placement,
            rgb.raw[b0_index],  // in-place re-ordering for the RGB data.
            rgb.raw[b1_index],
            rgb.raw[b2_index],
            w,  // The white component is not ordered in this call.
            b0_out, b1_out, b2_out, b3_out);  // RGBW data now in total native led order.
#endif
    }
};


FASTLED_NAMESPACE_END


FL_DISABLE_WARNING_POP
