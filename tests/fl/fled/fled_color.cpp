// Tests for the .fled v1 `video.color` source-color contract.
//
// The normative rules live in src/fl/fled/FLED_FORMAT.md ("Source Color
// Metadata"), which mirrors the canonical ledmapper spec. This file walks
// every rule that document states:
//
//   - the default tuple {bt709, srgb, rgb, full} on absent metadata;
//   - key inheritance, scoped to formats that define a default tuple;
//   - rejection of linear/pq/hlg transfers on display-encoded formats;
//   - rgb16_linear requiring transfer: linear;
//   - reserved `limited` range and non-`rgb` matrix values;
//   - all-or-nothing declarations for gray8/rgbw8;
//   - custom primaries as CIE xy pairs.
//
// FastLED only CARRIES this declaration today - nothing here asserts that
// pixels are transformed by it.

#include "fl/fled/color.h"
#include "fl/fled/detail/pixel_format.h"
#include "fl/fled/fled.h"
#include "fl/stl/cstring.h"
#include "fl/stl/int.h"
#include "fl/stl/json.h"
#include "fl/stl/move.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;
using fl::fled::ColorMatrix;
using fl::fled::ColorPrimaries;
using fl::fled::ColorRange;
using fl::fled::ColorStatus;
using fl::fled::ColorTransfer;
using fl::fled::PixelFormat;
using fl::fled::VideoColor;

namespace {

// Resolve a JSON envelope literal against a pixel format.
ColorStatus resolve(const char* envelopeText, fl::u8 pixelFormat,
                    VideoColor* out) {
    fl::json env = fl::json::parse(fl::string(envelopeText));
    return fl::fled::resolveVideoColor(env, pixelFormat, out);
}

constexpr fl::u8 kRgb8     = static_cast<fl::u8>(PixelFormat::Rgb8);
constexpr fl::u8 kRgba8    = static_cast<fl::u8>(PixelFormat::Rgba8);
constexpr fl::u8 kRgb565Le = static_cast<fl::u8>(PixelFormat::Rgb565Le);
constexpr fl::u8 kGray8    = static_cast<fl::u8>(PixelFormat::Gray8);
constexpr fl::u8 kRgbw8    = static_cast<fl::u8>(PixelFormat::Rgbw8);
constexpr fl::u8 kRgb16Lin = static_cast<fl::u8>(PixelFormat::Rgb16Linear);

// The canonical declaration a conforming rgb8 producer writes.
const char* const kCanonical =
    "{\"video\":{\"color\":{\"primaries\":\"bt709\",\"transfer\":\"srgb\","
    "\"matrix\":\"rgb\",\"range\":\"full\"}}}";

}  // namespace

// ============================================================================
// Pixel format: rgb16_linear joins the v1 enum
// ============================================================================

FL_TEST_CASE("FLED_COLOR - rgb16_linear is 6 bytes per LED") {
    FL_CHECK_EQ(fl::fled::bytesPerLed(kRgb16Lin), 6);
    // 0x06 and up remain reserved / rejected.
    FL_CHECK_EQ(fl::fled::bytesPerLed(static_cast<fl::u8>(0x06)), 0);
    FL_CHECK_EQ(fl::fled::bytesPerLed(static_cast<fl::u8>(0xff)), 0);
}

// ============================================================================
// Default tuple + absent metadata
// ============================================================================

FL_TEST_CASE("FLED_COLOR - absent video.color resolves to the default tuple") {
    VideoColor c;
    FL_CHECK(resolve("{}", kRgb8, &c) == ColorStatus::Ok);
    FL_CHECK(c.primaries == ColorPrimaries::Bt709);
    FL_CHECK(c.transfer  == ColorTransfer::Srgb);
    FL_CHECK(c.matrix    == ColorMatrix::Rgb);
    FL_CHECK(c.range     == ColorRange::Full);
    // The default is a compatibility interpretation, not an author's claim.
    FL_CHECK(c.declared == false);
}

