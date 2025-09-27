#!/usr/bin/env python3
"""
ESP32-S3 LCD Parallel LED Driver Build Script
Custom build optimizations and configuration validation
"""

Import("env")

import os

def validate_configuration():
    """Validate build configuration for optimal performance"""
    print("=" * 60)
    print("ESP32-S3 LCD Parallel LED Driver - Build Configuration")
    print("=" * 60)
    
    # Check for required build flags
    build_flags = env.ParseFlags(env['BUILD_FLAGS'])
    defines = {flag[2:]: True for flag in build_flags['CPPDEFINES'] if flag.startswith('-D')}
    
    # Validate LCD configuration
    if 'LCD_PCLK_HZ' in defines:
        print("✓ LCD PCLK frequency configured")
    else:
        print("⚠ LCD PCLK frequency not set, using default 20MHz")
    
    # Validate PSRAM configuration  
    if 'CONFIG_SPIRAM_USE_MALLOC' in defines:
        print("✓ PSRAM malloc support enabled")
    else:
        print("⚠ PSRAM malloc support not configured")
    
    # Validate FastLED configuration
    if 'FASTLED_ESP32_I2S' in defines:
        print("✓ FastLED ESP32 I2S support enabled")
    else:
        print("⚠ FastLED ESP32 I2S support not configured")
    
    # Check optimization level
    optimization = env.get('BUILD_FLAGS', '')
    if '-O2' in optimization:
        print("✓ Optimization level: -O2 (Performance)")
    elif '-Os' in optimization:
        print("✓ Optimization level: -Os (Size)")
    elif '-O0' in optimization:
        print("⚠ Optimization level: -O0 (Debug)")
    else:
        print("⚠ Optimization level not specified")
    
    print("=" * 60)

def add_custom_flags():
    """Add custom compiler and linker flags for optimal performance"""
    
    # Add custom include paths if needed
    env.Append(CPPPATH=[
        "$PROJECT_SRC_DIR/platforms/esp/32"
    ])
    
    # Add custom defines for driver configuration
    env.Append(CPPDEFINES=[
        ("FASTLED_ESP32S3_LCD_DRIVER", "1"),
        ("CONFIG_LCD_ENABLE_DEBUG_LOG", "0")
    ])
    
    # Add linker flags for PSRAM optimization
    env.Append(LINKFLAGS=[
        "-Wl,--gc-sections",
        "-Wl,--cref"
    ])

# Run configuration validation
validate_configuration()

# Add custom build flags
add_custom_flags()

print("Build script configuration complete")