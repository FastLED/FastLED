// ok standalone
// JSON Parser A/B Benchmark
// Compares parse() (ArduinoJson) vs parse2() (custom parser)
//
// Usage:
//   ./benchmark_json_parsers              # Run both small and large benchmarks (default)
//   ./benchmark_json_parsers small        # Run small JSON benchmark only
//   ./benchmark_json_parsers large        # Run large JSON benchmark only

#include "fl/file_system.h"
#include "fl/int.h"
#include "fl/json.h"
#include "fl/stl/chrono.h"
#include "fl/stl/cstring.h"
#include "fl/stl/stdio.h"
#include "fl/stl/string.h"
#include "stdio.h"  // ok include

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

void run_benchmark(const char* test_name, const fl::string& json_data, int iterations, bool& success) {
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
        if (result.is_null()) {
            success = false;
        }
        volatile bool valid = !result.is_null();
        (void)valid;
    }, iterations);

    // Benchmark Custom parse2()
    double parse2_time = benchmark_microseconds([&]() {
        Json result = Json::parse2(json_data);
        if (result.is_null()) {
            success = false;
        }
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

    // Structured output for AI consumption
    printf("PROFILE_RESULT:{\n");
    printf("  \"test\": \"%s\",\n", test_name);
    printf("  \"json_size_bytes\": %zu,\n", json_data.size());
    printf("  \"iterations\": %d,\n", iterations);
    printf("  \"parse1_us\": %.2f,\n", parse1_time);
    printf("  \"parse2_us\": %.2f,\n", parse2_time);
    printf("  \"speedup\": %.2f,\n", speedup);
    printf("  \"throughput1_mbps\": %.2f,\n", throughput1_mbps);
    printf("  \"throughput2_mbps\": %.2f\n", throughput2_mbps);
    printf("}\n");
}

bool run_small_benchmark() {
    bool success = true;

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

    run_benchmark("TEST 1: SMALL JSON (2.3KB Synthetic)", small_json, 1000, success);
    return success;
}

bool run_large_benchmark() {
    bool success = true;

    printf("\nLoading large JSON file: tests/profile/benchmark_1mb.json\n");

    // Initialize FileSystem for testing
    FileSystem fs;
    FsImplPtr fsImpl = make_sdcard_filesystem(0);
    if (!fs.begin(fsImpl)) {
        printf("❌ ERROR: Failed to initialize test filesystem\n");
        return false;
    }

    // Open and read the JSON file
    FileHandlePtr fh = fs.openRead("tests/profile/benchmark_1mb.json");
    if (!fh || !fh->valid()) {
        printf("❌ ERROR: Could not open tests/profile/benchmark_1mb.json\n");
        printf("   Download it with: curl -o tests/profile/benchmark_1mb.json https://microsoftedge.github.io/Demos/json-dummy-data/1MB.json\n");
        return false;
    }

    // Read file contents
    fl::size file_size = fh->size();
    fl::string large_json;
    large_json.resize(file_size);
    fl::size bytes_read = fh->read(reinterpret_cast<u8*>(&large_json[0]), file_size);
    fh->close();

    if (bytes_read != file_size) {
        printf("❌ ERROR: Read %zu bytes but expected %zu bytes\n", bytes_read, file_size);
        return false;
    }

    printf("✓ Loaded %zu bytes (%.2f KB)\n", bytes_read, bytes_read / 1024.0);

    run_benchmark("TEST 2: LARGE JSON (1MB Real-World Data)", large_json, 50, success);
    return success;
}

void print_usage() {
    printf("JSON Parser A/B Benchmark\n");
    printf("Compares parse() (ArduinoJson) vs parse2() (custom parser)\n\n");
    printf("Usage:\n");
    printf("  benchmark_json_parsers              # Run both benchmarks (default)\n");
    printf("  benchmark_json_parsers small        # Run small JSON benchmark\n");
    printf("  benchmark_json_parsers large        # Run large JSON benchmark\n");
}

int main(int argc, char* argv[]) {
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

    const char* mode = "all";  // Default mode
    if (argc > 1) {
        mode = argv[1];
    }

    bool success = true;

    if (fl::strcmp(mode, "small") == 0) {
        success = run_small_benchmark();
    } else if (fl::strcmp(mode, "large") == 0) {
        success = run_large_benchmark();
    } else if (fl::strcmp(mode, "all") == 0) {
        success = run_small_benchmark() && run_large_benchmark();

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
    } else if (fl::strcmp(mode, "help") == 0 || fl::strcmp(mode, "--help") == 0 || fl::strcmp(mode, "-h") == 0) {
        print_usage();
        return 0;
    } else {
        printf("Unknown mode: %s\n\n", mode);
        print_usage();
        return 1;
    }

    return success ? 0 : 1;
}
