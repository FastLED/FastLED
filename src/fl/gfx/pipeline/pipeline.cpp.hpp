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

bool wideWhiteColumnQ16(const float (&xy)[2], float luminance,
                        i32 (&out)[3]) FL_NO_EXCEPT {
    i32 xy_q16[2];
    i32 luminance_q16 = 0;
    if (!q16FromFloatBits(xy[0], &xy_q16[0]) ||
        !q16FromFloatBits(xy[1], &xy_q16[1]) ||
        !q16FromFloatBits(luminance, &luminance_q16) ||
        luminance_q16 <= 0) return false;
    i64 column[3];
    if (!detail::xyzColumnQ16(xy_q16, luminance_q16, column)) return false;
    for (int i = 0; i < 3; ++i) {
        if (column[i] < 0 || column[i] > 2147483647LL) return false;
        out[i] = static_cast<i32>(column[i]);
    }
    return true;
}

bool buildWidePipelineQ16(
    const colorimetric_response::EmitterProfile& device,
    const Chromaticity* target_white,
    const EmitterSolveMatrixQ16* effective_solve,
    shared_ptr<const WidePipelineQ16>* out) FL_NO_EXCEPT {
    shared_ptr<WidePipelineQ16> wide = make_shared<WidePipelineQ16>();
    if (!wide) return false;
    wide->topology = device.topology;
    i32 white1[3];
    i32 white2[3] = {};
    if (!wideWhiteColumnQ16(device.xy_white1, device.lum_white1, white1))
        return false;
    if (device.topology == colorimetric_response::EmitterTopology::RGBWW &&
        !wideWhiteColumnQ16(device.xy_white2, device.lum_white2, white2))
        return false;
    if (target_white != nullptr) {
        AdaptationMatrixQ16 to_working;
        if (!buildBradfordMatrixQ16(*target_white, kPipelineD65,
                                    &to_working)) return false;
        i32 adapted1[3];
        adaptXyzQ16(to_working, white1, adapted1);
        for (int i = 0; i < 3; ++i) white1[i] = adapted1[i];
        if (device.topology == colorimetric_response::EmitterTopology::RGBWW) {
            i32 adapted2[3];
            adaptXyzQ16(to_working, white2, adapted2);
            for (int i = 0; i < 3; ++i) white2[i] = adapted2[i];
        }
    }
    const WhiteAllocationPolicy allocation = WhiteAllocationPolicy::WhitePreferred;
    if (device.topology == colorimetric_response::EmitterTopology::RGBW) {
        const bool ok = effective_solve == nullptr
            ? buildGamutMapRgbwQ16(device, white1, allocation, &wide->rgbw)
            : buildGamutMapRgbwFromSolveQ16(device, *effective_solve, white1,
                                            allocation, &wide->rgbw);
        if (!ok) return false;
    } else if (device.topology == colorimetric_response::EmitterTopology::RGBWW) {
        const bool ok = effective_solve == nullptr
            ? buildGamutMapRgbwwQ16(device, white1, white2, allocation,
                                    &wide->rgbww)
            : buildGamutMapRgbwwFromSolveQ16(device, *effective_solve, white1,
                                             white2, allocation, &wide->rgbww);
        if (!ok) return false;
    } else {
        return false;
    }
    if (device.response_lut_size != 0) {
        const u16 size = device.response_lut_size;
        if (!validResponseLut(device.response_lut_white1, size)) return false;
        wide->white1_response.assign(device.response_lut_white1,
                                     device.response_lut_white1 + size);
        if (wide->white1_response.size() != size) return false;
        if (device.topology == colorimetric_response::EmitterTopology::RGBWW) {
            if (!validResponseLut(device.response_lut_white2, size)) return false;
            wide->white2_response.assign(device.response_lut_white2,
                                         device.response_lut_white2 + size);
            if (wide->white2_response.size() != size) return false;
        }
    }
    *out = wide;
    return true;
}

}  // namespace

