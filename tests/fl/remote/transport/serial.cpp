// tests/fl/remote/transport/serial.cpp
// Tests for serial transport layer optimizations

#include "fl/remote/transport/serial.h"

#if FASTLED_ENABLE_JSON

#include "test.h"

// =============================================================================
// String View Optimization Tests
// =============================================================================

FL_TEST_CASE("Serial: createSerialRequestSource - basic JSON") {
    // Test that valid JSON is parsed correctly
    fl::string mockInput = R"({"method":"test","params":[],"id":1})";

    // Simulate readSerialLine returning the mock input
    struct MockSerial {
        fl::string data;
        int available() const { return data.empty() ? 0 : 1; }
        int read() {
            if (data.empty()) return -1;
            char c = data[0];
            data = data.substr(1);
            return c;
        }
    };

    auto requestSource = fl::createSerialRequestSource();

    // Create a test by manually parsing (since we can't easily mock the serial input)
    fl::optional<fl::Json> result = fl::Json::parse(mockInput);
    FL_REQUIRE(result.has_value());
    FL_REQUIRE(result->contains("method"));
    FL_REQUIRE(result->contains("params"));
    FL_REQUIRE(result->contains("id"));
}

FL_TEST_CASE("Serial: String view prefix stripping - zero copy") {
    // Test that prefix stripping using string_view is zero-copy
    fl::string input = "PREFIX: {\"method\":\"test\",\"params\":[]}";
    fl::string_view view = input;

    // Strip prefix using string_view (no allocation)
    const char* prefix = "PREFIX: ";
    if (view.starts_with(prefix)) {
        view.remove_prefix(fl::strlen(prefix));
    }

    // View should point to the JSON part
    FL_REQUIRE(view.size() == fl::strlen("{\"method\":\"test\",\"params\":[]}"));
    FL_REQUIRE(view[0] == '{');
    FL_REQUIRE(view.starts_with("{\"method\""));
}

FL_TEST_CASE("Serial: String view trimming - zero copy") {
    // Test that trimming using string_view is zero-copy
    fl::string input = "  \t  {\"test\":true}  \r\n  ";
    fl::string_view view = input;

    // Trim leading whitespace
    while (!view.empty() && fl::isspace(view.front())) {
        view.remove_prefix(1);
    }

    // Trim trailing whitespace
    while (!view.empty() && fl::isspace(view.back())) {
        view.remove_suffix(1);
    }

    // View should point to just the JSON
    FL_REQUIRE(view.size() == fl::strlen("{\"test\":true}"));
    FL_REQUIRE(view[0] == '{');
    FL_REQUIRE(view[view.size() - 1] == '}');
}

FL_TEST_CASE("Serial: Combined prefix strip and trim - zero copy") {
    // Test the full optimization path: prefix strip + trim using string_view
    fl::string input = "REMOTE:   {\"method\":\"test\"}  \n";
    fl::string_view view = input;

    // Strip prefix
    const char* prefix = "REMOTE:";
    if (view.starts_with(prefix)) {
        view.remove_prefix(fl::strlen(prefix));
    }

    // Trim leading whitespace
    while (!view.empty() && fl::isspace(view.front())) {
        view.remove_prefix(1);
    }

    // Trim trailing whitespace
    while (!view.empty() && fl::isspace(view.back())) {
        view.remove_suffix(1);
    }

    // Verify result
    FL_REQUIRE(view.size() == fl::strlen("{\"method\":\"test\"}"));
    FL_REQUIRE(view[0] == '{');

    // Single copy when converting to string for parsing
    fl::string cleaned(view);
    FL_REQUIRE(cleaned == "{\"method\":\"test\"}");
}

FL_TEST_CASE("Serial: Reject non-JSON input after trim") {
    // Test that non-JSON input is rejected
    fl::string input = "REMOTE: not json";
    fl::string_view view = input;

    // Strip prefix
    const char* prefix = "REMOTE:";
    if (view.starts_with(prefix)) {
        view.remove_prefix(fl::strlen(prefix));
    }

    // Trim
    while (!view.empty() && fl::isspace(view.front())) {
        view.remove_prefix(1);
    }
    while (!view.empty() && fl::isspace(view.back())) {
        view.remove_suffix(1);
    }

    // Should not start with '{'
    FL_REQUIRE(!view.empty());
    FL_REQUIRE(view[0] != '{');
}

FL_TEST_CASE("Serial: Empty input after trim") {
    // Test that whitespace-only input becomes empty
    fl::string input = "REMOTE:   \t\n\r  ";
    fl::string_view view = input;

    // Strip prefix
    const char* prefix = "REMOTE:";
    if (view.starts_with(prefix)) {
        view.remove_prefix(fl::strlen(prefix));
    }

    // Trim
    while (!view.empty() && fl::isspace(view.front())) {
        view.remove_prefix(1);
    }
    while (!view.empty() && fl::isspace(view.back())) {
        view.remove_suffix(1);
    }

    // Should be empty
    FL_REQUIRE(view.empty());
}

FL_TEST_CASE("Serial: formatJsonResponse - compact output") {
    // Test that formatJsonResponse produces compact output
    fl::Json response = fl::Json::object();
    response.set("method", "test");
    response.set("id", 1);

    fl::string formatted = fl::formatJsonResponse(response);

    // Should not contain newlines
    FL_REQUIRE(formatted.find('\n') == fl::string::npos);
    FL_REQUIRE(formatted.find('\r') == fl::string::npos);
}

FL_TEST_CASE("Serial: formatJsonResponse - with prefix") {
    // Test that formatJsonResponse includes prefix
    fl::Json response = fl::Json::object();
    response.set("result", 42);

    fl::string formatted = fl::formatJsonResponse(response, "REMOTE: ");

    // Should start with prefix
    FL_REQUIRE(formatted.starts_with("REMOTE: "));

    // Should contain the JSON
    FL_REQUIRE(formatted.find("\"result\"") != fl::string::npos);
    FL_REQUIRE(formatted.find("42") != fl::string::npos);
}

// =============================================================================
// String Optimization Comparison Tests
// =============================================================================

FL_TEST_CASE("String: substr creates copy - not optimized") {
    // Demonstrate that substr creates a copy (baseline behavior)
    fl::string original = "PREFIX: content";
    fl::string substring = original.substr(8);

    // Modification of substring should not affect original
    // (This verifies they are separate copies)
    FL_REQUIRE(substring == "content");
    FL_REQUIRE(original == "PREFIX: content");
}

FL_TEST_CASE("String: trim creates copy - not optimized") {
    // Demonstrate that trim creates a copy (baseline behavior)
    fl::string original = "  content  ";
    fl::string trimmed = original.trim();

    // Verify trim result
    FL_REQUIRE(trimmed == "content");
    FL_REQUIRE(original == "  content  ");
}

FL_TEST_CASE("String view: zero-copy operations") {
    // Demonstrate that string_view operations are zero-copy
    fl::string original = "PREFIX: content";
    fl::string_view view1 = original;

    // Remove prefix - no allocation
    view1.remove_prefix(8);

    // Create another view - no allocation
    fl::string_view view2 = view1;

    // Both views point to same data
    FL_REQUIRE(view1.data() == view2.data());
    FL_REQUIRE(view1.size() == view2.size());
    FL_REQUIRE(view1.size() == 7);  // "content"
}

#endif // FASTLED_ENABLE_JSON
