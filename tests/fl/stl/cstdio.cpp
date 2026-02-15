// Unit tests for fl::readStringUntil and fl::sstream integration
// Tests the new readline API with sstream buffer

#include "fl/stl/cstdio.h"
#include "fl/stl/cstring.h"
#include "fl/stl/function.h"
#include "fl/stl/optional.h"
#include "fl/stl/stdio.h"
#include "fl/stl/string.h"
#include "fl/stl/strstream.h"
#include "test.h"

FL_TEST_CASE("fl::readStringUntil basic") {
    // Inject simple read handler that returns "hello\n"
    const char* testData = "hello\n";
    size_t pos = 0;

    fl::inject_available_handler([&]() {
        return (pos < fl::strlen(testData)) ? 1 : 0;
    });

    fl::inject_read_handler([&]() {
        return (pos < fl::strlen(testData)) ? testData[pos++] : -1;
    });

    // Test readStringUntil directly to sstream
    fl::sstream buffer;
    bool success = fl::readStringUntil(buffer, '\n', '\r', fl::nullopt);

    FL_CHECK(success);
    FL_CHECK_EQ(buffer.str(), "hello");

    fl::clear_io_handlers();
}

FL_TEST_CASE("fl::readStringUntil with skipChar") {
    // Inject handler that returns "hello\r\nworld\n" (Windows line ending)
    const char* testData = "hello\r\nworld\n";
    size_t pos = 0;

    fl::inject_available_handler([&]() {
        return (pos < fl::strlen(testData)) ? 1 : 0;
    });

    fl::inject_read_handler([&]() {
        return (pos < fl::strlen(testData)) ? testData[pos++] : -1;
    });

    // Read first line - should skip '\r' and stop at first '\n'
    fl::sstream buffer;
    bool success = fl::readStringUntil(buffer, '\n', '\r', fl::nullopt);

    FL_CHECK(success);
    FL_CHECK_EQ(buffer.str(), "hello");  // '\r' should be skipped

    // Read second line - pos should auto-advance from first read
    buffer.clear();
    success = fl::readStringUntil(buffer, '\n', '\r', fl::nullopt);

    FL_CHECK(success);
    FL_CHECK_EQ(buffer.str(), "world");

    fl::clear_io_handlers();
}

FL_TEST_CASE("fl::readLine delegation") {
    // Inject handler that returns "test data\n"
    const char* testData = "test data\n";
    size_t pos = 0;

    fl::inject_available_handler([&]() {
        return (pos < fl::strlen(testData)) ? 1 : 0;
    });

    fl::inject_read_handler([&]() {
        return (pos < fl::strlen(testData)) ? testData[pos++] : -1;
    });

    // Test readLine which delegates to readStringUntil
    auto result = fl::readLine('\n', '\r', fl::nullopt);

    FL_CHECK(result.has_value());
    FL_CHECK_EQ(result.value(), "test data");

    fl::clear_io_handlers();
}

FL_TEST_CASE("fl::readLine trims whitespace") {
    // Inject handler that returns "  hello world  \n" (spaces before and after)
    const char* testData = "  hello world  \n";
    size_t pos = 0;

    fl::inject_available_handler([&]() {
        return (pos < fl::strlen(testData)) ? 1 : 0;
    });

    fl::inject_read_handler([&]() {
        return (pos < fl::strlen(testData)) ? testData[pos++] : -1;
    });

    // readLine should trim whitespace
    auto result = fl::readLine('\n', '\r', fl::nullopt);

    FL_CHECK(result.has_value());
    FL_CHECK_EQ(result.value(), "hello world");

    fl::clear_io_handlers();
}

FL_TEST_CASE("fl::readStringUntil empty line") {
    // Inject handler that returns just delimiter
    const char* testData = "\n";
    size_t pos = 0;

    fl::inject_available_handler([&]() {
        return (pos < fl::strlen(testData)) ? 1 : 0;
    });

    fl::inject_read_handler([&]() {
        return (pos < fl::strlen(testData)) ? testData[pos++] : -1;
    });

    // Should return empty string (but success=true)
    fl::sstream buffer;
    bool success = fl::readStringUntil(buffer, '\n', '\r', fl::nullopt);

    FL_CHECK(success);
    FL_CHECK_EQ(buffer.str(), "");

    fl::clear_io_handlers();
}

