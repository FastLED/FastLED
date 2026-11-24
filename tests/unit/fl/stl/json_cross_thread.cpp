// Cross-thread stress test for fl::json / fl::string / fl::map
// (FastLED#3588). On classic ESP32, an esp_http_server task building
// Request/Response objects (fl::string + fl::map) concurrently with the
// main loop building fl::json RPC responses corrupts main-task json
// objects (LoadProhibited on garbage pointers inside shared_ptr
// machinery). This test reproduces that workload shape host-side where
// ASAN/UBSAN (debug mode) can catch the culprit.
//
// Each thread uses ONLY its own objects — no shared containers — so any
// sanitizer hit or crash indicates hidden shared state inside the fl::
// implementation (allocator pools, sentinels, COW holders, ...).

#include "test.h"

#include "fl/stl/json.h"
#include "fl/stl/map.h"
#include "fl/stl/string.h"

#include "fl/stl/atomic.h"

#include <thread>  // ok include - host-only stress test; fl::thread not available

FL_TEST_FILE(FL_FILEPATH) {

namespace {

// Mimics the AutoResearch runSingleTest response builder (main task).
void jsonResponderWorkload(fl::atomic<bool>& stop, fl::atomic<int>& iters) {
    while (!stop.load()) {
        fl::json response = fl::json::object();
        response.set("driver", "I2S");
        response.set("passed", true);
        fl::json patterns = fl::json::array();
        for (int p = 0; p < 4; ++p) {
            fl::json entry = fl::json::object();
            entry.set("runNumber", static_cast<int64_t>(p));
            entry.set("totalLeds", static_cast<int64_t>(10));
            entry.set("decodeBytes", static_cast<int64_t>(30 * p));
            entry.set("captureWaitResult", static_cast<int64_t>(1));
            patterns.push_back(entry);
        }
        response.set("patterns", patterns);
        response.set("duration_ms", static_cast<int64_t>(699));
        fl::string serialized = response.to_string();
        FL_CHECK(serialized.size() > 0);
        iters.fetch_add(1);
    }
}

// Mimics the esp_http_server glue (httpd task): Request headers/query
// maps + response strings.
void httpHandlerWorkload(fl::atomic<bool>& stop, fl::atomic<int>& iters) {
    while (!stop.load()) {
        fl::map<fl::string, fl::string> headers;
        headers["Host"] = "192.168.4.1";
        headers["User-Agent"] = "esp-idf/5.5 http_client";
        headers["Accept"] = "*/*";
        headers["Content-Type"] = "application/json";
        fl::map<fl::string, fl::string> query;
        query["a"] = "1";
        query["b"] = "2";

        fl::json body = fl::json::object();
        body.set("uptime_ms", static_cast<int64_t>(123456));
        body.set("free_heap", static_cast<int64_t>(25000));
        body.set("chip", "esp32");
        fl::string serialized = body.to_string();
        FL_CHECK(serialized.size() > 0);

        fl::string status_line;
        status_line += "HTTP/1.1 200 OK\r\n";
        for (auto it = headers.begin(); it != headers.end(); ++it) {
            status_line += it->first;
            status_line += ": ";
            status_line += it->second;
            status_line += "\r\n";
        }
        FL_CHECK(status_line.size() > 20);
        iters.fetch_add(1);
    }
}

} // namespace

FL_TEST_CASE("json + map/string workloads race-free across threads") {
    fl::atomic<bool> stop(false);
    fl::atomic<int> json_iters(0);
    fl::atomic<int> http_iters(0);

    std::thread responder([&]() { jsonResponderWorkload(stop, json_iters); });  // okay std namespace - fl::thread not available
    std::thread handler1([&]() { httpHandlerWorkload(stop, http_iters); });     // okay std namespace - fl::thread not available
    std::thread handler2([&]() { httpHandlerWorkload(stop, http_iters); });     // okay std namespace - fl::thread not available

    // Run long enough for allocator/COW interleavings to collide.
    while (json_iters.load() < 2000 || http_iters.load() < 2000) {
        std::this_thread::yield();  // okay std namespace - host-only stress test
    }
    stop.store(true);
    responder.join();
    handler1.join();
    handler2.join();

    FL_CHECK(json_iters.load() >= 2000);
    FL_CHECK(http_iters.load() >= 2000);
}

}