FL_TEST_CASE("FLED_COLOR - a video block without color still defaults") {
    VideoColor c;
    FL_CHECK(resolve("{\"video\":{\"fps\":30}}", kRgb8, &c) == ColorStatus::Ok);
    FL_CHECK(c.transfer == ColorTransfer::Srgb);
    FL_CHECK(c.declared == false);
}

FL_TEST_CASE("FLED_COLOR - the whole display-encoded family shares the tuple") {
    const fl::u8 formats[3] = {kRgb8, kRgba8, kRgb565Le};
    for (fl::size i = 0; i < 3; ++i) {
        VideoColor c;
        FL_CHECK(resolve("{}", formats[i], &c) == ColorStatus::Ok);
        FL_CHECK(c.transfer == ColorTransfer::Srgb);
        FL_CHECK(c.primaries == ColorPrimaries::Bt709);
    }
}

FL_TEST_CASE("FLED_COLOR - canonical declaration round-trips as declared") {
    VideoColor c;
    FL_CHECK(resolve(kCanonical, kRgb8, &c) == ColorStatus::Ok);
    FL_CHECK(c.primaries == ColorPrimaries::Bt709);
    FL_CHECK(c.transfer  == ColorTransfer::Srgb);
    FL_CHECK(c.matrix    == ColorMatrix::Rgb);
    FL_CHECK(c.range     == ColorRange::Full);
    FL_CHECK(c.declared == true);
}

// ============================================================================
// Key inheritance (scoped to formats with a default tuple)
// ============================================================================

FL_TEST_CASE("FLED_COLOR - missing keys inherit from the default tuple") {
    VideoColor c;
    const char* env =
        "{\"video\":{\"color\":{\"primaries\":\"display-p3\"}}}";
    FL_CHECK(resolve(env, kRgb8, &c) == ColorStatus::Ok);
    FL_CHECK(c.primaries == ColorPrimaries::DisplayP3);  // explicit
    FL_CHECK(c.transfer  == ColorTransfer::Srgb);        // inherited
    FL_CHECK(c.matrix    == ColorMatrix::Rgb);           // inherited
    FL_CHECK(c.range     == ColorRange::Full);           // inherited
    FL_CHECK(c.declared == true);
}

FL_TEST_CASE("FLED_COLOR - bt2020 primaries are recognized") {
    VideoColor c;
    FL_CHECK(resolve("{\"video\":{\"color\":{\"primaries\":\"bt2020\"}}}",
                     kRgb8, &c) == ColorStatus::Ok);
    FL_CHECK(c.primaries == ColorPrimaries::Bt2020);
}

FL_TEST_CASE("FLED_COLOR - bt709 transfer is distinct from srgb") {
    VideoColor c;
    FL_CHECK(resolve("{\"video\":{\"color\":{\"transfer\":\"bt709\"}}}",
                     kRgb8, &c) == ColorStatus::Ok);
    FL_CHECK(c.transfer == ColorTransfer::Bt709);
}

// ============================================================================
// Rule 2/3: transfer must agree with what the payload bytes hold
// ============================================================================

FL_TEST_CASE("FLED_COLOR - rgb8 rejects a linear transfer") {
    // "A producer must not put linear-light samples in rgb8."
    VideoColor c;
    FL_CHECK(resolve("{\"video\":{\"color\":{\"transfer\":\"linear\"}}}",
                     kRgb8, &c) == ColorStatus::TransferConflictsWithFormat);
}

FL_TEST_CASE("FLED_COLOR - reserved HDR transfers are rejected in v1") {
    VideoColor c;
    FL_CHECK(resolve("{\"video\":{\"color\":{\"transfer\":\"pq\"}}}",
                     kRgb8, &c) == ColorStatus::TransferConflictsWithFormat);
    FL_CHECK(resolve("{\"video\":{\"color\":{\"transfer\":\"hlg\"}}}",
                     kRgb8, &c) == ColorStatus::TransferConflictsWithFormat);
    // ... on the linear format too - no v1 payload can carry them.
    FL_CHECK(resolve("{\"video\":{\"color\":{\"transfer\":\"pq\"}}}",
                     kRgb16Lin, &c) == ColorStatus::TransferConflictsWithFormat);
}