FL_TEST_CASE("fl::printf %lu format test") {
    // Test that %lu now works correctly (length modifier is skipped)
    char buf[128];
    fl::u32 value = 4294967295U;
    fl::snprintf(buf, sizeof(buf), "Value: %lu", static_cast<unsigned long>(value));

    fl::string result(buf);
    fl::string expected = "Value: 4294967295";

    FL_CHECK_EQ(result, expected);
}

FL_TEST_CASE("fl::printf octal format") {
    char buf[128];

    // Basic octal
    fl::snprintf(buf, sizeof(buf), "%o", 8);
    FL_CHECK_EQ(fl::string(buf), "10");

    fl::snprintf(buf, sizeof(buf), "%o", 64);
    FL_CHECK_EQ(fl::string(buf), "100");

    fl::snprintf(buf, sizeof(buf), "%o", 0);
    FL_CHECK_EQ(fl::string(buf), "0");

    fl::snprintf(buf, sizeof(buf), "%o", 255);
    FL_CHECK_EQ(fl::string(buf), "377");
}

FL_TEST_CASE("fl::printf width specifier") {
    char buf[128];

    // Right-aligned with spaces (default)
    fl::snprintf(buf, sizeof(buf), "%5d", 42);
    FL_CHECK_EQ(fl::string(buf), "   42");

    fl::snprintf(buf, sizeof(buf), "%10s", "test");
    FL_CHECK_EQ(fl::string(buf), "      test");

    // Width smaller than content - no truncation
    fl::snprintf(buf, sizeof(buf), "%2d", 12345);
    FL_CHECK_EQ(fl::string(buf), "12345");
}

FL_TEST_CASE("fl::printf zero-padding flag") {
    char buf[128];

    // Zero-padded integers
    fl::snprintf(buf, sizeof(buf), "%02x", 0x0F);
    FL_CHECK_EQ(fl::string(buf), "0f");

    fl::snprintf(buf, sizeof(buf), "%02X", 0x0F);
    FL_CHECK_EQ(fl::string(buf), "0F");

    fl::snprintf(buf, sizeof(buf), "%04x", 0x12);
    FL_CHECK_EQ(fl::string(buf), "0012");

    fl::snprintf(buf, sizeof(buf), "%08x", 0xDEADBEEF);
    FL_CHECK_EQ(fl::string(buf), "deadbeef");

    // Zero-padded decimals
    fl::snprintf(buf, sizeof(buf), "%05d", 42);
    FL_CHECK_EQ(fl::string(buf), "00042");

    fl::snprintf(buf, sizeof(buf), "%03o", 8);
    FL_CHECK_EQ(fl::string(buf), "010");
}

FL_TEST_CASE("fl::printf left-align flag") {
    char buf[128];

    // Left-aligned
    fl::snprintf(buf, sizeof(buf), "%-5d", 42);
    FL_CHECK_EQ(fl::string(buf), "42   ");

    fl::snprintf(buf, sizeof(buf), "%-10s", "test");
    FL_CHECK_EQ(fl::string(buf), "test      ");
}

FL_TEST_CASE("fl::printf plus flag") {
    char buf[128];

    // Always show sign
    fl::snprintf(buf, sizeof(buf), "%+d", 42);
    FL_CHECK_EQ(fl::string(buf), "+42");

    fl::snprintf(buf, sizeof(buf), "%+d", -42);
    FL_CHECK_EQ(fl::string(buf), "-42");

    fl::snprintf(buf, sizeof(buf), "%+d", 0);
    FL_CHECK_EQ(fl::string(buf), "+0");
}

FL_TEST_CASE("fl::printf space flag") {
    char buf[128];

    // Prefix positive with space
    fl::snprintf(buf, sizeof(buf), "% d", 42);
    FL_CHECK_EQ(fl::string(buf), " 42");

    fl::snprintf(buf, sizeof(buf), "% d", -42);
    FL_CHECK_EQ(fl::string(buf), "-42");
}

FL_TEST_CASE("fl::printf hash flag") {
    char buf[128];

    // Alternate form for hex
    fl::snprintf(buf, sizeof(buf), "%#x", 0x2A);
    FL_CHECK_EQ(fl::string(buf), "0x2a");

    fl::snprintf(buf, sizeof(buf), "%#X", 0x2A);
    FL_CHECK_EQ(fl::string(buf), "0X2A");

    // Alternate form for octal
    fl::snprintf(buf, sizeof(buf), "%#o", 8);
    FL_CHECK_EQ(fl::string(buf), "010");

    // Zero has no prefix
    fl::snprintf(buf, sizeof(buf), "%#x", 0);
    FL_CHECK_EQ(fl::string(buf), "0");
}

