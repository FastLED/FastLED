// Test Coverage: Compile-time type compatibility verification
// This test suite ensures that FakeStrStream and StrStream both support
// the exact same types using template-based compile-time checks.
//
// Purpose: Verify that code can be compiled to work with either StrStream or
// FakeStrStream with zero modifications, ensuring API compatibility.

#include "test.h"
#include "fl/str.h"
#include "fl/strstream.h"
#include "fl/vector.h"
#include "crgb.h"

//-----------------------------------------------------------------------------
// Template-based type compatibility tests
// These templates ensure both StrStream and FakeStrStream accept the same types
//-----------------------------------------------------------------------------

// Generic stream operation template - works with any stream type
template<typename StreamType>
void test_string_types(StreamType& s) {
    s << "hello" << " " << "world";
}

template<typename StreamType>
void test_char_types(StreamType& s) {
    s << char('A');
    s << (signed char)(-10);
    s << (unsigned char)(200);
}

template<typename StreamType>
void test_fundamental_int_types(StreamType& s) {
    s << (short)(-1000);
    s << (unsigned short)(50000);
    s << (int)(-100000);
    s << (unsigned int)(4000000);
    s << (long)(-1000000L);
    s << (unsigned long)(4000000000UL);
    s << (long long)(-1000000000LL);
    s << (unsigned long long)(1000000000ULL);
}

template<typename StreamType>
void test_fl_int_types(StreamType& s) {
    s << fl::i8(-10);
    s << fl::u8(200);
    s << fl::i16(-1000);
    s << fl::u16(50000);
    s << fl::i32(-100000);
    s << fl::u32(4000000);
    s << fl::i64(-1000000000LL);
    s << fl::u64(1000000000ULL);
    s << fl::size(12345);
    s << fl::uint(999999);
}

template<typename StreamType>
void test_floating_point_types(StreamType& s) {
    s << 3.14f;
    s << 2.71828;
}

template<typename StreamType>
void test_bool_type(StreamType& s) {
    s << true;
    s << false;
}

template<typename StreamType>
void test_crgb_type(StreamType& s) {
    CRGB rgb(255, 0, 0);
    s << rgb;
}

template<typename StreamType>
void test_cpp_string_type(StreamType& s) {
    fl::string str("test");
    s << str;
}

template<typename StreamType>
void test_mixed_types(StreamType& s) {
    s << "Value: " << fl::i32(42) << " Flag: " << true << " Float: " << 3.14f;
    s << " Char: " << char('X') << " Int: " << (short)100;
}

//-----------------------------------------------------------------------------
// Test Cases: Verify both StrStream and FakeStrStream work with the templates
//-----------------------------------------------------------------------------

TEST_CASE("StrStream and FakeStrStream - string types compatibility") {
    SUBCASE("StrStream handles strings") {
        fl::StrStream stream;
        test_string_types(stream);
        CHECK(stream.str().size() > 0);
    }

    SUBCASE("FakeStrStream handles strings") {
        fl::FakeStrStream stream;
        test_string_types(stream);
        // FakeStrStream is a no-op, but must compile
        CHECK(true);
    }
}

TEST_CASE("StrStream and FakeStrStream - character types compatibility") {
    SUBCASE("StrStream handles character types") {
        fl::StrStream stream;
        test_char_types(stream);
        CHECK(stream.str().size() > 0);
    }

    SUBCASE("FakeStrStream handles character types") {
        fl::FakeStrStream stream;
        test_char_types(stream);
        // Must compile without errors
        CHECK(true);
    }
}

TEST_CASE("StrStream and FakeStrStream - fundamental integer types compatibility") {
    SUBCASE("StrStream handles all fundamental integer types") {
        fl::StrStream stream;
        test_fundamental_int_types(stream);
        CHECK(stream.str().size() > 0);
    }

    SUBCASE("FakeStrStream handles all fundamental integer types") {
        fl::FakeStrStream stream;
        test_fundamental_int_types(stream);
        // Must compile without errors
        CHECK(true);
    }
}

TEST_CASE("StrStream and FakeStrStream - fl:: integer types compatibility") {
    SUBCASE("StrStream handles all fl:: integer types") {
        fl::StrStream stream;
        test_fl_int_types(stream);
        CHECK(stream.str().size() > 0);
    }

    SUBCASE("FakeStrStream handles all fl:: integer types") {
        fl::FakeStrStream stream;
        test_fl_int_types(stream);
        // Must compile without errors
        CHECK(true);
    }
}

TEST_CASE("StrStream and FakeStrStream - floating point types compatibility") {
    SUBCASE("StrStream handles floating point types") {
        fl::StrStream stream;
        test_floating_point_types(stream);
        CHECK(stream.str().size() > 0);
    }

    SUBCASE("FakeStrStream handles floating point types") {
        fl::FakeStrStream stream;
        test_floating_point_types(stream);
        // Must compile without errors
        CHECK(true);
    }
}

