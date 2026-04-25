// ok cpp include
/// @file asset.cpp
/// @brief Tests for fl::asset_ref, fl::asset(), and fl::resolve_asset().

#include "fl/asset/asset.h"
#include "fl/stl/string_view.h"
#include "fl/stl/url.h"
#include "test.h"
#include "fl/stl/static_assert.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// -----------------------------------------------------------------------------
// Compile-time: path_has_parent_segment rejects ".." segments but allows
// innocent-looking paths that merely contain dot characters.
// -----------------------------------------------------------------------------

FL_STATIC_ASSERT(!fl::asset_detail::path_has_parent_segment("data/track.mp3"),
              "plain relative path should not be flagged");
FL_STATIC_ASSERT(!fl::asset_detail::path_has_parent_segment("track.mp3"),
              "bare file should not be flagged");
FL_STATIC_ASSERT(!fl::asset_detail::path_has_parent_segment("folder.with.dots/a.mp3"),
              "dots inside segments must not trigger false positive");
FL_STATIC_ASSERT(!fl::asset_detail::path_has_parent_segment("."),
              "single dot is not a parent segment");
FL_STATIC_ASSERT(!fl::asset_detail::path_has_parent_segment(""),
              "empty path is not a parent segment");

FL_STATIC_ASSERT(fl::asset_detail::path_has_parent_segment(".."),
              "bare .. must be rejected");
FL_STATIC_ASSERT(fl::asset_detail::path_has_parent_segment("../foo"),
              "leading .. must be rejected");
FL_STATIC_ASSERT(fl::asset_detail::path_has_parent_segment("foo/.."),
              "trailing .. must be rejected");
FL_STATIC_ASSERT(fl::asset_detail::path_has_parent_segment("data/../foo.mp3"),
              "embedded .. must be rejected");
FL_STATIC_ASSERT(fl::asset_detail::path_has_parent_segment("a/b/../c"),
              "deep embedded .. must be rejected");
// Windows-style separators are treated the same.
FL_STATIC_ASSERT(fl::asset_detail::path_has_parent_segment("data\\..\\foo.mp3"),
              "backslash-style .. must also be rejected");

// FL_ASSET() macro returns a valid handle at compile time when the path is OK.
// The static_assert inside FL_ASSET fires only for ".." paths so this compiles.
FL_TEST_CASE("fl::asset - FL_ASSET macro produces a valid handle") {
    asset_ref r = FL_ASSET("data/track.mp3");
    FL_CHECK(bool(r));
    FL_CHECK_EQ(r.path(), string_view("data/track.mp3"));
    FL_CHECK_EQ(r.size(), string_view("data/track.mp3").size());
}

// The constexpr-callable `fl::asset()` function returns an EMPTY handle when
// the path contains "..", giving a clean runtime miss instead of a crash.
FL_TEST_CASE("fl::asset - runtime path with .. produces empty handle") {
    asset_ref bad = fl::asset("data/../etc/passwd");
    FL_CHECK_FALSE(bool(bad));
    FL_CHECK_EQ(bad.size(), 0u);
}

FL_TEST_CASE("fl::asset - plain path produces non-empty handle") {
    asset_ref r = fl::asset("data/track.mp3");
    FL_CHECK(bool(r));
    FL_CHECK_EQ(r.path(), string_view("data/track.mp3"));
}

// -----------------------------------------------------------------------------
// Runtime resolution: registry always wins.
// -----------------------------------------------------------------------------

FL_TEST_CASE("fl::resolve_asset - registered path returns registered URL") {
    // Register a mapping directly (simulating the generated manifest).
    fl::register_asset(string_view("pipeline/a.mp3"),
                       url("https://example.com/registered-a.mp3"));

    asset_ref r = FL_ASSET("pipeline/a.mp3");
    url resolved = fl::resolve_asset(r);
    FL_CHECK(resolved.isValid());
    FL_CHECK_EQ(resolved.host(), string_view("example.com"));
    FL_CHECK_EQ(resolved.path(), string_view("/registered-a.mp3"));
}

// -----------------------------------------------------------------------------
// Host/stub resolution: local file on disk takes precedence over registry
// miss, and sibling .lnk is a fallback when the raw file is absent.
// -----------------------------------------------------------------------------

FL_TEST_CASE("fl::resolve_asset - host fallback finds local file") {
    // tests/fl/data/test_audio.ogg is checked into the repo; pwd for tests is
    // the project root, so this relative path exists on disk.
    asset_ref r = FL_ASSET("tests/fl/data/test_audio.ogg");
    url resolved = fl::resolve_asset(r);
    FL_CHECK(resolved.isValid());
    // url.repaired() is true because we passed a bare path (no scheme); fl::url
    // prepends https:// automatically. The important part is the string() contains
    // our path.
    FL_CHECK(resolved.string().find("tests/fl/data/test_audio.ogg") != fl::string::npos);
}

FL_TEST_CASE("fl::resolve_asset - host fallback finds sibling .lnk") {
    // examples/AudioUrl/data/track.mp3.lnk exists in the repo; the raw
    // track.mp3 does NOT. Resolution must fall through to the .lnk and return
    // the URL declared inside.
    asset_ref r = FL_ASSET("examples/AudioUrl/data/track.mp3");
    url resolved = fl::resolve_asset(r);
    FL_CHECK(resolved.isValid());
    FL_CHECK_EQ(resolved.host(), string_view("www.soundhelix.com"));
}

FL_TEST_CASE("fl::resolve_asset - unknown path returns invalid url") {
    asset_ref r = FL_ASSET("does/not/exist/anywhere.mp3");
    url resolved = fl::resolve_asset(r);
    FL_CHECK_FALSE(resolved.isValid());
}

FL_TEST_CASE("fl::resolve_asset - empty handle yields invalid url") {
    asset_ref empty;
    url resolved = fl::resolve_asset(empty);
    FL_CHECK_FALSE(resolved.isValid());
}

} // FL_TEST_FILE
