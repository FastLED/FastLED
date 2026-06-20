// Unit tests for the CFastLED::load / loadChannels / loadScreenMap
// bundle-load surface (#3311 PR5). PR5 ships the API shape and a
// FledDispatcher singleton; the wasm/micropy back-ends and the
// loadScreenMap broadcast policy land in later PRs. These tests verify
// that the entry points are reachable, accept null/empty input, and
// don't crash. We do NOT register real controllers here - that path is
// covered by the channel-API integration tests.

#include "FastLED.h"
#include "fl/channels/config.h"
#include "fl/fled/fled.h"
#include "fl/fled/fled_dispatch.h"
#include "fl/stl/int.h"
#include "fl/stl/move.h"
#include "fl/stl/vector.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

// Build a minimal valid v1 .fled buffer wrapping an envelope-only bundle
// (no payload). Same shape as buildBundleFromEnvelope() in fled.cpp.
fl::vector<fl::u8> buildBundle(const char *envelope, fl::size envelopeLen) {
    const fl::u32 jsonLen = static_cast<fl::u32>(envelopeLen);
    fl::vector<fl::u8> out;
    out.resize(12 + envelopeLen);
    out[0] = 'F'; out[1] = 'L'; out[2] = 'E'; out[3] = 'D';
    out[4] = 1;        // version
    out[5] = 0x00;     // pixel_format = rgb8
    out[6] = 0; out[7] = 0;  // reserved
    out[8]  = static_cast<fl::u8>(jsonLen & 0xff);
    out[9]  = static_cast<fl::u8>((jsonLen >> 8) & 0xff);
    out[10] = static_cast<fl::u8>((jsonLen >> 16) & 0xff);
    out[11] = static_cast<fl::u8>((jsonLen >> 24) & 0xff);
    for (fl::size i = 0; i < envelopeLen; ++i) {
        out[12 + i] = static_cast<fl::u8>(envelope[i]);
    }
    return out;
}

} // namespace

FL_TEST_CASE("CFastLED::load - null Fled is a no-op") {
    // operator bool == false; load() must early-return without touching
    // any controller list or broadcasting any event.
    Fled null;
    FL_CHECK_FALSE(static_cast<bool>(null));
    FastLED.load(null);  // must not crash.
}

FL_TEST_CASE("CFastLED::loadChannels - null Fled is a no-op") {
    Fled null;
    FastLED.loadChannels(null);  // must not crash.
}

FL_TEST_CASE("CFastLED::loadScreenMap - null Fled is a no-op") {
    Fled null;
    FastLED.loadScreenMap(null);  // must not crash (FL_WARN is fine).
}

FL_TEST_CASE("CFastLED::load - valid bundle without channels/script") {
    // Envelope-only bundle: empty 'map', no 'channels', no 'script'.
    // channels() is a PR-stub returning nullptr (real deserializer lands
    // in a follow-up), and v1 bundles never carry script sections, so
    // load() should complete without dispatching to FledDispatcher and
    // without registering any controllers.
    const char env[] = "{\"map\":{}}";
    fl::vector<fl::u8> buf = buildBundle(env, sizeof(env) - 1);
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    FastLED.load(f);  // must not crash.
}

FL_TEST_CASE("CFastLED::loadChannels - valid bundle with no channels") {
    // Same shape as above: channels() is a stub that returns nullptr,
    // so loadChannels should early-return without registering anything.
    const char env[] = "{\"map\":{}}";
    fl::vector<fl::u8> buf = buildBundle(env, sizeof(env) - 1);
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    FastLED.loadChannels(f);
}

FL_TEST_CASE("CFastLED::loadScreenMap - valid bundle with empty map") {
    // Empty map -> screenMap() returns nullptr -> loadScreenMap is a
    // no-op (no FL_WARN fires).
    const char env[] = "{\"map\":{}}";
    fl::vector<fl::u8> buf = buildBundle(env, sizeof(env) - 1);
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    FastLED.loadScreenMap(f);
}

FL_TEST_CASE("fl::detail::fledDispatcher - singleton accessor is reachable") {
    // Slots default to empty fl::function instances. PR6 will install
    // real loaders via FastLED.enableWasm() / FastLED.enableMicroPy().
    fl::detail::FledDispatcher& d = fl::detail::fledDispatcher();
    FL_CHECK_FALSE(static_cast<bool>(d.wasmLoader));
    FL_CHECK_FALSE(static_cast<bool>(d.microPyLoader));
    // Accessor is stable across calls (same instance).
    fl::detail::FledDispatcher& d2 = fl::detail::fledDispatcher();
    FL_CHECK_EQ(&d, &d2);
}

} // FL_TEST_FILE
