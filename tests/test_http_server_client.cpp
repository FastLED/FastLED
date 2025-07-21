#ifdef FASTLED_HAS_NETWORKING

#include <doctest.h>
#include "fl/net/http/server.h"
#include "fl/net/http/client.h"
#include "fl/net/http/types.h"
#include "fl/string.h"
#include "fl/shared_ptr.h"
#include <chrono>
#include <thread>

using namespace fl;

// Helper function to setup test server and client
struct TestSetup {
    fl::shared_ptr<HttpServer> server;
    fl::shared_ptr<HttpClient> client;
    int test_port;
    fl::string base_url;
    
    TestSetup() {
        // Create server and client
        server = HttpServer::create_local_server();
        client = HttpClient::create_simple_client();
        
        // Find an available port (start from 18080 to avoid conflicts)
        test_port = 18080;
        while (!server->listen(test_port, "127.0.0.1")) {
            test_port++;
            if (test_port > 18100) {
                FAIL("Could not find available port for testing");
            }
        }
        
        base_url = "http://127.0.0.1:" + fl::to_string(test_port);
        
        // Server should start immediately
    }
    
    ~TestSetup() {
        if (server) {
            server->stop();
        }
    }
    
    void ProcessServerRequests() {
        // Process server requests manually (since we're not in FastLED loop)
        if (server) {
            server->process_requests();
        }
    }
};

TEST_CASE("HTTP Server Client - Basic GET Request") {
    TestSetup setup;
    
    // Setup server route
    setup.server->get("/test", [](const Request& req) {
        return ResponseBuilder::ok("Hello, World!");
    });
    
    // Make client request
    auto response_future = setup.client->get(setup.base_url + "/test");
    
    // Process server requests
    auto result = response_future.try_get_result();
    for (int i = 0; i < 100 && !(result.is<Response>() || result.is<FutureError>()); ++i) {
        setup.ProcessServerRequests();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        result = response_future.try_get_result();
    }
    
    // Check response
    REQUIRE(result.is<Response>());
    
    Response response = *result.ptr<Response>();
    CHECK(response.get_status_code() == HttpStatusCode::OK);
    CHECK(response.get_body_text() == "Hello, World!");
}

TEST_CASE("HTTP Server Client - POST Request with Body") {
    TestSetup setup;
    
    // Setup server route
    setup.server->post("/echo", [](const Request& req) {
        fl::string received_body = req.get_body_text();
        return ResponseBuilder::ok("Echo: " + received_body);
    });
    
    // Make client request with body
    fl::string test_data = "Test POST data";
    auto response_future = setup.client->post(setup.base_url + "/echo", test_data, "text/plain");
    
    // Process server requests
    auto result = response_future.try_get_result();
    for (int i = 0; i < 100 && !(result.is<Response>() || result.is<FutureError>()); ++i) {
        setup.ProcessServerRequests();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        result = response_future.try_get_result();
    }
    
    // Check response
    REQUIRE(result.is<Response>());
    
    Response response = *result.ptr<Response>();
    CHECK(response.get_status_code() == HttpStatusCode::OK);
    CHECK(response.get_body_text() == "Echo: " + test_data);
}

