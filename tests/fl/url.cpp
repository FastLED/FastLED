// ok cpp include
/// @file url.cpp
/// @brief Tests for fl::url parser

#include "test.h"
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

} // FL_TEST_FILE
