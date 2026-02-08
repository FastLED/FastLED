/// @file format.cpp
/// @brief Unit tests for fl::format() - C++20 std::format style

#include "test.h"
#include "fl/stl/format.h"

using namespace fl;

// =============================================================================
// Basic placeholder tests
// =============================================================================

FL_TEST_CASE("format - no arguments") {
    fl::string result = fl::format("Hello, World!");
    FL_CHECK(result == "Hello, World!");
}

FL_TEST_CASE("format - empty string") {
    fl::string result = fl::format("");
    FL_CHECK(result == "");
}

FL_TEST_CASE("format - single placeholder") {
    fl::string result = fl::format("Hello {}!", "World");
    FL_CHECK(result == "Hello World!");
}

FL_TEST_CASE("format - single integer") {
    fl::string result = fl::format("Value: {}", 42);
    FL_CHECK(result == "Value: 42");
}

FL_TEST_CASE("format - multiple placeholders") {
    fl::string result = fl::format("{} + {} = {}", 2, 3, 5);
    FL_CHECK(result == "2 + 3 = 5");
}

FL_TEST_CASE("format - six arguments") {
    fl::string result = fl::format("{} {} {} {} {} {}", 1, 2, 3, 4, 5, 6);
    FL_CHECK(result == "1 2 3 4 5 6");
}

FL_TEST_CASE("format - eight arguments") {
    fl::string result = fl::format("{}{}{}{}{}{}{}{}", 1, 2, 3, 4, 5, 6, 7, 8);
    FL_CHECK(result == "12345678");
}

// =============================================================================
// Indexed arguments
// =============================================================================

FL_TEST_CASE("format - indexed arguments") {
    fl::string result = fl::format("{1} before {0}", "A", "B");
    FL_CHECK(result == "B before A");
}

FL_TEST_CASE("format - repeated index") {
    fl::string result = fl::format("{0} {0} {0}", "echo");
    FL_CHECK(result == "echo echo echo");
}

FL_TEST_CASE("format - mixed indexed") {
    fl::string result = fl::format("{2} {0} {1}", "A", "B", "C");
    FL_CHECK(result == "C A B");
}

// =============================================================================
// Type specifiers
// =============================================================================

FL_TEST_CASE("format - decimal explicit") {
    fl::string result = fl::format("{:d}", 42);
    FL_CHECK(result == "42");
}

FL_TEST_CASE("format - hexadecimal lowercase") {
    fl::string result = fl::format("{:x}", 255);
    FL_CHECK(result == "ff");
}

FL_TEST_CASE("format - hexadecimal uppercase") {
    fl::string result = fl::format("{:X}", 255);
    FL_CHECK(result == "FF");
}

FL_TEST_CASE("format - binary") {
    fl::string result = fl::format("{:b}", 5);
    FL_CHECK(result == "101");
}

FL_TEST_CASE("format - octal") {
    fl::string result = fl::format("{:o}", 8);
    FL_CHECK(result == "10");
}

FL_TEST_CASE("format - character from int") {
    fl::string result = fl::format("{:c}", 65);
    FL_CHECK(result == "A");
}

// =============================================================================
// Floating point
// =============================================================================

FL_TEST_CASE("format - float default") {
    fl::string result = fl::format("{}", 3.14f);
    FL_CHECK(fl::strstr(result.c_str(), "3.14") != nullptr);
}

FL_TEST_CASE("format - float precision") {
    fl::string result = fl::format("{:.2f}", 3.14159);
    FL_CHECK(result == "3.14");
}

FL_TEST_CASE("format - float zero precision") {
    fl::string result = fl::format("{:.0f}", 3.7);
    FL_CHECK(result == "4");
}

FL_TEST_CASE("format - float high precision") {
    fl::string result = fl::format("{:.4f}", 3.14159265);
    FL_CHECK(result == "3.1416");
}

// =============================================================================
// Width and alignment
// =============================================================================

FL_TEST_CASE("format - width right align default") {
    fl::string result = fl::format("{:10}", 42);
    FL_CHECK(result == "        42");
}

FL_TEST_CASE("format - width left align") {
    fl::string result = fl::format("{:<10}", 42);
    FL_CHECK(result == "42        ");
}

FL_TEST_CASE("format - width right align explicit") {
    fl::string result = fl::format("{:>10}", 42);
    FL_CHECK(result == "        42");
}

FL_TEST_CASE("format - width center align") {
    fl::string result = fl::format("{:^10}", 42);
    FL_CHECK(result == "    42    ");
}

FL_TEST_CASE("format - fill character") {
    fl::string result = fl::format("{:*^10}", 42);
    FL_CHECK(result == "****42****");
}

FL_TEST_CASE("format - fill with dash") {
    fl::string result = fl::format("{:-<10}", 42);
    FL_CHECK(result == "42--------");
}

FL_TEST_CASE("format - zero padding") {
    fl::string result = fl::format("{:05}", 42);
    FL_CHECK(result == "00042");
}

FL_TEST_CASE("format - zero padding negative") {
    fl::string result = fl::format("{:05}", -42);
    FL_CHECK(result == "-0042");
}

// =============================================================================
// Signs and alternate forms
// =============================================================================

FL_TEST_CASE("format - positive sign") {
    fl::string result = fl::format("{:+}", 42);
    FL_CHECK(result == "+42");
}

