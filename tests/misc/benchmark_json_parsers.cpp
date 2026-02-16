// ok standalone
// JSON Parser A/B Benchmark
// Compares parse() (ArduinoJson) vs parse2() (custom parser)

#include "test.h"
#include "fl/json.h"
#include "fl/stl/chrono.h"
#include "fl/file_system.h"
#include <stdio.h> // ok include

using namespace fl;

// Benchmark helper
template<typename Func>
double benchmark_microseconds(Func&& func, int iterations) {
    // Warmup
    func();

    u32 start = fl::micros();
    for (int i = 0; i < iterations; i++) {
        func();
    }
    u32 end = fl::micros();

    return static_cast<double>(end - start) / iterations;
}

void run_benchmark(const char* test_name, const fl::string& json_data, int iterations) {
    printf("\n");
    printf("================================================================================\n");
    printf("%s\n", test_name);
    printf("================================================================================\n");
    printf("JSON size: %zu bytes (%.2f KB)\n", json_data.size(), json_data.size() / 1024.0);
    printf("Iterations: %d\n", iterations);
    printf("\n");

    // Benchmark ArduinoJson parse()
    double parse1_time = benchmark_microseconds([&]() {
        Json result = Json::parse(json_data);
        volatile bool valid = !result.is_null();
        (void)valid;
    }, iterations);

    // Benchmark Custom parse2()
    double parse2_time = benchmark_microseconds([&]() {
        Json result = Json::parse2(json_data);
        volatile bool valid = !result.is_null();
        (void)valid;
    }, iterations);

    // Results
    printf("Performance Results:\n");
    printf("  ArduinoJson parse():  %.2f µs/parse\n", parse1_time);
    printf("  Custom parse2():      %.2f µs/parse\n", parse2_time);
    printf("\n");

    // Comparison
    printf("================================ COMPARISON =====================================\n");

    double speedup = parse1_time / parse2_time;
    double ratio = parse2_time / parse1_time;

    if (parse2_time < parse1_time) {
        printf("✓ parse2() is FASTER:   %.2fx speedup (%.1f%% of parse() time)\n",
               speedup, ratio * 100.0);
        printf("  Time saved: %.2f µs per parse (%.1f%% reduction)\n",
               parse1_time - parse2_time, (1.0 - ratio) * 100.0);
    } else {
        printf("✗ parse2() is SLOWER:   %.2fx slowdown (%.1f%% of parse() time)\n",
               1.0 / speedup, ratio * 100.0);
        printf("  Extra time: %.2f µs per parse (%.1f%% increase)\n",
               parse2_time - parse1_time, (ratio - 1.0) * 100.0);
    }

    // Throughput
    double throughput1_mbps = (json_data.size() / parse1_time) * 1.0;  // bytes/µs = MB/s
    double throughput2_mbps = (json_data.size() / parse2_time) * 1.0;

    printf("\n");
    printf("Throughput:\n");
    printf("  ArduinoJson parse():  %.2f MB/s\n", throughput1_mbps);
    printf("  Custom parse2():      %.2f MB/s\n", throughput2_mbps);
    printf("================================================================================\n\n");
}

FL_TEST_CASE("JSON Parser A/B Benchmark") {
    printf("\n\n");
    printf("################################################################################\n");
    printf("#                                                                              #\n");
    printf("#                   JSON PARSER A/B BENCHMARK RESULTS                          #\n");
    printf("#                                                                              #\n");
    printf("################################################################################\n");
    printf("\n");
    printf("Comparing:\n");
    printf("  • parse()  - ArduinoJson library (external dependency)\n");
    printf("  • parse2() - Custom native parser (zero external dependencies)\n");
    printf("\n");

    // Test 1: Small JSON (2.3KB synthetic ScreenMap)
    const char* small_json = R"({
  "version": "1.0",
  "fps": 60,
  "brightness": 0.85,
  "strips": [
    {"id": "strip_0", "type": "WS2812B", "length": 100},
    {"id": "strip_1", "type": "APA102", "length": 100},
    {"id": "strip_2", "type": "WS2812B", "length": 100}
  ],
  "effects": [
    {"name": "rainbow", "speed": 1.5, "brightness": 0.9},
    {"name": "twinkle", "speed": 2.0, "brightness": 0.7}
  ]
})";

    run_benchmark("TEST 1: SMALL JSON (2.3KB Synthetic)", small_json, 1000);

    // Test 2: Large JSON (1MB real-world data)
    printf("\nLoading large JSON file: tests/profile/benchmark_1mb.json\n");

    // Initialize FileSystem for testing
    FileSystem fs;
    FsImplPtr fsImpl = make_sdcard_filesystem(0);
    if (!fs.begin(fsImpl)) {
        printf("❌ ERROR: Failed to initialize test filesystem\n");
        FL_REQUIRE(false);
        return;
    }

    // Open and read the JSON file
    FileHandlePtr fh = fs.openRead("tests/profile/benchmark_1mb.json");
    if (!fh || !fh->valid()) {
        printf("❌ ERROR: Could not open tests/profile/benchmark_1mb.json\n");
        printf("   Download it with: curl -o tests/profile/benchmark_1mb.json https://microsoftedge.github.io/Demos/json-dummy-data/1MB.json\n");
        FL_REQUIRE(false);
        return;
    }

    // Read file contents
    fl::size file_size = fh->size();
    fl::string large_json;
    large_json.resize(file_size);
    fl::size bytes_read = fh->read(reinterpret_cast<u8*>(&large_json[0]), file_size);
    fh->close();

    if (bytes_read != file_size) {
        printf("❌ ERROR: Read %zu bytes but expected %zu bytes\n", bytes_read, file_size);
        FL_REQUIRE(false);
        return;
    }

    printf("✓ Loaded %zu bytes (%.2f KB)\n", bytes_read, bytes_read / 1024.0);

    run_benchmark("TEST 2: LARGE JSON (1MB Real-World Data)", large_json, 50);

    // Summary
    printf("\n");
    printf("================================================================================\n");
    printf("                              BENCHMARK COMPLETE                                \n");
    printf("================================================================================\n");
    printf("\n");
    printf("📊 Results show performance comparison on both small synthetic and large\n");
    printf("   real-world JSON datasets.\n");
    printf("\n");
    printf("🔗 Sources:\n");
    printf("  - Small JSON: Synthetic FastLED ScreenMap configuration\n");
    printf("  - Large JSON: Microsoft Edge Demos 1MB test dataset\n");
    printf("    https://microsoftedge.github.io/Demos/json-dummy-data/\n");
    printf("\n");

    FL_CHECK(true);
}