TEST_CASE("HTTP Server Client - JSON Request Response") {
    TestSetup setup;
    
    // Setup server route for JSON
    setup.server->post("/api/data", [](const Request& req) {
        // Simple JSON validation (just check if it starts with {)
        fl::string body = req.get_body_text();
        if (!body.empty() && body[0] == '{' && body[body.size()-1] == '}') {
            fl::string response_json = "{\"status\": \"received\", \"length\": " + 
                                     fl::to_string(body.size()) + "}";
            return ResponseBuilder::json_response(response_json);
        } else {
            return ResponseBuilder::bad_request("Invalid JSON");
        }
    });
    
    // Make JSON request
    fl::string json_data = "{\"name\": \"test\", \"value\": 123}";
    auto response_future = setup.client->post(setup.base_url + "/api/data", json_data, "application/json");
    
    // Process server requests
    auto result = response_future.try_get_result();
    for (int i = 0; i < 100 && !(result.is<Response>() || result.is<FutureError>()); ++i) {
        setup.ProcessServerRequests();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        result = response_future.try_get_result();
    }
    
    // Check response
    REQUIRE(result.is<Response>());
    
    Response response = *result.ptr<Response>();
    CHECK(response.get_status_code() == HttpStatusCode::OK);
    
    // Check Content-Type header
    auto content_type = response.get_header("Content-Type");
    REQUIRE(!content_type.empty());
    CHECK(*content_type == "application/json");
    
    // Check response body contains expected JSON
    fl::string response_body = response.get_body_text();
    CHECK(response_body.contains("\"status\": \"received\""));
    CHECK(response_body.contains("\"length\": " + fl::to_string(json_data.size())));
}

TEST_CASE("HTTP Server Client - Not Found Response") {
    TestSetup setup;
    
    // Don't register any routes, so all requests should return 404
    auto response_future = setup.client->get(setup.base_url + "/nonexistent");
    
    // Process server requests
    auto result = response_future.try_get_result();
    for (int i = 0; i < 100 && !(result.is<Response>() || result.is<FutureError>()); ++i) {
        setup.ProcessServerRequests();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        result = response_future.try_get_result();
    }
    
    // Check response
    REQUIRE(result.is<Response>());
    
    Response response = *result.ptr<Response>();
    CHECK(response.get_status_code() == HttpStatusCode::NOT_FOUND);
    CHECK(response.get_body_text().contains("Not Found"));
}

TEST_CASE("HTTP Server Client - Middleware Processing") {
    TestSetup setup;
    
    // Add middleware that adds a custom header
    setup.server->use([](const Request& req, ResponseBuilder& res) {
        res.header("X-Custom-Middleware", "processed");
        return true; // Continue processing
    });
    
    // Add CORS middleware
    setup.server->use_cors("*", "GET, POST", "Content-Type");
    
    // Setup server route
    setup.server->get("/middleware-test", [](const Request& req) {
        return ResponseBuilder::ok("Middleware test");
    });
    
    // Make request
    auto response_future = setup.client->get(setup.base_url + "/middleware-test");
    
    // Process server requests
    auto result = response_future.try_get_result();
    for (int i = 0; i < 100 && !(result.is<Response>() || result.is<FutureError>()); ++i) {
        setup.ProcessServerRequests();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        result = response_future.try_get_result();
    }
    
    // Check response
    REQUIRE(result.is<Response>());
    
    Response response = *result.ptr<Response>();
    CHECK(response.get_status_code() == HttpStatusCode::OK);
    
    // Check middleware headers
    auto custom_header = response.get_header("X-Custom-Middleware");
    REQUIRE(!custom_header.empty());
    CHECK(*custom_header == "processed");
    
    auto cors_header = response.get_header("Access-Control-Allow-Origin");
    REQUIRE(!cors_header.empty());
    CHECK(*cors_header == "*");
}