bool buildStreamingPipelineQ16(const SourceProfile& source,
                               const colorimetric_response::EmitterProfile& device,
                               GamutPolicy policy,
                               StreamingPipelineQ16* out,
                               const Chromaticity* target_white) FL_NO_EXCEPT {
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

    out->wide.reset();
    const bool is_rgb =
        device.topology == colorimetric_response::EmitterTopology::RGB;
    if (target_white == nullptr) {
        if (is_rgb) {
            if (!buildGamutMapQ16(device, &out->gamut)) return false;
        } else if (!buildWidePipelineQ16(device, nullptr, nullptr,
                                        &out->wide)) {
            return false;
        }
    } else {
        // The source and OKLab objective remain in D65 coordinates. Map the
        // physical gamut into that space by composing the inverse emitter
        // solve with Bradford D65 -> selected rendering white. The neutral
        // cap must be computed from the composed solve, not the raw profile.
        AdaptationMatrixQ16 to_target;
        if (!buildBradfordMatrixQ16(kPipelineD65, *target_white, &to_target))
            return false;
        EmitterSolveMatrixQ16 physical_solve;
        if (!buildRgbSolveMatrixQ16(device, &physical_solve)) return false;
        EmitterSolveMatrixQ16 effective_solve;
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                i64 sum = 0;
                constexpr i64 kMax = 9223372036854775807LL;
                for (int k = 0; k < 3; ++k) {
                    // Each i32 product fits i64, but their sum need not.
                    const i64 term = static_cast<i64>(physical_solve.m[row][k]) *
                                     to_target.m[k][col];
                    if ((term > 0 && sum > kMax - term) ||
                        (term < 0 && sum < -kMax - term)) return false;
                    sum += term;
                }
                // Keep rounding/negation away from the i64 boundary too.
                if (sum > 2147483647LL * 65536 + 32768 ||
                    sum < -2147483648LL * 65536 - 32768) return false;
                const i64 value = sum >= 0 ? (sum + 32768) >> 16
                                           : -((-sum + 32768) >> 16);
                if (value < -2147483647LL - 1 || value > 2147483647LL)
                    return false;
                effective_solve.m[row][col] = static_cast<i32>(value);
            }
        }
        if (is_rgb) {
            if (!buildGamutMapFromSolveQ16(effective_solve, &out->gamut))
                return false;
        } else if (!buildWidePipelineQ16(device, target_white,
                                        &effective_solve, &out->wide)) {
            return false;
        }
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

void processPixelLinearQ16(const StreamingPipelineQ16& pipeline, u8 r, u8 g,
                           u8 b, i32 (&drives)[3]) FL_NO_EXCEPT {
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

}

void processPixelQ16(const StreamingPipelineQ16& pipeline, u8 r, u8 g, u8 b,
                     i32 (&drives)[3]) FL_NO_EXCEPT {
    processPixelLinearQ16(pipeline, r, g, b, drives);
    // C4's single amplitude stage scales linear light. Physical response
    // inversion follows it; dimming compensated drive codes would skew hue.
    applyFluxScalar(pipeline.flux, span<i32>(drives, 3));
    if (pipeline.response) {
        drives[0] = inverseResponseQ16(drives[0], pipeline.response->red);
        drives[1] = inverseResponseQ16(drives[1], pipeline.response->green);
        drives[2] = inverseResponseQ16(drives[2], pipeline.response->blue);
    }
}

