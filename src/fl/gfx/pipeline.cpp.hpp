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

}  // namespace

bool buildStreamingPipelineQ16(const SourceProfile& source,
                               const EmitterProfile& device,
                               StreamingPipelineQ16* out) FL_NO_EXCEPT {
    if (out == nullptr) {
        return false;
    }
    if (!buildSourceMatrixQ16(source.primaries, &out->source)) {
        return false;
    }

    // Fold the source white's adaptation to D65 into the matrix above, so
    // the per-pixel path never pays for it. Skipped when the source already
    // renders to D65, where the transform is the identity and folding it
    // would only add a round trip's worth of quantization.
    const Chromaticity source_white = source.primaries.white;
    const bool already_d65 =
        source_white.x > kPipelineD65.x - 1e-4f &&
        source_white.x < kPipelineD65.x + 1e-4f &&
        source_white.y > kPipelineD65.y - 1e-4f &&
        source_white.y < kPipelineD65.y + 1e-4f;
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

    // The mapper owns clamping, so the stages above hand it the honest
    // target even when that is outside the hull.
    mapAndSolveDrivesQ16(pipeline.gamut, xyz, drives);

    // C4's single amplitude stage, last so nothing downstream can rescale a
    // channel on its own.
    applyFluxScalar(pipeline.flux, span<i32>(drives, 3));
}

}  // namespace fl
