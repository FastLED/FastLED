# FastLED Corkscrew API Documentation

## Overview

*From corkscrew.h header documentation:*

Provides math to project a cork screw onto a rectangular surface. This allows drawing into a normal rectangle, and then have it mapped correctly to the corkscrew.

**Construction:** Take a stick and wrap as dense as possible. Measure the number of turns. That's it.

Corkscrew will provide you a width and height for the rectangular grid. There are constexpr functions to calculate the width and height so you can statically allocate the CRGB buffer to draw into.

## Core Theory

*From example header documentation:*

This is forward mapping, in which we test that the corkscrew is mapped to cylinder cartesian coordinates.

Most of the time, you'll want the reverse mapping, that is drawing to a rectangular grid, and then mapping that to a corkscrew.

However, to make sure the above mapping works correctly, we have to test that the forward mapping works correctly first.

## API Structure

### Input Parameters

**Inputs:**
- Total Height of the Corkscrew in centimeters
- Total angle of the Corkscrew (number of vertical segments × 2π)
- Optional offset circumference (default 0)
  - Allows pixel-perfect corkscrew with gaps via circumference offsetting
  - Example (accounting for gaps):
    - segment 0: offset circumference = 0, circumference = 100
    - segment 1: offset circumference = 100.5, circumference = 100
    - segment 2: offset circumference = 101, circumference = 100

**Outputs:**
- Width and Height of the cylindrical map
  - Width is the circumference of one turn
  - Height is the total number of vertical segments
- Vector of vec2f {width, height} mapping corkscrew (r,c) to cylindrical {w,h}

### CorkscrewInput Structure

```cpp
struct CorkscrewInput {
    float totalTurns = 19.f;        // Default to 19 turns
    float offsetCircumference = 0;   // Optional offset for gap accounting
    uint16_t numLeds = 144;         // Default to dense 144 leds
    bool invert = false;            // If true, reverse the mapping order
}
```

**Purpose:** Defines the input parameters for constructing a corkscrew mapping.

**Methods:**
- `calculateWidth()` - Calculate optimal width based on number of turns and LEDs (Width = LEDs per turn)
- `calculateHeight()` - Calculate optimal height to minimize empty pixels

### CorkscrewState Structure

```cpp
struct CorkscrewState {
    uint16_t width = 0;   // Width of cylindrical map (circumference of one turn)
    uint16_t height = 0;  // Height of cylindrical map (total vertical segments)
}
```

**Purpose:** Contains the computed dimensions of the cylindrical mapping.

**Features:**
- Includes iterator support for traversing corkscrew positions
- Positions now computed on-the-fly (removed mapping vector)

### Main Corkscrew Class

**Purpose:** Maps a Corkscrew defined by the input to a cylindrical mapping for rendering a densely wrapped LED corkscrew.

#### Core Mapping Methods

- `at_no_wrap(uint16_t i)` - Returns unwrapped coordinates (can exceed width)
- `at_exact(uint16_t i)` - Returns wrapped coordinates (x wrapped to [0, width))
- `at_wrap(float i)` - Returns sub-pixel tiles for smooth rendering (future API)

#### Dimension Access

- `cylinder_width()` - Returns width of cylindrical map
- `cylinder_height()` - Returns height of cylindrical map
- `size()` - Returns total number of positions

#### Buffer Management

**New Rectangular Buffer Support:**
*From example header:* You can now draw into a rectangular fl::Leds grid and read that into the corkscrew's internal buffer for display.

- `getBuffer()` - Get access to the rectangular buffer (lazily initialized)
- `data()` - Get raw CRGB* access to the buffer (lazily initialized)
- `clearBuffer()` - Clear the rectangular buffer
- `fillBuffer(const CRGB& color)` - Fill the rectangular buffer with a color

#### Grid Integration

- `readFrom(const fl::Grid<CRGB>& source_grid, bool use_multi_sampling = true)` - Read from fl::Grid<CRGB> object and populate internal rectangular buffer by sampling from the XY coordinates mapped to each corkscrew LED position
  - `use_multi_sampling = true` will use multi-sampling to sample from the source grid, this will give a little bit better accuracy and the screenmap will be more accurate

## New Features

### ScreenMap Support

*From example header documentation:*

**NEW: ScreenMap Support**
You can now create a ScreenMap directly from a Corkscrew, which maps each LED index to its exact position on the cylindrical surface. This is useful for web interfaces and visualization.

**API Method:**
- `toScreenMap(float diameter = 0.5f)` - Create and return a fully constructed ScreenMap for this corkscrew. Each LED index will be mapped to its exact position on the cylindrical surface.

### Rectangular Buffer Support

*From example header documentation:*

**NEW: Rectangular Buffer Support**
You can now draw into a rectangular fl::Leds grid and read that into the corkscrew's internal buffer for display.

**Key Benefits:**
- Draw 2D patterns into rectangular grid
- Automatic mapping to corkscrew LED positions
- Support for multi-sampling for improved accuracy
- Lazy buffer initialization (no memory overhead until first use)

## Compile-Time Optimization

### Constexpr Functions

**Purpose:** Simple constexpr functions for compile-time corkscrew dimension calculation.

- `calculateCorkscrewWidth(float totalTurns, uint16_t numLeds)` - Calculate width at compile time
- `calculateCorkscrewHeight(float totalTurns, uint16_t numLeds)` - Calculate height at compile time

**Benefits:**
- Static memory allocation
- Compile-time dimension calculation
- No runtime overhead for dimension computation

## Internal Implementation Details

### Private Methods

- `at_splat_extrapolate(float i)` - For internal use. Splats the pixel on the surface which extends past the width. This extended Tile2x2 is designed to be wrapped around with a Tile2x2_u8_wrap.
- `readFromMulti(const fl::Grid<CRGB>& target_grid)` - Read from rectangular buffer using multi-sampling and store in target grid. Uses Tile2x2_u8_wrap for sub-pixel accurate sampling with proper blending.
- `initializeBuffer()` - Initialize the rectangular buffer if not already done.

### State Management

- Input parameters are stored in `mInput`
- Computed state is stored in `mState`
- Rectangular buffer `mCorkscrewLeds` is lazily initialized
- Buffer initialization tracked with `mBufferInitialized` flag

## Iterator Support

The CorkscrewState includes full iterator support for STL-style traversal:

- Forward/backward iteration
- Random access capabilities
- Distance calculation between iterators
- Standard iterator traits (value_type, difference_type, etc.)

## Usage Patterns

### Forward Mapping
Testing that the corkscrew is mapped to cylinder cartesian coordinates correctly.

### Reverse Mapping (Primary Use Case)
Drawing to a rectangular grid, and then mapping that to a corkscrew for display.

### Web Visualization
Using ScreenMap integration for accurate web interface display of corkscrew LED arrangements.

### Multi-Sampling
Enhanced accuracy through sub-pixel sampling when mapping from rectangular grids to corkscrew positions.