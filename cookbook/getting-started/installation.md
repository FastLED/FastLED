# Installation and Setup

How to install FastLED for your development environment.

**Difficulty Level**: ⭐ Beginner
**Time to Complete**: 10-20 minutes (depending on internet speed and IDE download)
**Prerequisites**: None - this is the software setup step
**You'll Learn**:
- How to install FastLED in PlatformIO (recommended method)
- How to install FastLED in Arduino IDE
- How to manually install from source (advanced users)
- How to verify your installation is working correctly
- What to do after installation succeeds

## PlatformIO (Recommended)

PlatformIO provides the best development experience with dependency management, multiple board support, and easy configuration.

Add FastLED to your `platformio.ini`:

```ini
[env:myboard]
platform = ...
board = ...
lib_deps =
    fastled/FastLED
```

## Arduino IDE

1. Open **Sketch → Include Library → Manage Libraries**
2. Search for "FastLED"
3. Click **Install**
4. Wait for installation to complete
5. Restart Arduino IDE

## Manual Installation

If you need to install from source:

1. Download the latest release from [GitHub](https://github.com/FastLED/FastLED/releases)
2. Extract the archive
3. Copy the `FastLED` folder to your libraries directory:
   - **Windows**: `Documents\Arduino\libraries\`
   - **macOS**: `~/Documents/Arduino/libraries/`
   - **Linux**: `~/Arduino/libraries/`

## Verification

Test your installation with this minimal program:

```cpp
#include <FastLED.h>

void setup() {
    Serial.begin(115200);
    Serial.println("FastLED is installed!");
}

void loop() {
    // Your code here
}
```

If it compiles without errors, you're ready to go!

## Next Steps

- [Hardware Requirements](hardware.md) - Gather the components you need
- [Basic Concepts](concepts.md) - Understand LED programming fundamentals
