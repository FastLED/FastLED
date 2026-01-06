// Test runner - loads test category DLLs and executes tests
// This executable is lightweight and doesn't link against libfastled.a

#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using run_tests_fn = int(*)(int, const char**);

struct TestCategory {
    std::string name;
    std::string dll_name;
};

// List of test category DLLs to load
static const TestCategory test_categories[] = {
    {"core_tests", "libcore_tests.dll"},
    {"fl_tests_1", "libfl_tests_1.dll"},
    {"fl_tests_2", "libfl_tests_2.dll"},
    {"ftl_tests", "libftl_tests.dll"},
    {"fx_tests", "libfx_tests.dll"},
    {"noise_tests", "libnoise_tests.dll"},
    {"platform_tests", "libplatform_tests.dll"},
};

#ifdef _WIN32
static void* load_library(const char* path) {
    return LoadLibraryA(path);
}

static void* get_function(void* handle, const char* name) {
    return (void*)GetProcAddress((HMODULE)handle, name);
}

static void free_library(void* handle) {
    FreeLibrary((HMODULE)handle);
}

static const char* get_error() {
    static char buffer[256];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
                   0, buffer, sizeof(buffer), NULL);
    return buffer;
}
#else
static void* load_library(const char* path) {
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

static void* get_function(void* handle, const char* name) {
    return dlsym(handle, name);
}

static void free_library(void* handle) {
    dlclose(handle);
}

static const char* get_error() {
    return dlerror();
}
#endif

int main(int argc, char** argv) {
    int total_failures = 0;
    int categories_run = 0;
    int categories_passed = 0;

    std::cout << "[TEST_RUNNER] Running " << (sizeof(test_categories) / sizeof(test_categories[0]))
              << " test categories\n";

    for (const auto& category : test_categories) {
        std::cout << "[TEST_RUNNER] Loading " << category.name << "...\n";

        void* lib = load_library(category.dll_name.c_str());
        if (!lib) {
            std::cerr << "[TEST_RUNNER] ERROR: Failed to load " << category.dll_name
                     << ": " << get_error() << "\n";
            total_failures++;
            continue;
        }

        auto run_tests = (run_tests_fn)get_function(lib, "run_tests");
        if (!run_tests) {
            std::cerr << "[TEST_RUNNER] ERROR: Failed to find run_tests in " << category.dll_name
                     << ": " << get_error() << "\n";
            free_library(lib);
            total_failures++;
            continue;
        }

        std::cout << "[TEST_RUNNER] Running tests in " << category.name << "...\n";

        // Convert argv to const char** for the DLL
        std::vector<const char*> args;
        for (int i = 0; i < argc; i++) {
            args.push_back(argv[i]);
        }

        int result = run_tests(argc, args.data());
        categories_run++;

        if (result == 0) {
            std::cout << "[TEST_RUNNER] ✓ " << category.name << " PASSED\n";
            categories_passed++;
        } else {
            std::cout << "[TEST_RUNNER] ✗ " << category.name << " FAILED (exit code " << result << ")\n";
            total_failures++;
        }

        free_library(lib);
    }

    std::cout << "\n[TEST_RUNNER] Summary: " << categories_passed << "/" << categories_run
              << " test categories passed\n";

    return total_failures > 0 ? 1 : 0;
}
