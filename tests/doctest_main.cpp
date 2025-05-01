#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

// This file contains the main function for doctest
// It will be compiled once and linked to all test executables

#ifdef _WIN32
// Windows-specific entry point
#include <windows.h>

// Define the main entry point for Windows
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {
    return doctest::Context().run();
}

// Also provide a standard main for compatibility
int main(int argc, char** argv) {
    return doctest::Context(argc, argv).run();
}
#else
// Standard entry point for non-Windows platforms
int main(int argc, char** argv) {
    return doctest::Context(argc, argv).run();
}
#endif
