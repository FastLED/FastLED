// ok standalone
// Performance benchmark for JSON parsers
// Compares parse() (ArduinoJson) vs parse2() (custom parser) parsing speed
//
// Usage:
//   ./json_performance              # Run small JSON benchmark
//   ./json_performance small        # Run small JSON benchmark
//   ./json_performance large        # Run large JSON benchmark (requires benchmark_1mb.json)
//   ./json_performance all          # Run all benchmarks

#include "fl/file_system.h"
#include "fl/int.h"
#include "fl/json.h"
#include "fl/stl/chrono.h"
#include "fl/stl/cstdint.h"
#include "fl/stl/cstring.h"
#include "fl/stl/stdio.h"
#include "fl/stl/string.h"
#include "stdio.h"  // ok include

using namespace fl;

// Small Test JSON (2.3KB ScreenMap)
constexpr const char* SMALL_BENCHMARK_JSON = R"({
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

// Helper to run benchmark on a JSON string
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
        // Force compiler to not optimize away
        volatile bool valid = result.is_object() || result.is_array();
        (void)valid;
    }, iterations);

    // Benchmark Custom parse2()
    double parse2_time = benchmark_microseconds([&]() {
        Json result = Json::parse2(json_data);
        if (result.is_null()) {
            success = false;
        }
        // Force compiler to not optimize away
        volatile bool valid = result.is_object() || result.is_array();
        (void)valid;
    }, iterations);

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
    double throughput1_mbps = (json_data.size() / parse1_time) * 1.0;  // bytes/µs = MB/s
    double throughput2_mbps = (json_data.size() / parse2_time) * 1.0;

    printf("\n");
    printf("Throughput:\n");
    printf("  ArduinoJson parse():  %.2f MB/s\n", throughput1_mbps);
    printf("  Custom parse2():      %.2f MB/s\n", throughput2_mbps);

    printf("================================================================================\n");

    // Structured output for AI consumption
    printf("\nPROFILE_RESULT:{\n");
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
    run_benchmark("SMALL JSON BENCHMARK (2.3KB ScreenMap)", SMALL_BENCHMARK_JSON, 1000, success);
    return success;
}

bool run_large_benchmark() {
    bool success = true;

    // Load large JSON file
    fl::string large_json;
    const char* filepath = "tests/profile/benchmark_1mb.json";

    printf("\nLoading large JSON file: %s\n", filepath);

    // Initialize FileSystem for testing
    FileSystem fs;
    FsImplPtr fsImpl = make_sdcard_filesystem(0);
    if (!fs.begin(fsImpl)) {
        printf("❌ ERROR: Failed to initialize test filesystem\n");
        return false;
    }

    // Open and read the JSON file
    FileHandlePtr fh = fs.openRead(filepath);
    if (!fh || !fh->valid()) {
        printf("❌ ERROR: Could not open %s\n", filepath);
        printf("   Make sure to download it first with:\n");
        printf("   curl -o %s https://microsoftedge.github.io/Demos/json-dummy-data/1MB.json\n", filepath);
        return false;
    }

    size_t file_size = fh->size();
    large_json.resize(file_size);
    size_t bytes_read = fh->read(reinterpret_cast<u8*>(&large_json[0]), file_size);
    fh->close();

    if (bytes_read != file_size) {
        printf("❌ ERROR: Read %zu bytes but expected %zu bytes\n", bytes_read, file_size);
        return false;
    }

    printf("✓ Loaded %zu bytes (%.2f KB)\n", bytes_read, bytes_read / 1024.0);

    // Run benchmark with fewer iterations for large file
    run_benchmark("LARGE JSON BENCHMARK (1MB Real-World Data)", large_json, 100, success);

    return success;
}

void print_usage() {
    printf("JSON Performance Profiler\n");
    printf("Compares parse() (ArduinoJson) vs parse2() (custom parser)\n\n");
    printf("Usage:\n");
    printf("  json_performance              # Run small JSON benchmark (default)\n");
    printf("  json_performance small        # Run small JSON benchmark\n");
    printf("  json_performance large        # Run large JSON benchmark\n");
    printf("  json_performance all          # Run all benchmarks\n");
}

int main(int argc, char* argv[]) {
    const char* mode = "small";  // Default mode

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
