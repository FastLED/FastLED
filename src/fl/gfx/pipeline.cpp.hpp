// ok no header - implementation for fl/gfx/pipeline.h

#include "fl/gfx/pipeline.h"

#include "fl/gfx/chromatic_adaptation.h"
#include "fl/gfx/transfer.h"

namespace fl {

namespace {

/// The working domain's rendering white.
///
/// Named for this file because .cpp.hpp files share a translation unit under
/// the unity build, so an anonymous namespace does not isolate it.
constexpr Chromaticity kPipelineD65 = Chromaticity(0.3127f, 0.3290f);

bool validResponseLut(const u16* values, u16 size) FL_NO_EXCEPT {
    if (values == nullptr || size < 2 || values[0] != 0 ||
        values[size - 1] != 65535) {
        return false;
    }
    for (u16 i = 1; i < size; ++i) {
        if (values[i] < values[i - 1]) return false;
    }
    return true;
}

/// Invert piecewise-linear code-to-light data. The first sample that reaches
/// the requested light wins at a plateau; zero and full-scale are exact.
i32 inverseResponseQ16(i32 light, const vector<u16>& values) FL_NO_EXCEPT {
    if (light <= 0) return 0;
    if (light >= 65536) return 65536;
    const u32 target =
        (static_cast<u32>(light) * 65535u + 32768u) >> 16;
    u32 low = 1;
    u32 high = static_cast<u32>(values.size() - 1);
    while (low < high) {
        const u32 middle = low + (high - low) / 2;
        if (values[middle] >= target) high = middle;
        else low = middle + 1;
    }
    high = low;
    const u32 low_value = values[high - 1];
    const u32 high_value = values[high];
    // A plateau at the requested light maps to its first attainable code.
    if (high_value == low_value) {
        return static_cast<i32>((high - 1) * 65536u /
                                static_cast<u32>(values.size() - 1));
    }
    const u32 interval = high_value - low_value;
    const u32 fraction =
        ((target - low_value) * 65536u + interval / 2) / interval;
    const u32 segments = static_cast<u32>(values.size() - 1);
    return static_cast<i32>(((high - 1) * 65536u + fraction + segments / 2) /
                            segments);
}

}  // namespace

bool buildStreamingPipelineQ16(const SourceProfile& source,
                               const colorimetric_response::EmitterProfile& device,
                               GamutPolicy policy,
                               StreamingPipelineQ16* out) FL_NO_EXCEPT {
    if (out == nullptr) {
        return false;
    }
    out->gamut_policy = policy;
    if (!buildSourceMatrixQ16(source.primaries, &out->source)) {
        return false;
    }

    // Fold the source white's adaptation to D65 into the matrix above, so
    // the per-pixel path never pays for it. Skipped when the source already
    // renders to D65, where the transform is the identity and folding it
    // would only add a round trip's worth of quantization.
    //
    // Compared in s16.16 from the float's bits, so this check links no float
    // either (FastLED#4458). 1e-4 is 6.55 steps; 7 keeps the same window.
    const Chromaticity source_white = source.primaries.white;
    i32 white_x = 0;
    i32 white_y = 0;
    if (!q16FromFloatBits(source_white.x, &white_x) ||
        !q16FromFloatBits(source_white.y, &white_y)) {
        return false;
    }
    constexpr i32 kD65XQ16 = 20493;  // 0.3127
    constexpr i32 kD65YQ16 = 21561;  // 0.3290
    constexpr i32 kD65ToleranceQ16 = 7;  // ~1e-4
    const bool already_d65 = white_x > kD65XQ16 - kD65ToleranceQ16 &&
                             white_x < kD65XQ16 + kD65ToleranceQ16 &&
                             white_y > kD65YQ16 - kD65ToleranceQ16 &&
                             white_y < kD65YQ16 + kD65ToleranceQ16;
    if (!already_d65) {
        AdaptationMatrixQ16 adaptation;
        if (!buildBradfordMatrixQ16(source_white, kPipelineD65, &adaptation)) {
            return false;
        }
        foldAdaptationIntoSourceMatrix(adaptation, &out->source);
    }

    if (!buildGamutMapQ16(device, &out->gamut)) {
        return false;
    }
    out->response.reset();
    if (device.response_lut_size != 0) {
        const u16 size = device.response_lut_size;
        if (!validResponseLut(device.response_lut_r, size) ||
            !validResponseLut(device.response_lut_g, size) ||
            !validResponseLut(device.response_lut_b, size)) {
            return false;
        }
        shared_ptr<ResponseLutsQ16> response = make_shared<ResponseLutsQ16>();
        if (!response) return false;
        response->red.assign(device.response_lut_r, device.response_lut_r + size);
        response->green.assign(device.response_lut_g, device.response_lut_g + size);
        response->blue.assign(device.response_lut_b, device.response_lut_b + size);
        if (response->red.size() != size || response->green.size() != size ||
            response->blue.size() != size) return false;
        out->response = response;
    }
    out->transfer = source.transfer;
    out->flux = FluxScalar::unity();
    return true;
}

void setPipelineFluxQ16(StreamingPipelineQ16* pipeline,
                        FluxScalar flux) FL_NO_EXCEPT {
    if (pipeline == nullptr) {
        return;
    }
    pipeline->flux = flux;
}

void processPixelQ16(const StreamingPipelineQ16& pipeline, u8 r, u8 g, u8 b,
                     i32 (&drives)[3]) FL_NO_EXCEPT {
    // One semantic conversion per channel: code straight to linear light.
    // Nothing here writes an RGB8 intermediate, which is what B3 asks and
    // what ci/tests/test_no_rgb8_intermediate.py enforces structurally.
    const u16 linear_r = decodeTransferU16(pipeline.transfer, r);
    const u16 linear_g = decodeTransferU16(pipeline.transfer, g);
    const u16 linear_b = decodeTransferU16(pipeline.transfer, b);

    i32 xyz[3];
    linearRgbToXyzQ16(pipeline.source, linear_r, linear_g, linear_b, xyz);

    if (pipeline.gamut_policy == GamutPolicy::Clamp) {
        // The caller asked for the cheap answer. Solve and clip, which is
        // `map_clip` from the P7 study -- about 20 dE2000 from the reference
        // where the mapper is at 0.15, and offered only because the binding
        // offers it.
        solveRgbDrivesQ16(pipeline.gamut.solve, xyz, drives);
        for (int i = 0; i < 3; ++i) {
            if (drives[i] < 0) {
                drives[i] = 0;
            } else if (drives[i] > 65536) {
                drives[i] = 65536;
            }
        }
    } else {
        // The mapper owns clamping, so the stages above hand it the honest
        // target even when that is outside the hull.
        mapAndSolveDrivesQ16(pipeline.gamut, xyz, drives);
    }

    // C4's single amplitude stage scales linear light. Physical response
    // inversion follows it; dimming compensated drive codes would skew hue.
    applyFluxScalar(pipeline.flux, span<i32>(drives, 3));
    if (pipeline.response) {
        drives[0] = inverseResponseQ16(drives[0], pipeline.response->red);
        drives[1] = inverseResponseQ16(drives[1], pipeline.response->green);
        drives[2] = inverseResponseQ16(drives[2], pipeline.response->blue);
    }
}

}  // namespace fl
