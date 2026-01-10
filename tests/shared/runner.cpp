// Generic test runner that loads and executes test DLLs
// This executable is copied to each test directory as <test_name>.exe
// It automatically loads <test_name>.dll and calls run_tests()

#include <windef.h>
#include <libloaderapi.h>
#include <string>
#include <iostream>
#include <stddef.h>
#include "errhandlingapi.h"
#include "minwindef.h"
#include "winbase.h"

typedef int (*RunTestsFunc)();

int main(int /*argc*/, char** /*argv*/) {
    // Get executable path
    char exe_path[MAX_PATH];
    DWORD result = GetModuleFileNameA(NULL, exe_path, MAX_PATH);

    if (result == 0 || result == MAX_PATH) {
        std::cerr << "Error: Failed to get executable path" << std::endl;
        return 1;
    }

    // Extract directory and filename using string manipulation
    std::string full_path(exe_path);
    size_t last_slash = full_path.find_last_of("\\/");
    std::string exe_dir = (last_slash != std::string::npos) ? full_path.substr(0, last_slash) : ".";
    std::string exe_file = (last_slash != std::string::npos) ? full_path.substr(last_slash + 1) : full_path;

    // Remove .exe extension
    size_t dot_pos = exe_file.find_last_of('.');
    std::string exe_name = (dot_pos != std::string::npos) ? exe_file.substr(0, dot_pos) : exe_file;

    // Construct DLL path
    std::string dll_name = exe_name + ".dll";
    std::string dll_path = exe_dir + "\\" + dll_name;

    // Load DLL
    HMODULE dll = LoadLibraryA(dll_path.c_str());
    if (!dll) {
        DWORD error = GetLastError();
        std::cerr << "Error: Failed to load " << dll_name << " (error code: " << error << ")" << std::endl;
        std::cerr << "Expected path: " << dll_path << std::endl;
        return 1;
    }

    // Get run_tests function
    RunTestsFunc run_tests = (RunTestsFunc)GetProcAddress(dll, "run_tests");
    if (!run_tests) {
        std::cerr << "Error: Failed to find run_tests() in " << dll_name << std::endl;
        FreeLibrary(dll);
        return 1;
    }

    // Call test function
    int test_result = run_tests();

    // Cleanup
    FreeLibrary(dll);

    return test_result;
}
