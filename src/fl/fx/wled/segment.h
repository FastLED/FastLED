#pragma once

#include "fl/stl/vector.h"
#include "fl/stl/string.h"

namespace fl {

/**
 * @brief WLED segment configuration
 *
 * Represents a single segment with independent control of LEDs.
 * Segments allow multiple regions of an LED strip to have different
 * colors, brightness, and effects.
 */
struct WLEDSegment {
    u8 mId = 0;              // Segment ID (0-255)
    u16 mStart = 0;          // Starting LED index (inclusive)
    u16 mStop = 0;           // Ending LED index (exclusive)
    bool mOn = true;              // Segment power state
    u8 mBri = 255;           // Segment brightness (0-255)

    // Color slots: each color is [R,G,B] or [R,G,B,W]
    // Index 0: Primary color, Index 1: Secondary color, Index 2: Tertiary color
    fl::vector<fl::vector<u8>> mColors;

    // Layout properties
    u16 mLen = 0;            // Segment length (alternative to stop, 0=use stop)
    u8 mGrp = 1;             // LED grouping factor (default 1)
    u8 mSpc = 0;             // Spacing between groups (default 0)
    u16 mOf = 0;             // Group offset (default 0)

    // Effect properties
    u8 mFx = 0;              // Effect ID (0-fxcount-1)
    u8 mSx = 128;            // Effect speed (0-255, default 128)
    u8 mIx = 128;            // Effect intensity (0-255, default 128)
    u8 mPal = 0;             // Palette ID (0-palcount-1)
    u8 mC1 = 128;            // Effect custom parameter 1 (0-255)
    u8 mC2 = 128;            // Effect custom parameter 2 (0-255)
    u8 mC3 = 16;             // Effect custom parameter 3 (0-255)

    // Effect option flags
    bool mSel = false;            // Segment selected for API control
    bool mRev = false;            // Reverse segment direction
    bool mMi = false;             // Mirror effect within segment
    bool mO1 = false;             // Effect option flag 1
    bool mO2 = false;             // Effect option flag 2
    bool mO3 = false;             // Effect option flag 3

    // Other properties
    u16 mCct = 0;            // Color temperature (0-255 or Kelvin 1900-10091, 0=not set)
    u8 mSi = 0;              // Sound simulation mode (0-3)
    u8 mM12 = 0;             // Segment mapping mode (0-3)
    bool mRpt = false;            // Repeat segment pattern
    fl::string mName;             // Segment name (optional)

    // Individual LED control: stores per-LED colors as RGB triplets
    // Each inner vector is [R,G,B] for a specific LED
    fl::vector<fl::vector<u8>> mIndividualLeds;
};

} // namespace fl
