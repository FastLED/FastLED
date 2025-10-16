# FastLED Teensy Platform Support

## Overview

This directory contains platform-specific support for Teensy microcontrollers across multiple generations.

## Platform Structure

- `common/`: Shared utilities across all Teensy boards
- `teensy3_common/`: Shared code for Teensy 3.x family
- `teensy4_common/`: Shared code for Teensy 4.x family
- Board-specific directories:
  - `teensy_lc/`: Teensy LC (Cortex-M0+)
  - `teensy30/`: Teensy 3.0
  - `teensy31_32/`: Teensy 3.1 and 3.2
  - `teensy35/`: Teensy 3.5
  - `teensy36/`: Teensy 3.6
  - `teensy40/`: Teensy 4.0
  - `teensy41/`: Teensy 4.1

## Migration Notes

This structure replaces the previous flat organization under `src/platforms/arm/`.

- Old paths like `src/platforms/arm/k20/` are deprecated
- Compatibility headers are provided to maintain backward compatibility
- Future Teensy boards can be easily added by creating new directories

## Compatibility

- All existing Teensy board support is preserved
- Build system and examples should work without modification
- Compatibility headers ensure existing includes continue to work

## Contributing

When adding support for new Teensy boards:
1. Create a new board-specific directory under the appropriate generation
2. Add board-specific headers and implementations
3. Update common headers if necessary
4. Ensure backward compatibility is maintained