FL_TEST_CASE("FLED_COLOR - rgb16_linear defaults to a linear transfer") {
    VideoColor c;
    FL_CHECK(resolve("{}", kRgb16Lin, &c) == ColorStatus::Ok);
    FL_CHECK(c.transfer  == ColorTransfer::Linear);
    FL_CHECK(c.primaries == ColorPrimaries::Bt709);
    FL_CHECK(c.range     == ColorRange::Full);
}

FL_TEST_CASE("FLED_COLOR - rgb16_linear accepts only a linear transfer") {
    VideoColor c;
    FL_CHECK(resolve("{\"video\":{\"color\":{\"transfer\":\"linear\"}}}",
                     kRgb16Lin, &c) == ColorStatus::Ok);
    FL_CHECK(c.transfer == ColorTransfer::Linear);
    FL_CHECK(resolve("{\"video\":{\"color\":{\"transfer\":\"srgb\"}}}",
                     kRgb16Lin, &c) == ColorStatus::TransferConflictsWithFormat);
}

// ============================================================================
// Rules 4/5: reserved range and matrix values
// ============================================================================

FL_TEST_CASE("FLED_COLOR - limited range is reserved and rejected") {
    VideoColor c;
    FL_CHECK(resolve("{\"video\":{\"color\":{\"range\":\"limited\"}}}",
                     kRgb8, &c) == ColorStatus::LimitedRangeUnsupported);
}

FL_TEST_CASE("FLED_COLOR - non-rgb matrix values are reserved and rejected") {
    VideoColor c;
    // A YCbCr payload needs a pixel format that does not exist yet.
    FL_CHECK(resolve("{\"video\":{\"color\":{\"matrix\":\"bt709\"}}}",
                     kRgb8, &c) == ColorStatus::MatrixUnsupported);
    FL_CHECK(resolve("{\"video\":{\"color\":{\"matrix\":\"bt2020ncl\"}}}",
                     kRgb8, &c) == ColorStatus::MatrixUnsupported);
}

// ============================================================================
// Rule 1: unrecognized values are rejected, never silently defaulted
// ============================================================================

FL_TEST_CASE("FLED_COLOR - an explicit null color is treated as absent") {
    // Omitting the key and setting it to null must not have opposite
    // outcomes; many serializers emit null for an unset optional.
    VideoColor c;
    FL_CHECK(resolve("{\"video\":{\"color\":null}}", kRgb8, &c) == ColorStatus::Ok);
    FL_CHECK(c.declared == false);
    FL_CHECK(c.transfer == ColorTransfer::Srgb);
}

FL_TEST_CASE("FLED_COLOR - a typo'd matrix reads as unknown, not reserved") {
    VideoColor c;
    // Reserved YCbCr coefficient names stay "reserved"...
    FL_CHECK(resolve("{\"video\":{\"color\":{\"matrix\":\"ycbcr\"}}}",
                     kRgb8, &c) == ColorStatus::MatrixUnsupported);
    // ...but a typo is unrecognized, so the author is not sent to the spec.
    FL_CHECK(resolve("{\"video\":{\"color\":{\"matrix\":\"rbg\"}}}",
                     kRgb8, &c) == ColorStatus::UnknownMatrix);
}

FL_TEST_CASE("FLED_COLOR - unknown values are rejected per field") {
    VideoColor c;
    FL_CHECK(resolve("{\"video\":{\"color\":{\"primaries\":\"rec2100\"}}}",
                     kRgb8, &c) == ColorStatus::UnknownPrimaries);
    FL_CHECK(resolve("{\"video\":{\"color\":{\"transfer\":\"gamma22\"}}}",
                     kRgb8, &c) == ColorStatus::UnknownTransfer);
    FL_CHECK(resolve("{\"video\":{\"color\":{\"range\":\"studio\"}}}",
                     kRgb8, &c) == ColorStatus::UnknownRange);
}

