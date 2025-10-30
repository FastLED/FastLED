# FastLED Recipes

**Difficulty Level**: ⭐ to ⭐⭐ Beginner to Intermediate
**Time to Complete**: 1-2 hours (to explore all four recipe documents)
**Prerequisites**:
- [Getting Started](../getting-started/) - Basic LED control and FastLED setup
- [Core Concepts](../core-concepts/) - Understanding color theory, arrays, and timing
- [Basic Patterns](../basic-patterns/) - Foundation pattern programming
- Recommended: [Intermediate Techniques](../intermediate/) - For deeper understanding of techniques used in recipes

**You'll Learn**:
- How to use ready-to-use, copy-paste LED effect recipes
- Fire simulation techniques using heat arrays and color palettes
- Twinkle/sparkle effects with random LED selection
- Breathing/pulsing animations with smooth fading
- Various wave patterns including sine waves and color waves

---

This directory contains practical, ready-to-use recipes for common LED effects. Each recipe is self-contained with complete code examples and explanations.

## Available Recipes

- **[Fire Effect](fire.md)** - Realistic fire simulation using heat arrays and color palettes
- **[Twinkle/Sparkle](twinkle.md)** - Random twinkling stars and sparkle effects
- **[Breathing Effect](breathing.md)** - Smooth pulsing/breathing animations
- **[Wave Patterns](waves.md)** - Various wave effects including sine waves, color waves, and ocean-inspired patterns

## Using These Recipes

Each recipe is designed to be copied directly into your Arduino sketch. They assume you have:

```cpp
#include <FastLED.h>

#define NUM_LEDS 60
CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
}
```

Then simply copy the effect function into your code and call it from your `loop()` function.

## Customization Tips

- Adjust timing by changing the `delay()` values or beat rates
- Modify colors by changing the CRGB or CHSV values
- Scale effects to your LED count by adjusting loop ranges
- Combine multiple effects for unique patterns

## Contributing

Have a great recipe to share? Contributions are welcome! Make sure your recipe:
- Is self-contained and well-documented
- Includes parameter explanations
- Follows existing code style
- Has been tested on actual hardware
