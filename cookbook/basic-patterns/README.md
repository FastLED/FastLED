# Basic Patterns

**Difficulty Level**: ⭐ Beginner
**Time to Complete**: 1-2 hours (for entire section)
**Prerequisites**: [Getting Started](../getting-started/README.md), [Core Concepts](../core-concepts/README.md)

**You'll Learn**:
- How to create solid colors and fill effects on LED strips
- Timing control and smooth animations using brightness
- Movement patterns including chases and scanner effects
- Rainbow effects using HSV color space for easy color cycling

This section covers fundamental LED patterns that form the building blocks of more complex effects. These patterns are perfect for beginners and demonstrate core FastLED concepts.

## Contents

1. [Solid Colors](solid-colors.md) - Fill strips with single colors
2. [Animations](animations.md) - Simple fade in/out effects
3. [Chases](chases.md) - Moving patterns and scanner effects
4. [Rainbow](rainbow.md) - Rainbow and color cycling effects

## Prerequisites

Before working with these patterns, make sure you've completed the basic FastLED setup:

```cpp
#include <FastLED.h>

#define LED_PIN     5
#define NUM_LEDS    60
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(50);  // 0-255
}
```

## Pattern Categories

### Solid Colors
Learn how to fill your LED strip with solid colors using FastLED's efficient fill functions. These are the most basic operations you'll use in every project.

### Animations
Simple animations that demonstrate timing and state management. The fade in/out effect teaches you how to create smooth transitions using brightness control.

### Chases and Scanners
Moving patterns that show off trail effects and position tracking. The classic Cylon/Knight Rider scanner is a great introduction to animation loops.

### Rainbow Effects
Colorful effects using HSV color space. Learn the difference between static and animated rainbow patterns, and why HSV makes color cycling easy.

## Learning Path

1. Start with **Solid Colors** to understand basic LED control
2. Move to **Animations** to learn timing and state management
3. Try **Chases** to explore movement and trails
4. Experiment with **Rainbow** effects to master HSV color space

Each pattern builds on concepts from the previous ones, so following this order is recommended for beginners.
