#pragma once

#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"

namespace fl {

/// CIE 1931 xy chromaticity, independent of encoded storage and chipset.
struct Chromaticity {
    float x;
    float y;
    constexpr Chromaticity(float x_in = 0.0f, float y_in = 0.0f) FL_NO_EXCEPT
        : x(x_in), y(y_in) {}
};

struct RgbPrimaries {
    Chromaticity red;
    Chromaticity green;
    Chromaticity blue;
    Chromaticity white;
    constexpr RgbPrimaries(Chromaticity r = Chromaticity(),
                           Chromaticity g = Chromaticity(),
                           Chromaticity b = Chromaticity(),
                           Chromaticity w = Chromaticity()) FL_NO_EXCEPT
        : red(r), green(g), blue(b), white(w) {}
};

enum class TransferFunction : u8 { Linear, Srgb, Bt709 };

/// A source declaration, not a container or an output-device selection.
struct SourceProfile {
    RgbPrimaries primaries;
    TransferFunction transfer;
    constexpr SourceProfile(RgbPrimaries p = srgbPrimaries(),
                            TransferFunction t = TransferFunction::Linear) FL_NO_EXCEPT
        : primaries(p), transfer(t) {}
    static constexpr SourceProfile linearSrgb() FL_NO_EXCEPT {
        return SourceProfile(srgbPrimaries(), TransferFunction::Linear);
    }
    static constexpr SourceProfile srgbBt709() FL_NO_EXCEPT {
        return SourceProfile(srgbPrimaries(), TransferFunction::Srgb);
    }
    static constexpr SourceProfile bt709() FL_NO_EXCEPT {
        return SourceProfile(srgbPrimaries(), TransferFunction::Bt709);
    }
    static constexpr SourceProfile displayP3() FL_NO_EXCEPT {
        return SourceProfile(RgbPrimaries(Chromaticity(.680f, .320f), Chromaticity(.265f, .690f), Chromaticity(.150f, .060f), d65()), TransferFunction::Srgb);
    }
    static constexpr SourceProfile bt2020() FL_NO_EXCEPT {
        return SourceProfile(RgbPrimaries(Chromaticity(.708f, .292f), Chromaticity(.170f, .797f), Chromaticity(.131f, .046f), d65()), TransferFunction::Bt709);
    }
    static constexpr SourceProfile custom(RgbPrimaries p, TransferFunction t) FL_NO_EXCEPT { return SourceProfile(p, t); }
private:
    static constexpr Chromaticity d65() FL_NO_EXCEPT { return Chromaticity(.3127f, .3290f); }
    static constexpr RgbPrimaries srgbPrimaries() FL_NO_EXCEPT {
        return RgbPrimaries(Chromaticity(.640f, .330f), Chromaticity(.300f, .600f), Chromaticity(.150f, .060f), d65());
    }
};

enum class GamutPolicy : u8 { Clamp, ChromaCompress };
enum class FiveBitSemantics : u8 { NotApplicable, SecondarySlowPwm, CurrentGain, Unknown };

}  // namespace fl