FL_TEST_CASE("FLED_COLOR - transfer \"none\" is not a valid value") {
    // The spec forbids "none" explicitly: it is ambiguous. Linear-light
    // producers must say "linear" and use a format that permits it.
    VideoColor c;
    FL_CHECK(resolve("{\"video\":{\"color\":{\"transfer\":\"none\"}}}",
                     kRgb8, &c) == ColorStatus::UnknownTransfer);
}

FL_TEST_CASE("FLED_COLOR - non-string scalars are rejected, not coerced") {
    VideoColor c;
    FL_CHECK(resolve("{\"video\":{\"color\":{\"transfer\":42}}}",
                     kRgb8, &c) == ColorStatus::UnknownTransfer);
    FL_CHECK(resolve("{\"video\":{\"color\":{\"matrix\":true}}}",
                     kRgb8, &c) == ColorStatus::UnknownMatrix);
    FL_CHECK(resolve("{\"video\":{\"color\":{\"range\":null}}}",
                     kRgb8, &c) == ColorStatus::UnknownRange);
}

FL_TEST_CASE("FLED_COLOR - video.color must be an object") {
    VideoColor c;
    FL_CHECK(resolve("{\"video\":{\"color\":\"bt709\"}}", kRgb8, &c) ==
             ColorStatus::NotAnObject);
    FL_CHECK(resolve("{\"video\":{\"color\":[1,2]}}", kRgb8, &c) ==
             ColorStatus::NotAnObject);
}

// ============================================================================
// Rule 6: gray8 / rgbw8 have no default tuple - all or nothing
// ============================================================================

FL_TEST_CASE("FLED_COLOR - gray8 and rgbw8 define no default tuple") {
    FL_CHECK(fl::fled::pixelFormatHasDefaultTuple(kRgb8));
    FL_CHECK(fl::fled::pixelFormatHasDefaultTuple(kRgb16Lin));
    FL_CHECK(!fl::fled::pixelFormatHasDefaultTuple(kGray8));
    FL_CHECK(!fl::fled::pixelFormatHasDefaultTuple(kRgbw8));

    VideoColor c;
    FL_CHECK(resolve("{}", kGray8, &c) == ColorStatus::NoDefaultTuple);
    FL_CHECK(resolve("{}", kRgbw8, &c) == ColorStatus::NoDefaultTuple);
}

FL_TEST_CASE("FLED_COLOR - a partial declaration cannot inherit without a tuple") {
    // This is the guard that keeps a future YCbCr format from silently
    // inheriting matrix: rgb.
    VideoColor c;
    FL_CHECK(resolve("{\"video\":{\"color\":{\"primaries\":\"bt709\"}}}",
                     kRgbw8, &c) == ColorStatus::IncompleteForFormat);
    FL_CHECK(resolve("{\"video\":{\"color\":{\"primaries\":\"bt709\","
                     "\"transfer\":\"srgb\",\"matrix\":\"rgb\"}}}",
                     kGray8, &c) == ColorStatus::IncompleteForFormat);
}

FL_TEST_CASE("FLED_COLOR - a complete declaration is accepted without a tuple") {
    VideoColor c;
    FL_CHECK(resolve(kCanonical, kRgbw8, &c) == ColorStatus::Ok);
    FL_CHECK(c.transfer == ColorTransfer::Srgb);
    FL_CHECK(c.declared == true);
}

// ============================================================================
// Rule 7: custom primaries
// ============================================================================

FL_TEST_CASE("FLED_COLOR - custom primaries carry CIE xy pairs") {
    VideoColor c;
    const char* env =
        "{\"video\":{\"color\":{\"primaries\":{"
        "\"red\":[0.640,0.330],\"green\":[0.300,0.600],"
        "\"blue\":[0.150,0.060],\"white\":[0.3127,0.3290]}}}}";
    FL_CHECK(resolve(env, kRgb8, &c) == ColorStatus::Ok);
    FL_CHECK(c.primaries == ColorPrimaries::Custom);
    FL_CHECK(c.customPrimaries[0] > 0.63f);
    FL_CHECK(c.customPrimaries[0] < 0.65f);
    FL_CHECK(c.customPrimaries[7] > 0.32f);
    FL_CHECK(c.customPrimaries[7] < 0.34f);
    // Unstated keys still inherit.
    FL_CHECK(c.transfer == ColorTransfer::Srgb);
}

