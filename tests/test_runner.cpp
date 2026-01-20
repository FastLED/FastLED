// Test runner - loads test category DLLs and executes tests
// This executable is lightweight and doesn't link against libfastled.a
// CRITICAL: NO STL USAGE - prevents CRT boundary issues across DLLs

#include "fl/str.h" // ok include
#include <stdlib.h> // ok include - test runner needs printf/fprintf for DLL boundary safety

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

typedef int(*run_tests_fn)(int, const char**);

struct TestCategory {
    const char* name;
    const char* dll_name;
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

    int num_categories = sizeof(test_categories) / sizeof(test_categories[0]);
    printf("[TEST_RUNNER] Running %d test categories\n", num_categories);

    for (int i = 0; i < num_categories; i++) {
        const TestCategory* category = &test_categories[i];
        printf("[TEST_RUNNER] Loading %s...\n", category->name);

        void* lib = load_library(category->dll_name);
        if (!lib) {
            fprintf(stderr, "[TEST_RUNNER] ERROR: Failed to load %s: %s\n",
                    category->dll_name, get_error());
            total_failures++;
            continue;
        }

        run_tests_fn run_tests = (run_tests_fn)get_function(lib, "run_tests");
        if (!run_tests) {
            fprintf(stderr, "[TEST_RUNNER] ERROR: Failed to find run_tests in %s: %s\n",
                    category->dll_name, get_error());
            free_library(lib);
            total_failures++;
            continue;
        }

        printf("[TEST_RUNNER] Running tests in %s...\n", category->name);

        // Pass argv directly to the DLL
        int result = run_tests(argc, (const char**)argv);
        categories_run++;

        if (result == 0) {
            printf("[TEST_RUNNER] ✓ %s PASSED\n", category->name);
            categories_passed++;
        } else {
            printf("[TEST_RUNNER] ✗ %s FAILED (exit code %d)\n", category->name, result);
            total_failures++;
        }

        free_library(lib);
    }

    printf("\n[TEST_RUNNER] Summary: %d/%d test categories passed\n",
           categories_passed, categories_run);

    return total_failures > 0 ? 1 : 0;
}
