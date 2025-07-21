#include "test.h"

#ifdef FASTLED_HAS_NETWORKING

#include "fl/networking/http_client.h"
#include "fl/networking/http_types.h"
#include "fl/future.h"
#include "fl/string.h"

using namespace fl;

TEST_CASE("Real HTTP GET to fastled.io - NO MOCKS!") {
    FL_WARN("🌐 TESTING REAL HTTP CONNECTION TO FASTLED.IO - NO MOCKS!");
    
    // Create a simple HTTP client
    auto client = HttpClient::create_simple_client();
    REQUIRE(client != nullptr);
    
    FL_WARN("📡 Making HTTP GET request to http://fastled.io");
    
    // Make the actual HTTP request to fastled.io
    auto response_future = client->get("http://fastled.io");
    
    // Get the result - this should be a real response from fastled.io
    auto result = response_future.try_get_result();
    
    if (result.is<Response>()) {
        auto response = *result.ptr<Response>();
        
        FL_WARN("✅ GOT REAL RESPONSE FROM FASTLED.IO!");
        FL_WARN("Status Code: " << response.get_status_code_int());
        FL_WARN("Status Text: " << response.get_status_text());
        
        // Check that we got a valid HTTP response
        REQUIRE(response.get_status_code_int() > 0);
        
        // Accept either 200 OK or redirect status codes (3xx)
        bool valid_status = response.is_success() || response.is_redirection();
        if (!valid_status) {
            FL_WARN("❌ Unexpected status code: " << response.get_status_code_int());
            FL_WARN("Response body: " << response.get_body_text());
        }
        REQUIRE(valid_status);
        
        // Check that we got headers
        REQUIRE(response.headers().size() > 0);
        FL_WARN("Headers received: " << response.headers().size());
        
        // Log some key headers
        auto content_type = response.get_content_type();
        if (content_type) {
            FL_WARN("Content-Type: " << *content_type);
        }
        
        auto server = response.get_server();
        if (server) {
            FL_WARN("Server: " << *server);
        }
        
        // Check that we got some body content
        if (response.has_body()) {
            FL_WARN("Response body size: " << response.get_body_size() << " bytes");
            
            // Log first 200 chars of response for debugging
            fl::string body = response.get_body_text();
            if (body.size() > 200) {
                FL_WARN("Response preview: " << body.substr(0, 200) << "...");
            } else {
                FL_WARN("Response body: " << body);
            }
        }
        
        FL_WARN("🎉 REAL HTTP TEST PASSED! Successfully connected to fastled.io");
        
    } else if (result.is<FutureError>()) {
        auto error = *result.ptr<FutureError>();
        FL_WARN("❌ HTTP Request failed: " << error.message);
        FAIL(("Failed to connect to fastled.io: " + error.message).c_str());
        
    } else {
        FL_WARN("⏳ Request still pending - this shouldn't happen in blocking test");
        FAIL("Request returned neither result nor error");
    }
}

TEST_CASE("Real HTTP GET with simple convenience function") {
    FL_WARN("🌐 TESTING SIMPLE HTTP_GET FUNCTION");
    
    // Use the simple convenience function
    auto response_future = http_get("http://fastled.io");
    
    auto result = response_future.try_get_result();
    
    if (result.is<Response>()) {
        auto response = *result.ptr<Response>();
        
        FL_WARN("✅ Simple http_get() worked!");
        FL_WARN("Status: " << response.get_status_code_int() << " " << response.get_status_text());
        
        // Accept success or redirect
        bool valid_status = response.is_success() || response.is_redirection();
        REQUIRE(valid_status);
        
        FL_WARN("🎉 Simple HTTP function test passed!");
        
    } else if (result.is<FutureError>()) {
        auto error = *result.ptr<FutureError>();
        FL_WARN("❌ Simple http_get failed: " << error.message);
        FAIL(("Simple http_get failed: " + error.message).c_str());
        
    } else {
        FAIL("Simple http_get returned unexpected result type");
    }
}

TEST_CASE("HTTP Client error handling") {
    FL_WARN("🌐 TESTING ERROR HANDLING WITH INVALID URL");
    
    auto client = HttpClient::create_simple_client();
    
    // Test with invalid URL
    auto response_future = client->get("invalid://not.a.real.url");
    auto result = response_future.try_get_result();
    
    // Should get an error, not a successful response
    REQUIRE(result.is<FutureError>());
    
    if (result.is<FutureError>()) {
        auto error = *result.ptr<FutureError>();
        FL_WARN("✅ Got expected error for invalid URL: " << error.message);
    }
    
    FL_WARN("🎉 Error handling test passed!");
}

#else
TEST_CASE("HTTP Tests Skipped - Networking Disabled") {
    FL_WARN("⚠️ HTTP tests skipped - FASTLED_HAS_NETWORKING not defined");
    // Just pass - can't test HTTP without networking support
}
#endif // FASTLED_HAS_NETWORKING 
