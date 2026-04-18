
// Tests for fl::parse_lnk — the .lnk file format parser that turns
// a `data/track.mp3.lnk` file into a fl::url.

#include "fl/stl/string_view.h"
#include "fl/stl/url.h"
#include "test.h"

using namespace fl;

FL_TEST_CASE("fl::parse_lnk") {
    FL_SUBCASE("plain URL") {
        url u = parse_lnk(string_view("https://example.com/track.mp3"));
        FL_CHECK(u.isValid());
        FL_CHECK_EQ(u.host(), string_view("example.com"));
    }

    FL_SUBCASE("URL with trailing newline") {
        url u = parse_lnk(string_view("https://example.com/track.mp3\n"));
        FL_CHECK(u.isValid());
        FL_CHECK_EQ(u.host(), string_view("example.com"));
    }

    FL_SUBCASE("CRLF line endings") {
        url u = parse_lnk(string_view("https://example.com/track.mp3\r\n"));
        FL_CHECK(u.isValid());
        FL_CHECK_EQ(u.host(), string_view("example.com"));
    }

    FL_SUBCASE("leading blank lines are skipped") {
        url u = parse_lnk(string_view("\n\n\nhttps://example.com/a.mp3\n"));
        FL_CHECK(u.isValid());
        FL_CHECK_EQ(u.host(), string_view("example.com"));
    }

    FL_SUBCASE("# comment lines are skipped") {
        url u = parse_lnk(string_view(
            "# this is a comment\n"
            "# another comment\n"
            "https://example.com/b.mp3\n"));
        FL_CHECK(u.isValid());
        FL_CHECK_EQ(u.host(), string_view("example.com"));
    }

    FL_SUBCASE("leading whitespace before URL is trimmed") {
        url u = parse_lnk(string_view("   \thttps://example.com/c.mp3   \n"));
        FL_CHECK(u.isValid());
        FL_CHECK_EQ(u.host(), string_view("example.com"));
    }

    FL_SUBCASE("trailing metadata lines are ignored") {
        url u = parse_lnk(string_view(
            "https://example.com/d.mp3\n"
            "sha256=abcdef\n"
            "fallback=https://mirror.example.com/d.mp3\n"));
        FL_CHECK(u.isValid());
        FL_CHECK_EQ(u.host(), string_view("example.com"));
    }

    FL_SUBCASE("empty content returns invalid url") {
        url u = parse_lnk(string_view(""));
        FL_CHECK_FALSE(u.isValid());
    }

    FL_SUBCASE("only comments returns invalid url") {
        url u = parse_lnk(string_view(
            "# comment one\n"
            "   # indented comment\n"
            "\n"));
        FL_CHECK_FALSE(u.isValid());
    }

    FL_SUBCASE("indented comment is still a comment") {
        url u = parse_lnk(string_view(
            "    # leading space then hash\n"
            "https://example.com/e.mp3\n"));
        FL_CHECK(u.isValid());
        FL_CHECK_EQ(u.host(), string_view("example.com"));
    }
}
