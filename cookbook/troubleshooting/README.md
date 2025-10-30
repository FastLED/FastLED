# Troubleshooting Guide

**Difficulty Level**: ⭐ Beginner
**Time to Complete**: 10-15 minutes (reading and understanding troubleshooting resources available)
**Prerequisites**:
- [Getting Started: First Example](../getting-started/first-example.md) - Basic FastLED setup
- [Getting Started: Hardware](../getting-started/hardware.md) - Hardware wiring basics

**You'll Learn**:
- How to navigate troubleshooting resources to quickly find solutions to common LED problems
- Using the quick troubleshooting checklist to verify basic setup before detailed debugging
- When to check common-issues.md for quick fixes vs debugging.md for systematic diagnosis
- Identifying hardware problems (wiring, power, signal integrity) vs software issues (code, configuration)
- Where to find help when problems persist beyond the troubleshooting guides

This section provides comprehensive troubleshooting resources for FastLED projects, covering common issues, debugging techniques, and hardware problems.

## Contents

### [Common Issues and Solutions](common-issues.md)
Quick fixes for the most frequently encountered problems:
- LEDs not lighting up
- Wrong colors displayed
- Flickering and instability
- First LED issues

### [Debugging Tips](debugging.md)
Techniques and code examples for debugging your LED projects:
- Serial monitoring and diagnostics
- Test patterns to verify setup
- Memory usage checking
- Color and frame rate debugging
- Data corruption validation

### [Hardware Problems](hardware.md)
In-depth guide to hardware-related issues:
- Voltage drop and power injection
- Wiring best practices
- Signal integrity issues
- Platform-specific problems (ESP32, etc.)
- Power supply selection
- Comprehensive hardware testing

## Quick Troubleshooting Checklist

Before diving into specific guides, verify these basics:

- [ ] Power supply voltage correct (5V for most strips)
- [ ] Power supply capacity sufficient (60mA per LED at full white)
- [ ] Data pin in code matches physical wiring
- [ ] LED type correct (WS2812B, APA102, etc.)
- [ ] COLOR_ORDER correct for your strip (try GRB if RGB doesn't work)
- [ ] Ground shared between microcontroller and LED strip
- [ ] `FastLED.show()` being called in your code
- [ ] Brightness not set to 0
- [ ] Strip not physically damaged

## Getting Help

If you're still stuck after reviewing these resources:

1. Double-check your wiring against the diagrams in [Hardware Problems](hardware.md)
2. Run the comprehensive hardware test in [Debugging Tips](debugging.md)
3. Check the [FastLED community forums](https://www.reddit.com/r/FastLED/)
4. Review the main [FastLED documentation](https://fastled.io)

## Related Resources

- [Main Cookbook](../COOKBOOK.md) - Complete FastLED guide
- [Getting Started](../COOKBOOK.md#getting-started) - Initial setup and basics
- [Reference](../COOKBOOK.md#reference) - API and platform configurations
