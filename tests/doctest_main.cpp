// Fix INPUT macro conflict between Arduino and Windows headers
#ifdef INPUT
#define ARDUINO_INPUT_BACKUP INPUT
#undef INPUT
#endif

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

// Restore Arduino INPUT macro after Windows headers
#ifdef ARDUINO_INPUT_BACKUP
#define INPUT ARDUINO_INPUT_BACKUP
#undef ARDUINO_INPUT_BACKUP
#endif

#ifdef ENABLE_CRASH_HANDLER
#include "crash_handler.h"
#endif

// This file contains the main function for doctest
// It will be compiled once and linked to all test executables

#ifdef _WIN32
// Windows-specific entry point
#include <windows.h>

// Define the main entry point for Windows
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow; // Suppress unused parameter warnings
#ifdef ENABLE_CRASH_HANDLER
    setup_crash_handler();
#endif
    return doctest::Context().run();
}

// Also provide a standard main for compatibility
int main(int argc, char** argv) {
#ifdef ENABLE_CRASH_HANDLER
    setup_crash_handler();
#endif
    return doctest::Context(argc, argv).run();
}
#else
// Standard entry point for non-Windows platforms
int main(int argc, char** argv) {
#ifdef ENABLE_CRASH_HANDLER
    setup_crash_handler();
#endif
    return doctest::Context(argc, argv).run();
}
#endif