TEST_CASE("StrStream and FakeStrStream - bool type compatibility") {
    SUBCASE("StrStream handles bool") {
        fl::StrStream stream;
        test_bool_type(stream);
        CHECK(stream.str().size() > 0);
    }

    SUBCASE("FakeStrStream handles bool") {
        fl::FakeStrStream stream;
        test_bool_type(stream);
        // Must compile without errors
        CHECK(true);
    }
}

TEST_CASE("StrStream and FakeStrStream - CRGB type compatibility") {
    SUBCASE("StrStream handles CRGB") {
        fl::StrStream stream;
        test_crgb_type(stream);
        CHECK(stream.str().size() > 0);
    }

    SUBCASE("FakeStrStream handles CRGB") {
        fl::FakeStrStream stream;
        test_crgb_type(stream);
        // Must compile without errors
        CHECK(true);
    }
}

TEST_CASE("StrStream and FakeStrStream - fl::string type compatibility") {
    SUBCASE("StrStream handles fl::string") {
        fl::StrStream stream;
        test_cpp_string_type(stream);
        CHECK(stream.str().size() > 0);
    }

    SUBCASE("FakeStrStream handles fl::string") {
        fl::FakeStrStream stream;
        test_cpp_string_type(stream);
        // Must compile without errors
        CHECK(true);
    }
}

TEST_CASE("StrStream and FakeStrStream - mixed types compatibility") {
    SUBCASE("StrStream handles mixed types") {
        fl::StrStream stream;
        test_mixed_types(stream);
        CHECK(stream.str().size() > 0);
    }

    SUBCASE("FakeStrStream handles mixed types") {
        fl::FakeStrStream stream;
        test_mixed_types(stream);
        // Must compile without errors
        CHECK(true);
    }
}

//-----------------------------------------------------------------------------
// Template instantiation verification tests
// These ensure the template functions work correctly with both types
//-----------------------------------------------------------------------------

TEST_CASE("Template function instantiation with both stream types") {
    SUBCASE("StrStream instantiation") {
        fl::StrStream s;
        // This instantiation verifies the template compiles with StrStream
        test_string_types(s);
        test_char_types(s);
        test_fundamental_int_types(s);
        test_fl_int_types(s);
        test_floating_point_types(s);
        test_bool_type(s);
        test_crgb_type(s);
        test_cpp_string_type(s);
        test_mixed_types(s);
        CHECK(s.str().size() > 0);
    }

    SUBCASE("FakeStrStream instantiation") {
        fl::FakeStrStream s;
        // This instantiation verifies the template compiles with FakeStrStream
        test_string_types(s);
        test_char_types(s);
        test_fundamental_int_types(s);
        test_fl_int_types(s);
        test_floating_point_types(s);
        test_bool_type(s);
        test_crgb_type(s);
        test_cpp_string_type(s);
        test_mixed_types(s);
        // FakeStrStream is a no-op, but must compile
        CHECK(true);
    }
}

//-----------------------------------------------------------------------------
// Advanced type tests - references and const qualifiers
//-----------------------------------------------------------------------------

template<typename StreamType>
void test_const_types(StreamType& s) {
    const fl::i32 ci32 = 100;
    const fl::u32 cu32 = 200;
    const char* cstr = "const";
    s << ci32 << cu32 << cstr;
}

template<typename StreamType>
void test_reference_types(StreamType& s) {
    fl::i32 val = 100;
    fl::i32& ref = val;
    const fl::u16& cref = static_cast<const fl::u16&>(fl::u16(200));
    s << ref << cref;
}

TEST_CASE("StrStream and FakeStrStream - const and reference types compatibility") {
    SUBCASE("StrStream handles const and references") {
        fl::StrStream stream;
        test_const_types(stream);
        test_reference_types(stream);
        CHECK(stream.str().size() > 0);
    }

    SUBCASE("FakeStrStream handles const and references") {
        fl::FakeStrStream stream;
        test_const_types(stream);
        test_reference_types(stream);
        // Must compile without errors
        CHECK(true);
    }
}

//-----------------------------------------------------------------------------
// Chaining and compound operations
//-----------------------------------------------------------------------------

template<typename StreamType>
void test_operator_chaining(StreamType& s) {
    s << "Start" << " " << fl::i32(42) << " " << true << " " << 3.14f << " End";
}

TEST_CASE("StrStream and FakeStrStream - operator chaining compatibility") {
    SUBCASE("StrStream supports operator chaining") {
        fl::StrStream stream;
        test_operator_chaining(stream);
        CHECK(stream.str().size() > 0);
    }

    SUBCASE("FakeStrStream supports operator chaining") {
        fl::FakeStrStream stream;
        test_operator_chaining(stream);
        // Must compile without errors
        CHECK(true);
    }
}
