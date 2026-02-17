# Font Support

This directory contains font rendering support for FastLED.

## TrueType Fonts

### `truetype.h` / `truetype.cpp.hpp`
TrueType font rendering support for LED matrices.

### `ttf_covenant5x5.h` / `ttf_covenant5x5.cpp.hpp`
Example TrueType font (Covenant 5x5) for LED displays.

## Bitmap Console Fonts

Bitmap fonts courtesy of [raster-fonts](https://github.com/idispatch/raster-fonts), originally copied from the WLED project.

Available fonts:
- `console_font_4x6.h` - 4x6 pixel bitmap font
- `console_font_5x8.h` - 5x8 pixel bitmap font
- `console_font_5x12.h` - 5x12 pixel bitmap font
- `console_font_6x8.h` - 6x8 pixel bitmap font
- `console_font_7x9.h` - 7x9 pixel bitmap font

### ⚠️ BETA PREVIEW WARNING

All bitmap fonts are **BETA PREVIEW**. If you choose to use these fonts, keep in mind they will break in future versions until this API stabilizes.

**Known limitations:**
- Missing mapping functions to convert ASCII symbols to bit patterns for LED strip arrays
- API subject to change without notice

**Recommendation:** If you want to use these fonts, it's **STRONGLY encouraged** that you copy them to your own project. If you bind to these headers, you are on your own to fix your project when they change.

## Usage

All fonts are in the `fl` namespace and use `FL_PROGMEM` for memory optimization on embedded platforms.

```cpp
#include "fl/font/console_font_5x8.h"  // Bitmap font
#include "fl/font/truetype.h"           // TrueType support

// Use fl::console_font_5x8 for bitmap rendering
// Use fl::TrueType classes for vector font rendering
```