FL_TEST_CASE("FLED_COLOR - malformed custom primaries are rejected") {
    VideoColor c;
    // Missing white.
    FL_CHECK(resolve("{\"video\":{\"color\":{\"primaries\":{"
                     "\"red\":[0.64,0.33],\"green\":[0.3,0.6],"
                     "\"blue\":[0.15,0.06]}}}}",
                     kRgb8, &c) == ColorStatus::MalformedCustomPrimaries);
    // A one-element pair is not an xy.
    FL_CHECK(resolve("{\"video\":{\"color\":{\"primaries\":{"
                     "\"red\":[0.64],\"green\":[0.3,0.6],"
                     "\"blue\":[0.15,0.06],\"white\":[0.31,0.33]}}}}",
                     kRgb8, &c) == ColorStatus::MalformedCustomPrimaries);
    // A string where a pair belongs.
    FL_CHECK(resolve("{\"video\":{\"color\":{\"primaries\":{"
                     "\"red\":\"0.64,0.33\",\"green\":[0.3,0.6],"
                     "\"blue\":[0.15,0.06],\"white\":[0.31,0.33]}}}}",
                     kRgb8, &c) == ColorStatus::MalformedCustomPrimaries);
}

// ============================================================================
// Diagnostics
// ============================================================================

FL_TEST_CASE("FLED_COLOR - every status has a non-empty message") {
    const ColorStatus all[] = {
        ColorStatus::Ok,
        ColorStatus::NullOutput,
        ColorStatus::NoDefaultTuple,
        ColorStatus::NotAnObject,
        ColorStatus::UnknownPrimaries,
        ColorStatus::UnknownTransfer,
        ColorStatus::UnknownMatrix,
        ColorStatus::UnknownRange,
        ColorStatus::TransferConflictsWithFormat,
        ColorStatus::LimitedRangeUnsupported,
        ColorStatus::MatrixUnsupported,
        ColorStatus::IncompleteForFormat,
        ColorStatus::MalformedCustomPrimaries,
    };
    for (fl::size i = 0; i < sizeof(all) / sizeof(all[0]); ++i) {
        const char* msg = fl::fled::colorStatusMessage(all[i]);
        FL_CHECK(msg != nullptr);
        FL_CHECK(msg[0] != '\0');
    }
}

// ============================================================================
// Fled::videoColor() - the accessor over a real bundle
// ============================================================================

namespace {

// Hand-assemble a v1 .fled bundle (see FLED_FORMAT.md layout table).
fl::vector<fl::u8> buildBundle(fl::u8 pixelFormat, const char* envelope,
                               fl::size envelopeLen) {
    const fl::u32 jsonLen = static_cast<fl::u32>(envelopeLen);
    fl::vector<fl::u8> out;
    out.resize(12 + envelopeLen);
    out[0] = 'F'; out[1] = 'L'; out[2] = 'E'; out[3] = 'D';
    out[4] = 1;
    out[5] = pixelFormat;
    out[6] = 0; out[7] = 0;
    out[8]  = static_cast<fl::u8>(jsonLen & 0xff);
    out[9]  = static_cast<fl::u8>((jsonLen >> 8) & 0xff);
    out[10] = static_cast<fl::u8>((jsonLen >> 16) & 0xff);
    out[11] = static_cast<fl::u8>((jsonLen >> 24) & 0xff);
    for (fl::size i = 0; i < envelopeLen; ++i) {
        out[12 + i] = static_cast<fl::u8>(envelope[i]);
    }
    return out;
}

}  // namespace

FL_TEST_CASE("FLED_COLOR - videoColor reads a declared tuple off a bundle") {
    fl::vector<fl::u8> buf = buildBundle(0x00, kCanonical,
                                          fl::strlen(kCanonical));
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));

    VideoColor c;
    FL_CHECK(f.videoColor(&c) == ColorStatus::Ok);
    FL_CHECK(c.declared == true);
    FL_CHECK(c.transfer == ColorTransfer::Srgb);
}

