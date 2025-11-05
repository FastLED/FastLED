/**
 * @file win_main.cpp
 * @brief Windows WinMain wrapper for native platform builds
 *
 * On Windows with MinGW, when building for the native platform, the linker
 * expects WinMain() instead of main(). This file provides a WinMain() wrapper
 * that calls the standard main() function.
 *
 * This file is automatically added to the build for PLATFORM_NATIVE Windows builds.
 */

#if defined(PLATFORM_NATIVE) && defined(_WIN32)

// Define WIN32_LEAN_AND_MEAN before including windows.h to avoid pulling in
// unnecessary Windows headers that might conflict with FastLED types
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

// Forward declare main() from the generated main.cpp
extern "C" int main(void);

/**
 * Windows entry point that forwards to standard main()
 *
 * @param hInstance Current instance handle
 * @param hPrevInstance Previous instance handle (always NULL in Win32)
 * @param lpCmdLine Command line string
 * @param nCmdShow Window show state
 * @return Exit code from main()
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Suppress unused parameter warnings
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    // Call the standard main() function
    return main();
}

#endif  // PLATFORM_NATIVE && _WIN32
