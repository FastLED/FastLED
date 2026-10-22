// tests/fl/remote/transport/serial.cpp
// Tests for serial transport layer optimizations

#include "fl/remote/transport/serial.h"
#include "fl/stl/cstring.h"
#include "fl/stl/detail/string_holder.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

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
    fl::optional<fl::json> result = fl::json::parse(mockInput);
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
    fl::json response = fl::json::object();
    response.set("method", "test");
    response.set("id", 1);

    fl::string formatted = fl::formatJsonResponse(response);

    // Should not contain newlines
    FL_REQUIRE(formatted.find('\n') == fl::string::npos);
    FL_REQUIRE(formatted.find('\r') == fl::string::npos);
}

FL_TEST_CASE("Serial: formatJsonResponse - with prefix") {
    // Test that formatJsonResponse includes prefix
    fl::json response = fl::json::object();
    response.set("result", 42);

    fl::string formatted = fl::formatJsonResponse(response, "REMOTE: ");

    // Should start with prefix
    FL_REQUIRE(formatted.starts_with("REMOTE: "));

    // Should contain the JSON
    FL_REQUIRE(formatted.find("\"result\"") != fl::string::npos);
    FL_REQUIRE(formatted.find("42") != fl::string::npos);
}

namespace serial_response_growth_test {

void checkPayload(fl::size payloadSize) {
    const fl::string payload(payloadSize, 'x');
    fl::json response = fl::json::object();
    response.set("payload", payload);
    // Repeated production formatting exercises prefix growth and reuse of
    // freed allocations, as repeated RPC replies do on ESP32.
    for (int repeat = 0; repeat < 3; ++repeat) {
        const fl::string formatted = fl::formatJsonResponse(response, "REMOTE: ");
        const fl::size payloadOffset = 8 + 12; // prefix + {"payload":"
        FL_REQUIRE_EQ(formatted.size(), payloadSize + 8 + 14);
        FL_CHECK(formatted.starts_with("REMOTE: {\"payload\":\""));
        FL_CHECK_EQ(fl::memcmp(formatted.c_str() + payloadOffset,
                              payload.c_str(), payloadSize), 0);
        FL_CHECK_EQ(formatted.c_str()[formatted.size() - 2], '"');
        FL_CHECK_EQ(formatted.c_str()[formatted.size() - 1], '}');
        FL_CHECK_EQ(formatted.c_str()[formatted.size()], '\0');
    }
}

} // namespace serial_response_growth_test

FL_TEST_CASE("Serial: formatJsonResponse preserves bytes across allocation boundaries") {
    // Include every small boundary, then powers of two and adjacent lengths.
    // Cover prefix insertion at both inline and heap allocation boundaries.
    for (fl::size size = 0; size <= 256; ++size) {
        serial_response_growth_test::checkPayload(size);
    }
    for (fl::size size = 512; size <= 65536; size *= 2) {
        serial_response_growth_test::checkPayload(size - 1);
        serial_response_growth_test::checkPayload(size);
        serial_response_growth_test::checkPayload(size + 1);
    }
    serial_response_growth_test::checkPayload(100000);
}

FL_TEST_CASE("Serial: formatJsonResponse preserves escapes in a large schema-shaped reply") {
    fl::json methods = fl::json::array();
    for (int index = 0; index < 256; ++index) {
        fl::json method = fl::json::array();
        method.push_back("legacyAsyncProbe");
        method.push_back("json");
        method.push_back("quotes\"slash\\newline\ncarriage\rtab\t");
        methods.push_back(method);
    }
    fl::json response = fl::json::object();
    response.set("schema", methods);
    const fl::string expected = response.to_string();
    const fl::string formatted = fl::formatJsonResponse(response, "REMOTE: ");
    FL_REQUIRE_EQ(formatted.size(), expected.size() + 8);
    FL_CHECK_EQ(fl::memcmp(formatted.c_str() + 8, expected.c_str(), expected.size()), 0);
    FL_CHECK_EQ(formatted.c_str()[formatted.size()], '\0');
    FL_CHECK_EQ(formatted.find('\n'), fl::string::npos);
    FL_CHECK_EQ(formatted.find('\r'), fl::string::npos);
}

