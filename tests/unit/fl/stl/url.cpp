// ok cpp include
/// @file url.cpp
/// @brief Tests for fl::url parser

#include "test.h"
#include "fl/stl/string_view.h"
#include "fl/stl/url.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("Url - basic http") {
    url u("http://example.com/path");
    FL_CHECK(u.isValid());
    FL_CHECK(u.scheme() == "http");
    FL_CHECK(u.host() == "example.com");
    FL_CHECK(u.path() == "/path");
    FL_CHECK(u.port() == 80);
    FL_CHECK(u.port_str().empty());
    FL_CHECK(u.query().empty());
    FL_CHECK(u.fragment().empty());
    FL_CHECK(u.userinfo().empty());
}

FL_TEST_CASE("Url - full URL with all components") {
    url u("https://user:pass@api.example.com:8080/foo/bar?key=val&a=b#section");
    FL_CHECK(u.isValid());
    FL_CHECK(u.scheme() == "https");
    FL_CHECK(u.userinfo() == "user:pass");
    FL_CHECK(u.host() == "api.example.com");
    FL_CHECK(u.port() == 8080);
    FL_CHECK(u.port_str() == "8080");
    FL_CHECK(u.path() == "/foo/bar");
    FL_CHECK(u.query() == "key=val&a=b");
    FL_CHECK(u.fragment() == "section");
    FL_CHECK(u.authority() == "user:pass@api.example.com:8080");
}

FL_TEST_CASE("Url - default ports") {
    url http("http://h.com/");
    FL_CHECK(http.port() == 80);

    url https("https://h.com/");
    FL_CHECK(https.port() == 443);

    url ws("ws://h.com/");
    FL_CHECK(ws.port() == 80);

    url wss("wss://h.com/");
    FL_CHECK(wss.port() == 443);

    url ftp("ftp://h.com/");
    FL_CHECK(ftp.port() == 21);
}

FL_TEST_CASE("Url - missing components") {
    // No path
    url u1("http://example.com");
    FL_CHECK(u1.isValid());
    FL_CHECK(u1.host() == "example.com");
    FL_CHECK(u1.path().empty());

    // No query/fragment
    url u2("http://example.com/p");
    FL_CHECK(u2.query().empty());
    FL_CHECK(u2.fragment().empty());

    // Fragment only (no query)
    url u3("http://example.com/p#frag");
    FL_CHECK(u3.path() == "/p");
    FL_CHECK(u3.query().empty());
    FL_CHECK(u3.fragment() == "frag");

    // Query only (no fragment)
    url u4("http://example.com/p?q=1");
    FL_CHECK(u4.query() == "q=1");
    FL_CHECK(u4.fragment().empty());
}

FL_TEST_CASE("Url - invalid URLs") {
    url empty("");
    FL_CHECK_FALSE(empty.isValid());
    FL_CHECK(empty.scheme().empty());

    url def;
    FL_CHECK_FALSE(def.isValid());
}

FL_TEST_CASE("Url - no scheme assumes https") {
    url noScheme("example.com/path");
    FL_CHECK(noScheme.isValid());
    FL_CHECK(noScheme.wasRepaired());
    FL_CHECK(noScheme.scheme() == "https");
    FL_CHECK(noScheme.host() == "example.com");
    FL_CHECK(noScheme.path() == "/path");
    FL_CHECK(noScheme.port() == 443);
}

FL_TEST_CASE("Url - no scheme audio file") {
    url u("example.com/test.mp3");
    FL_CHECK(u.isValid());
    FL_CHECK(u.wasRepaired());
    FL_CHECK(u.scheme() == "https");
    FL_CHECK(u.host() == "example.com");
    FL_CHECK(u.path() == "/test.mp3");
    FL_CHECK(u.port() == 443);
}

FL_TEST_CASE("Url - copy semantics") {
    url orig("https://example.com:9090/api?x=1#top");
    url copy(orig);
    FL_CHECK(copy.isValid());
    FL_CHECK(copy == orig);
    FL_CHECK(copy.host() == "example.com");
    FL_CHECK(copy.port() == 9090);
    FL_CHECK(copy.query() == "x=1");
    FL_CHECK(copy.fragment() == "top");
}

FL_TEST_CASE("Url - real-world audio URL") {
    url u("https://hebbkx1anhila5yf.public.blob.vercel-storage.com/"
          "jingle-bells-8bit-lullaby-116015.mp3");
    FL_CHECK(u.isValid());
    FL_CHECK(u.scheme() == "https");
    FL_CHECK(u.host() ==
             "hebbkx1anhila5yf.public.blob.vercel-storage.com");
    FL_CHECK(u.port() == 443);
    FL_CHECK(u.path() == "/jingle-bells-8bit-lullaby-116015.mp3");
}

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

FL_TEST_CASE("fl::parse_lnk_with_metadata") {
    FL_SUBCASE("URL only, no metadata") {
        LnkMetadata m = parse_lnk_with_metadata(
            string_view("https://example.com/a.mp3\n"));
        FL_CHECK(m.isValid());
        FL_CHECK_EQ(m.primary.host(), string_view("example.com"));
        FL_CHECK(m.sha256.empty());
        FL_CHECK_FALSE(m.fallback.isValid());
    }

    FL_SUBCASE("URL + sha256 + fallback") {
        LnkMetadata m = parse_lnk_with_metadata(string_view(
            "https://example.com/b.mp3\n"
            "sha256=deadbeef1234\n"
            "fallback=https://mirror.example.com/b.mp3\n"));
        FL_CHECK(m.isValid());
        FL_CHECK_EQ(m.primary.host(), string_view("example.com"));
        FL_CHECK_EQ(m.sha256, fl::string("deadbeef1234"));
        FL_CHECK(m.fallback.isValid());
        FL_CHECK_EQ(m.fallback.host(), string_view("mirror.example.com"));
    }

    FL_SUBCASE("unknown metadata keys are ignored (forward-compat)") {
        LnkMetadata m = parse_lnk_with_metadata(string_view(
            "https://example.com/c.mp3\n"
            "content-type=audio/mpeg\n"
            "future-key=some value\n"
            "sha256=abc\n"));
        FL_CHECK(m.isValid());
        FL_CHECK_EQ(m.sha256, fl::string("abc"));
        FL_CHECK_FALSE(m.fallback.isValid());
    }

    FL_SUBCASE("comments before and between metadata are skipped") {
        LnkMetadata m = parse_lnk_with_metadata(string_view(
            "# comment first\n"
            "https://example.com/d.mp3\n"
            "# between metadata\n"
            "sha256=1234\n"));
        FL_CHECK(m.isValid());
        FL_CHECK_EQ(m.sha256, fl::string("1234"));
    }

    FL_SUBCASE("empty input yields invalid metadata") {
        LnkMetadata m = parse_lnk_with_metadata(string_view(""));
        FL_CHECK_FALSE(m.isValid());
        FL_CHECK_FALSE(bool(m));
    }
}

} // FL_TEST_FILE
