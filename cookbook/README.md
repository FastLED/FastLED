# FastLED Cookbook

A practical guide to creating LED effects with FastLED.

This cookbook is organized into focused sections, each covering a specific aspect of LED programming with FastLED.

## Table of Contents

### [Getting Started](getting-started/)
Learn the basics to get your LED strip up and running.
- [Installation and Setup](getting-started/installation.md)
- [Hardware Requirements](getting-started/hardware.md)
- [Basic Concepts](getting-started/concepts.md)
- [Your First Blink Example](getting-started/first-example.md)

### [Core Concepts](core-concepts/)
Understand the fundamental building blocks of LED programming.
- [Understanding LED Data Structures](core-concepts/led-structures.md)
- [Color Theory](core-concepts/color-theory.md)
- [Timing and Frame Rate](core-concepts/timing.md)
- [Power Considerations](core-concepts/power.md)

### [Basic Patterns](basic-patterns/)
Start creating your first LED animations and effects.
- [Solid Colors](basic-patterns/solid-colors.md)
- [Simple Animations](basic-patterns/animations.md)
- [Chases and Scanners](basic-patterns/chases.md)
- [Rainbow Effects](basic-patterns/rainbow.md)

### [Classical LED Drawing](core-concepts/led-structures.md#array-manipulation)
Learn fundamental pixel manipulation techniques.
- Direct pixel addressing: `leds[i] = CRGB::Red`
- Array-based control and LED mapping
- XY coordinate mapping for matrix layouts
- Basic drawing primitives for LED strips and grids

### [Intermediate Techniques](intermediate/)
Level up with more advanced programming techniques.
- [Color Palettes](intermediate/palettes.md)
- [Noise and Perlin Noise](intermediate/noise.md)
- [Blending and Transitions](intermediate/blending.md)
- [Math and Mapping Functions](intermediate/math.md)

### [Advanced Effects](advanced/)
Create professional-grade LED installations and effects.
- [2D/Matrix Operations](advanced/matrix.md)
- [Multiple Strip Coordination](advanced/multi-strip.md)
- [Performance Optimization](advanced/optimization.md)

### [Common Recipes](recipes/)
Copy-paste ready recipes for popular LED effects.
- [Fire Effect](recipes/fire.md)
- [Twinkle/Sparkle](recipes/twinkle.md)
- [Breathing Effect](recipes/breathing.md)
- [Wave Patterns](recipes/waves.md)

### [Troubleshooting](troubleshooting/)
Solve common problems and debug your LED setup.
- [Common Issues and Solutions](troubleshooting/common-issues.md)
- [Debugging Tips](troubleshooting/debugging.md)
- [Hardware Problems](troubleshooting/hardware.md)

### [Reference](reference/)
Quick reference guides and configuration tables.
- [API Quick Reference](reference/api.md)
- [Pin Configurations by Platform](reference/pins.md)
- [Supported LED Types](reference/led-types.md)

---

## Additional Resources

- FastLED Documentation: https://fastled.io
- GitHub Repository: https://github.com/FastLED/FastLED
- Community Wiki: https://github.com/FastLED/FastLED/wiki

---

**Note**: This cookbook is a living document. Contributions welcome!
