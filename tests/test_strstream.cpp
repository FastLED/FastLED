
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fl/str.h"
#include "fl/strstream.h"
#include "fl/vector.h"
#include "crgb.h"
#include <sstream>

#include "fl/namespace.h"

using namespace fl;

TEST_CASE("StrStream basic operations") {
    SUBCASE("Construction and assignment") {
        StrStream s;
        CHECK(s.str().size() == 0);
        CHECK(s.str().c_str()[0] == '\0');

        StrStream s2("hello");
        CHECK(s2.str().size() == 5);
        CHECK(strcmp(s2.str().c_str(), "hello") == 0);

        StrStream s3 = s2;
        CHECK(s3.str().size() == 5);
        CHECK(strcmp(s3.str().c_str(), "hello") == 0);

        s = "world";
        CHECK(s.str().size() == 5);
        CHECK(strcmp(s.str().c_str(), "world") == 0);
    }

    SUBCASE("Comparison operators") {
        StrStream s1("hello");
        StrStream s2("hello");
        StrStream s3("world");

        CHECK(s1.str() == s2.str());
        CHECK(s1.str() != s3.str());
    }

    SUBCASE("Indexing") {
        StrStream s("hello");
        CHECK(s.str()[0] == 'h');
        CHECK(s.str()[4] == 'o');
        CHECK(s.str()[5] == '\0');  // Null terminator
    }

    SUBCASE("Append") {
        StrStream s("hello");
        s << " world";
        CHECK(s.str().size() == 11);
        CHECK(strcmp(s.str().c_str(), "hello world") == 0);
    }

    SUBCASE("CRGB to StrStream") {
        CRGB c(255, 0, 0);
        StrStream s;
        s << c;
        CHECK(s.str().size() == 13);  // "CRGB(255,0,0)" is 13 chars
        CHECK(strcmp(s.str().c_str(), "CRGB(255,0,0)") == 0);
    }
}
