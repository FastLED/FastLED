// ok standalone
// Performance benchmark for JSON parsers
// Compares parse() (ArduinoJson) vs parse2() (custom parser) parsing speed

#include "test.h"
#include "fl/json.h"
#include "fl/stl/chrono.h"

using namespace fl;

// Test JSON (2.3KB ScreenMap)
constexpr const char* BENCHMARK_JSON = R"({
  "version": "1.0",
  "fps": 60,
  "brightness": 0.85,
  "strips": [
    {
      "id": "strip_0",
      "type": "WS2812B",
      "length": 100,
      "x": [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99],
      "y": [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
      "diameter": 0.5,
      "color_order": "RGB"
    },
    {
      "id": "strip_1",
      "type": "APA102",
      "length": 100,
      "x": [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99],
      "y": [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1],
      "diameter": 0.3,
      "color_order": "BGR"
    },
    {
      "id": "strip_2",
      "type": "WS2812B",
      "length": 100,
      "x": [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99],
      "y": [2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2],
      "diameter": 0.5,
      "color_order": "RGB"
    }
  ],
  "effects": [
    {"name": "rainbow", "speed": 1.5, "brightness": 0.9},
    {"name": "twinkle", "speed": 2.0, "brightness": 0.7},
    {"name": "fade", "speed": 0.5, "brightness": 1.0}
  ],
  "metadata": {
    "created": "2024-01-15",
    "author": "FastLED",
    "description": "Performance benchmark JSON"
  }
})";

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

    u32 duration = end - start;
    return static_cast<double>(duration) / iterations;
}

FL_TEST_CASE("JSON Performance: parse() vs parse2()") {
    printf("\n\n");
    printf("================================================================================\n");
    printf("JSON PERFORMANCE BENCHMARK\n");
    printf("================================================================================\n");
    printf("JSON size: %zu bytes\n", ::strlen(BENCHMARK_JSON));
    printf("JSON complexity: 3 strips × 100 LEDs each, nested objects/arrays\n");
    printf("Iterations: 1000\n");
    printf("\n");

    constexpr int ITERATIONS = 1000;

    // Benchmark ArduinoJson parse()
    double parse1_time = benchmark_microseconds([&]() {
        Json result = Json::parse(BENCHMARK_JSON);
        FL_REQUIRE(!result.is_null());
        // Force compiler to not optimize away
        volatile bool valid = result.is_object();
        (void)valid;
    }, ITERATIONS);

    // Benchmark Custom parse2()
    double parse2_time = benchmark_microseconds([&]() {
        Json result = Json::parse2(BENCHMARK_JSON);
        FL_REQUIRE(!result.is_null());
        // Force compiler to not optimize away
        volatile bool valid = result.is_object();
        (void)valid;
    }, ITERATIONS);

    // Results
    printf("Performance Results:\n");
    printf("  ArduinoJson parse():  %.2f µs/parse\n", parse1_time);
    printf("  Custom parse2():      %.2f µs/parse\n", parse2_time);
    printf("\n");

    // Comparison
    printf("================================================================================\n");
    printf("COMPARISON\n");
    printf("================================================================================\n");

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
    double throughput1_mbps = (::strlen(BENCHMARK_JSON) / parse1_time) * 1.0;  // bytes/µs = MB/s
    double throughput2_mbps = (::strlen(BENCHMARK_JSON) / parse2_time) * 1.0;

    printf("\n");
    printf("Throughput:\n");
    printf("  ArduinoJson parse():  %.2f MB/s\n", throughput1_mbps);
    printf("  Custom parse2():      %.2f MB/s\n", throughput2_mbps);

    printf("================================================================================\n");
    printf("\n");

    // Log results for README update
    printf("COPY TO README.md:\n");
    printf("| Metric | ArduinoJson parse() | Custom parse2() | Result |\n");
    printf("|--------|---------------------|-----------------|--------|\n");
    printf("| **Parse Time** | %.2f µs | %.2f µs | ", parse1_time, parse2_time);
    if (parse2_time < parse1_time) {
        printf("**%.2fx faster** |\n", speedup);
    } else {
        printf("%.2fx slower |\n", 1.0 / speedup);
    }
    printf("| **Throughput** | %.2f MB/s | %.2f MB/s | ", throughput1_mbps, throughput2_mbps);
    if (throughput2_mbps > throughput1_mbps) {
        printf("**+%.1f%%** |\n", ((throughput2_mbps / throughput1_mbps) - 1.0) * 100.0);
    } else {
        printf("%.1f%% |\n", ((throughput2_mbps / throughput1_mbps) - 1.0) * 100.0);
    }
    printf("\n");

    // Test always passes (informational only)
    FL_CHECK(true);
}