void processPixelWideLinearQ16(const StreamingPipelineQ16& pipeline, u8 r,
                               u8 g, u8 b, i32 (&drives)[5]) FL_NO_EXCEPT {
    for (int i = 0; i < 5; ++i) drives[i] = 0;
    if (!pipeline.wide) return;
    const u16 linear_r = decodeTransferU16(pipeline.transfer, r);
    const u16 linear_g = decodeTransferU16(pipeline.transfer, g);
    const u16 linear_b = decodeTransferU16(pipeline.transfer, b);
    i32 xyz[3];
    linearRgbToXyzQ16(pipeline.source, linear_r, linear_g, linear_b, xyz);
    const bool rgbww = pipeline.wide->topology ==
        colorimetric_response::EmitterTopology::RGBWW;
    if (rgbww) {
        i32 wide_drives[5];
        if (pipeline.gamut_policy == GamutPolicy::Clamp &&
            !allocateTwoWhiteDrivesQ16(pipeline.wide->rgbww.allocation,
                                       xyz, wide_drives)) {
            i32 rgb_drives[3];
            solveRgbDrivesQ16(pipeline.wide->rgbww.allocation.rgb_solve,
                              xyz, rgb_drives);
            for (int i = 0; i < 3; ++i) {
                wide_drives[i] = rgb_drives[i] < 0 ? 0 :
                    (rgb_drives[i] > 65536 ? 65536 : rgb_drives[i]);
            }
            wide_drives[3] = 0;
            wide_drives[4] = 0;
        } else if (pipeline.gamut_policy != GamutPolicy::Clamp) {
            mapAndAllocateRgbwwQ16(pipeline.wide->rgbww, xyz, wide_drives);
        }
        for (int i = 0; i < 5; ++i) drives[i] = wide_drives[i];
    } else {
        i32 wide_drives[4];
        if (pipeline.gamut_policy == GamutPolicy::Clamp &&
            !allocateEmitterDrivesQ16(pipeline.wide->rgbw.allocation,
                                      xyz, wide_drives)) {
            i32 rgb_drives[3];
            solveRgbDrivesQ16(pipeline.wide->rgbw.allocation.rgb_solve,
                              xyz, rgb_drives);
            for (int i = 0; i < 3; ++i) {
                wide_drives[i] = rgb_drives[i] < 0 ? 0 :
                    (rgb_drives[i] > 65536 ? 65536 : rgb_drives[i]);
            }
            wide_drives[3] = 0;
        } else if (pipeline.gamut_policy != GamutPolicy::Clamp) {
            mapAndAllocateRgbwQ16(pipeline.wide->rgbw, xyz, wide_drives);
        }
        for (int i = 0; i < 4; ++i) drives[i] = wide_drives[i];
    }
}

void processPixelWideQ16(const StreamingPipelineQ16& pipeline, u8 r, u8 g,
                         u8 b, i32 (&drives)[5]) FL_NO_EXCEPT {
    processPixelWideLinearQ16(pipeline, r, g, b, drives);
    if (!pipeline.wide) return;
    const bool rgbww = pipeline.wide && pipeline.wide->topology ==
        colorimetric_response::EmitterTopology::RGBWW;
    const int count = rgbww ? 5 : 4;
    applyFluxScalar(pipeline.flux, span<i32>(drives, count));
    if (pipeline.response) {
        drives[0] = inverseResponseQ16(drives[0], pipeline.response->red);
        drives[1] = inverseResponseQ16(drives[1], pipeline.response->green);
        drives[2] = inverseResponseQ16(drives[2], pipeline.response->blue);
        drives[3] = inverseResponseQ16(drives[3],
                                        pipeline.wide->white1_response);
        if (rgbww) {
            drives[4] = inverseResponseQ16(drives[4],
                                            pipeline.wide->white2_response);
        }
    }
}

i32 encodeLinearEmitterQ16(const StreamingPipelineQ16& pipeline, u8 emitter,
                            i32 light, FluxScalar flux) FL_NO_EXCEPT {
    i32 scaled[1] = {light};
    applyFluxScalar(flux, span<i32>(scaled, 1));
    if (!pipeline.response) return scaled[0];
    if (emitter == 0) return inverseResponseQ16(scaled[0], pipeline.response->red);
    if (emitter == 1) return inverseResponseQ16(scaled[0], pipeline.response->green);
    if (emitter == 2) return inverseResponseQ16(scaled[0], pipeline.response->blue);
    if (!pipeline.wide) return 0;
    if (emitter == 3) return inverseResponseQ16(scaled[0], pipeline.wide->white1_response);
    if (emitter == 4) return inverseResponseQ16(scaled[0], pipeline.wide->white2_response);
    return 0;
}

}  // namespace fl