FL_TEST_CASE("fl::printf combined flags and width") {
    char buf[128];

    // Zero-pad with width
    fl::snprintf(buf, sizeof(buf), "%08lx", 0xDEADBEEF);
    FL_CHECK_EQ(fl::string(buf), "deadbeef");

    // Left-align with width
    fl::snprintf(buf, sizeof(buf), "%-8d", 42);
    FL_CHECK_EQ(fl::string(buf), "42      ");

    // Plus sign with width
    fl::snprintf(buf, sizeof(buf), "%+5d", 42);
    FL_CHECK_EQ(fl::string(buf), "  +42");

    // Hash with zero-pad
    fl::snprintf(buf, sizeof(buf), "%#06x", 0x2A);
    FL_CHECK_EQ(fl::string(buf), "0x002a");
}

FL_TEST_CASE("fl::printf pointer format") {
    char buf[128];

    // Basic pointer formatting
    int value = 42;
    int* ptr = &value;
    fl::snprintf(buf, sizeof(buf), "%p", ptr);

    // Should start with "0x" prefix
    fl::string result(buf);
    FL_CHECK(result.length() >= 3);  // At least "0x" + 1 hex digit
    FL_CHECK_EQ(result[0], '0');
    FL_CHECK_EQ(result[1], 'x');

    // Test null pointer
    void* null_ptr = nullptr;
    fl::snprintf(buf, sizeof(buf), "%p", null_ptr);
    FL_CHECK_EQ(fl::string(buf), "0x0");

    // Test const pointer
    const char* str = "test";
    fl::snprintf(buf, sizeof(buf), "%p", str);
    result = fl::string(buf);
    FL_CHECK(result.length() >= 3);
    FL_CHECK_EQ(result[0], '0');
    FL_CHECK_EQ(result[1], 'x');
}

// NOTE: Dynamic width/precision features (%*d, %.*f, %*.*f) are NOT yet implemented
// These tests are commented out until the variadic template unpacking complexity is resolved
//
// FL_TEST_CASE("fl::printf dynamic width") {
//     char buf[128];
//
//     // Dynamic width from argument
//     fl::snprintf(buf, sizeof(buf), "%*d", 5, 42);
//     FL_CHECK_EQ(fl::string(buf), "   42");
//
//     fl::snprintf(buf, sizeof(buf), "%*s", 10, "test");
//     FL_CHECK_EQ(fl::string(buf), "      test");
//
//     // Dynamic width with other flags
//     fl::snprintf(buf, sizeof(buf), "%0*d", 5, 42);
//     FL_CHECK_EQ(fl::string(buf), "00042");
//
//     fl::snprintf(buf, sizeof(buf), "%-*d", 5, 42);
//     FL_CHECK_EQ(fl::string(buf), "42   ");
// }
//
// FL_TEST_CASE("fl::printf dynamic precision") {
//     char buf[128];
//
//     // Dynamic precision for floats
//     fl::snprintf(buf, sizeof(buf), "%.*f", 2, 3.14159f);
//     FL_CHECK_EQ(fl::string(buf), "3.14");
//
//     fl::snprintf(buf, sizeof(buf), "%.*f", 0, 3.14159f);
//     FL_CHECK_EQ(fl::string(buf), "3");
//
//     fl::snprintf(buf, sizeof(buf), "%.*f", 4, 3.14159f);
//     FL_CHECK_EQ(fl::string(buf), "3.1416");
// }
//
// FL_TEST_CASE("fl::printf dynamic width and precision") {
//     char buf[128];
//
//     // Both width and precision dynamic
//     fl::snprintf(buf, sizeof(buf), "%*.*f", 10, 2, 3.14159f);
//     FL_CHECK_EQ(fl::string(buf), "      3.14");
//
//     fl::snprintf(buf, sizeof(buf), "%*.*f", 8, 3, 3.14159f);
//     FL_CHECK_EQ(fl::string(buf), "   3.142");
// }

// Advanced features not yet implemented:
// - %p (pointer format) - requires specialized template handling for void*
// - %e/%E (scientific notation) - requires exponential formatting
// - %g/%G (general float) - requires choosing between %f and %e
// - %*d (dynamic width) - requires runtime argument consumption in variadic templates
// - %.*f (dynamic precision) - requires runtime argument consumption in variadic templates