TEST_CASE("HTTP Server Client - Multiple Routes") {
    TestSetup setup;
    
    // Setup multiple routes
    setup.server->get("/users", [](const Request& req) {
        return ResponseBuilder::json_response("[{\"id\": 1, \"name\": \"Alice\"}]");
    });
    
    setup.server->post("/users", [](const Request& req) {
        return fl::move(ResponseBuilder().json("{\"id\": 2, \"name\": \"Bob\"}").status(201)).build();
    });
    
    setup.server->get("/health", [](const Request& req) {
        return ResponseBuilder::ok("Healthy");
    });
    
    // Test GET /users
    {
        auto response_future = setup.client->get(setup.base_url + "/users");
        
        auto result = response_future.try_get_result();
        for (int i = 0; i < 100 && !(result.is<Response>() || result.is<FutureError>()); ++i) {
            setup.ProcessServerRequests();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            result = response_future.try_get_result();
        }
        
        REQUIRE(result.is<Response>());
        
        Response response = *result.ptr<Response>();
        CHECK(response.get_status_code() == HttpStatusCode::OK);
        CHECK(response.get_body_text().contains("Alice"));
    }
    
    // Test POST /users
    {
        auto response_future = setup.client->post(setup.base_url + "/users", fl::string("{\"name\": \"Charlie\"}"), "application/json");
        
        auto result = response_future.try_get_result();
        for (int i = 0; i < 100 && !(result.is<Response>() || result.is<FutureError>()); ++i) {
            setup.ProcessServerRequests();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            result = response_future.try_get_result();
        }
        
        REQUIRE(result.is<Response>());
        
        Response response = *result.ptr<Response>();
        CHECK(response.get_status_code() == HttpStatusCode::CREATED);
        CHECK(response.get_body_text().contains("Bob"));
    }
    
    // Test GET /health
    {
        auto response_future = setup.client->get(setup.base_url + "/health");
        
        auto result = response_future.try_get_result();
        for (int i = 0; i < 100 && !(result.is<Response>() || result.is<FutureError>()); ++i) {
            setup.ProcessServerRequests();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            result = response_future.try_get_result();
        }
        
        REQUIRE(result.is<Response>());
        
        Response response = *result.ptr<Response>();
        CHECK(response.get_status_code() == HttpStatusCode::OK);
        CHECK(response.get_body_text() == "Healthy");
    }
}

TEST_CASE("HTTP Server Client - Error Handling") {
    TestSetup setup;
    
    // Setup route that returns an error
    setup.server->get("/error", [](const Request& req) {
        return ResponseBuilder::internal_error("Something went wrong");
    });
    
    // Make request to error endpoint
    auto response_future = setup.client->get(setup.base_url + "/error");
    
    // Process server requests
    auto result = response_future.try_get_result();
    for (int i = 0; i < 100 && !(result.is<Response>() || result.is<FutureError>()); ++i) {
        setup.ProcessServerRequests();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        result = response_future.try_get_result();
    }
    
    // Check response
    REQUIRE(result.is<Response>());
    
    Response response = *result.ptr<Response>();
    CHECK(response.get_status_code() == HttpStatusCode::INTERNAL_SERVER_ERROR);
    CHECK(response.get_body_text().contains("Something went wrong"));
}

TEST_CASE("HTTP Server Client - Server Statistics") {
    TestSetup setup;
    
    // Setup some routes
    setup.server->get("/stats-test", [](const Request& req) {
        return ResponseBuilder::ok("Stats test");
    });
    
    // Make several requests
    for (int i = 0; i < 3; ++i) {
        auto response_future = setup.client->get(setup.base_url + "/stats-test");
        
        auto result = response_future.try_get_result();
        for (int j = 0; j < 100 && !(result.is<Response>() || result.is<FutureError>()); ++j) {
            setup.ProcessServerRequests();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            result = response_future.try_get_result();
        }
        
        REQUIRE(result.is<Response>());
    }
    
    // Check server statistics
    auto stats = setup.server->get_stats();
    CHECK(stats.total_requests_handled >= 3);
    CHECK(stats.route_matches >= 3);
    CHECK(stats.not_found_responses == 0);  // All requests should have matched
    CHECK(stats.server_uptime_ms > 0);
}

TEST_CASE("HTTP Server Client - Simple Server Factories") {
    // Test the simple factory functions
    auto local_server = create_local_server();
    REQUIRE(local_server != nullptr);
    
    auto dev_server = create_development_server();
    REQUIRE(dev_server != nullptr);
    
    // Test that we can start and stop them
    int test_port = 18200;  // Use different port to avoid conflicts
    CHECK(local_server->listen(test_port, "127.0.0.1"));
    CHECK(local_server->is_listening());
    local_server->stop();
    CHECK_FALSE(local_server->is_listening());
}

#endif // FASTLED_HAS_NETWORKING 