FL_TEST_CASE("format - space for positive") {
    fl::string result = fl::format("{: }", 42);
    FL_CHECK(result == " 42");
}

FL_TEST_CASE("format - negative number") {
    fl::string result = fl::format("{}", -42);
    FL_CHECK(result == "-42");
}

FL_TEST_CASE("format - alternate hex") {
    fl::string result = fl::format("{:#x}", 255);
    FL_CHECK(result == "0xff");
}

FL_TEST_CASE("format - alternate HEX") {
    fl::string result = fl::format("{:#X}", 255);
    FL_CHECK(result == "0xFF");
}

FL_TEST_CASE("format - alternate binary") {
    fl::string result = fl::format("{:#b}", 5);
    FL_CHECK(result == "0b101");
}

FL_TEST_CASE("format - alternate octal") {
    fl::string result = fl::format("{:#o}", 8);
    FL_CHECK(result == "010");
}

// =============================================================================
// Escaping braces
// =============================================================================

FL_TEST_CASE("format - escaped open brace") {
    fl::string result = fl::format("{{");
    FL_CHECK(result == "{");
}

FL_TEST_CASE("format - escaped close brace") {
    fl::string result = fl::format("}}");
    FL_CHECK(result == "}");
}

FL_TEST_CASE("format - escaped pair") {
    fl::string result = fl::format("{{}}");
    FL_CHECK(result == "{}");
}

FL_TEST_CASE("format - escaped with value") {
    fl::string result = fl::format("{{{}}}", 42);
    FL_CHECK(result == "{42}");
}

// =============================================================================
// String types
// =============================================================================

FL_TEST_CASE("format - const char*") {
    fl::string result = fl::format("Name: {}", "Alice");
    FL_CHECK(result == "Name: Alice");
}

FL_TEST_CASE("format - fl::string") {
    fl::string name = "Bob";
    fl::string result = fl::format("Name: {}", name);
    FL_CHECK(result == "Name: Bob");
}

FL_TEST_CASE("format - string precision") {
    fl::string result = fl::format("{:.5}", "Hello World");
    FL_CHECK(result == "Hello");
}

FL_TEST_CASE("format - string width") {
    fl::string result = fl::format("{:10}", "Hi");
    FL_CHECK(result == "        Hi");
}

FL_TEST_CASE("format - string left align") {
    fl::string result = fl::format("{:<10}", "Hi");
    FL_CHECK(result == "Hi        ");
}

// =============================================================================
// Character type
// =============================================================================

FL_TEST_CASE("format - char") {
    fl::string result = fl::format("{}", 'A');
    FL_CHECK(result == "A");
}

FL_TEST_CASE("format - char as int") {
    fl::string result = fl::format("{:d}", 'A');
    FL_CHECK(result == "65");
}

// =============================================================================
// Integer types
// =============================================================================

FL_TEST_CASE("format - short") {
    short val = 123;
    fl::string result = fl::format("{}", val);
    FL_CHECK(result == "123");
}

FL_TEST_CASE("format - unsigned") {
    unsigned int val = 4294967295u;
    fl::string result = fl::format("{}", val);
    FL_CHECK(result == "4294967295");
}

FL_TEST_CASE("format - long") {
    long val = 1234567890L;
    fl::string result = fl::format("{}", val);
    FL_CHECK(result == "1234567890");
}

FL_TEST_CASE("format - long long") {
    long long val = 9223372036854775807LL;
    fl::string result = fl::format("{}", val);
    FL_CHECK(fl::strstr(result.c_str(), "9223372036854775807") != nullptr);
}

FL_TEST_CASE("format - zero") {
    fl::string result = fl::format("{}", 0);
    FL_CHECK(result == "0");
}

// =============================================================================
// Pointer
// =============================================================================

FL_TEST_CASE("format - pointer") {
    int x = 42;
    fl::string result = fl::format("{}", static_cast<const void*>(&x));
    // Should start with "0x"
    FL_CHECK(result.size() >= 3);
    FL_CHECK(result[0] == '0');
    FL_CHECK(result[1] == 'x');
}

// =============================================================================
// Combined features
// =============================================================================

FL_TEST_CASE("format - width with hex") {
    fl::string result = fl::format("{:08x}", 255);
    FL_CHECK(result == "000000ff");
}

FL_TEST_CASE("format - alternate with width") {
    fl::string result = fl::format("{:#10x}", 255);
    FL_CHECK(result == "      0xff");
}

FL_TEST_CASE("format - positive float") {
    fl::string result = fl::format("{:+.2f}", 3.14);
    FL_CHECK(result == "+3.14");
}

FL_TEST_CASE("format - complex format") {
    fl::string result = fl::format("Dec:{} Hex:{:#x} Bin:{:#b}", 15, 15, 15);
    FL_CHECK(result == "Dec:15 Hex:0xf Bin:0b1111");
}

FL_TEST_CASE("format - mixed types") {
    fl::string result = fl::format("{} {} {} {}", "text", 42, 3.14f, 'X');
    FL_CHECK(fl::strstr(result.c_str(), "text") != nullptr);
    FL_CHECK(fl::strstr(result.c_str(), "42") != nullptr);
    FL_CHECK(fl::strstr(result.c_str(), "3.14") != nullptr);
    FL_CHECK(fl::strstr(result.c_str(), "X") != nullptr);
}