FL_TEST_CASE("Serial: formatJsonResponse reports failed prefix growth without corrupting JSON") {
    fl::json response = fl::json::object();
    response.set("payload", fl::string(1000, 'x'));
    struct FailGrowth {
        bool previous = fl::StringHolder::setGrowFailureForTesting(true);
        ~FailGrowth() { fl::StringHolder::setGrowFailureForTesting(previous); }
    } fail;
    const fl::string formatted = fl::formatJsonResponse(response, "REMOTE: ");
    FL_REQUIRE(formatted.starts_with("REMOTE: "));
    const fl::json parsed = fl::json::parse(formatted.substr(8));
    FL_CHECK_EQ(parsed["jsonrpc"].as_string().value_or(""), "2.0");
    FL_CHECK_EQ(parsed["error"]["code"].as_int().value_or(0), -32603);
    FL_CHECK(parsed["id"].is_null());
    FL_CHECK_EQ(formatted.c_str()[formatted.size()], '\0');
}

FL_TEST_CASE("Serial: createSerialResponseStreamSink - streams framed JSON") {
    fl::string output;
    fl::size maxWrite = 0;

    fl::inject_write_bytes_handler([&](const fl::u8* data, fl::size size) -> fl::size {
        output.append(reinterpret_cast<const char*>(data), size);  // ok reinterpret cast
        if (size > maxWrite) {
            maxWrite = size;
        }
        return size;
    });

    auto sink = fl::createSerialResponseStreamSink("REMOTE: ");
    sink([](fl::JsonStreamWriter& writer) {
        writer.beginObject();
        writer.member("jsonrpc", "2.0");
        writer.key("result");
        writer.beginArray();
        for (int i = 0; i < 64; ++i) {
            writer.value("abcdef0123456789");
        }
        writer.endArray();
        writer.member("id", 5);
        writer.endObject();
    });

    fl::clear_io_handlers();

    FL_REQUIRE(output.starts_with("REMOTE: "));
    FL_REQUIRE(output.back() == '\n');
    FL_CHECK(maxWrite <= fl::JsonStreamWriter::kBufSize);

    fl::string jsonText = output.substr(fl::strlen("REMOTE: "));
    jsonText = jsonText.substr(0, jsonText.size() - 1);
    fl::json parsed = fl::json::parse(jsonText);
    FL_CHECK_EQ(parsed["jsonrpc"].as_string().value_or(""), "2.0");
    FL_CHECK_EQ(parsed["id"].as_int().value_or(0), 5);
    FL_CHECK_EQ(parsed["result"].size(), 64);
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

// FastLED #4893: a request that overflowed the console receive buffer arrives
// truncated. It must be reported, and still passed on so the RPC layer can
// answer with its invalid-request error.
FL_TEST_CASE("Serial: truncated JSON request is reported, not dropped silently") {
    static const char kLine[] = "{\"method\":\"debugTest\",\"params\":[{\"t\":\"xxxx\n";
    static fl::size pos = 0;
    pos = 0;
    fl::inject_available_handler([]() {
        return pos < sizeof(kLine) - 1 ? 1 : 0;
    });
    fl::inject_read_handler([]() {
        return pos < sizeof(kLine) - 1 ? static_cast<int>(kLine[pos++]) : -1;
    });
    static fl::string captured;
    captured.clear();
    fl::inject_print_handler([](const char* text) { captured += text; });
    fl::inject_println_handler([](const char* text) { captured += text; });

    auto requestSource = fl::createSerialRequestSource();
    fl::optional<fl::json> request = requestSource();

    fl::clear_io_handlers();
    FL_REQUIRE(request.has_value());
    FL_CHECK_FALSE(request->has_value());
    FL_CHECK(captured.find("malformed") != fl::string::npos);
}

} // FL_TEST_FILE

