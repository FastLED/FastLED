// Unit tests for fl::readStringUntil and fl::sstream integration
// Tests the new readline API with sstream buffer

#include "fl/stl/cstdio.h"
#include "fl/stl/strstream.h"
#include "fl/stl/string.h"
#include "fl/stl/optional.h"
#include "fl/stl/cstring.h"
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