FL_TEST_CASE("FLED_COLOR - videoColor honors the header's pixel_format") {
    // Same envelope, different header byte: rgb16_linear rejects srgb.
    fl::vector<fl::u8> buf = buildBundle(0x05, kCanonical,
                                          fl::strlen(kCanonical));
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    FL_CHECK_EQ(f.pixelFormat(), 0x05);

    VideoColor c;
    FL_CHECK(f.videoColor(&c) == ColorStatus::TransferConflictsWithFormat);
}

FL_TEST_CASE("FLED_COLOR - a legacy bundle with no color still resolves") {
    const char env[] = "{\"map\":{\"a\":{\"x\":[0],\"y\":[0]}}}";
    fl::vector<fl::u8> buf = buildBundle(0x00, env, sizeof(env) - 1);
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));

    VideoColor c;
    FL_CHECK(f.videoColor(&c) == ColorStatus::Ok);
    FL_CHECK(c.declared == false);
    FL_CHECK(c.transfer == ColorTransfer::Srgb);
}

// ============================================================================
// Cross-repo conformance vectors
//
// These two envelopes are the VERBATIM output of ledmapper's producer
// (src/moviemaker/recording.ts embedFps() -> buildVideoColor(format)), captured
// from a real run. FastLED is the consumer half of this contract, so what the
// producer actually emits must resolve here without complaint. If a future
// ledmapper change alters the emitted shape, this is the test that notices.
// ============================================================================

FL_TEST_CASE("FLED_COLOR - accepts ledmapper's emitted rgb8 envelope verbatim") {
    const char* env =
        "{\"map\":{\"a\":{\"x\":[0],\"y\":[0]}},\"video\":{\"fps\":60,\"color\":"
        "{\"primaries\":\"bt709\",\"transfer\":\"srgb\",\"matrix\":\"rgb\","
        "\"range\":\"full\"}}}";
    fl::vector<fl::u8> buf = buildBundle(0x00, env, fl::strlen(env));
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));

    VideoColor c;
    FL_CHECK(f.videoColor(&c) == ColorStatus::Ok);
    FL_CHECK(c.declared == true);
    FL_CHECK(c.primaries == ColorPrimaries::Bt709);
    FL_CHECK(c.transfer  == ColorTransfer::Srgb);
    FL_CHECK(c.matrix    == ColorMatrix::Rgb);
    FL_CHECK(c.range     == ColorRange::Full);
    // The rest of the envelope still parses normally alongside it.
    FL_CHECK(f.videoFps() == 60.0f);
}

FL_TEST_CASE("FLED_COLOR - accepts ledmapper's emitted rgb16_linear envelope") {
    const char* env =
        "{\"map\":{\"a\":{\"x\":[0],\"y\":[0]}},\"video\":{\"fps\":60,\"color\":"
        "{\"primaries\":\"bt709\",\"transfer\":\"linear\",\"matrix\":\"rgb\","
        "\"range\":\"full\"}}}";
    fl::vector<fl::u8> buf = buildBundle(0x05, env, fl::strlen(env));
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    FL_CHECK_EQ(f.bytesPerLed(), 6);

    VideoColor c;
    FL_CHECK(f.videoColor(&c) == ColorStatus::Ok);
    FL_CHECK(c.declared == true);
    FL_CHECK(c.transfer == ColorTransfer::Linear);
}

FL_TEST_CASE("FLED_COLOR - videoColor on a null Fled yields the default tuple") {
    Fled null;
    VideoColor c;
    FL_CHECK(null.videoColor(&c) == ColorStatus::Ok);
    FL_CHECK(c.declared == false);
    FL_CHECK(c.transfer == ColorTransfer::Srgb);
    FL_CHECK(null.videoColor(nullptr) == ColorStatus::NullOutput);
}

}  // FL_TEST_FILE
