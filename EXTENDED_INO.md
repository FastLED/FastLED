# Extended INO Format Specification

## Overview

Extended INO is a format that allows Arduino sketch files (`.ino`) to embed build configuration metadata directly within the source code, similar to Astral UV's inlined script dependencies. This enables self-contained sketches that carry their own build requirements, making them more portable and easier to share.

## Motivation

Traditional Arduino development requires external configuration files (`platformio.ini`, `arduino-cli.yaml`) or IDE-specific settings to specify build parameters. Extended INO embeds this configuration directly in the sketch file, providing:

- **Portability**: Sketches carry their build requirements
- **Self-Documentation**: Build configuration is visible alongside code
- **Reproducibility**: Consistent builds across different environments
- **Simplicity**: Single file contains both code and configuration

## Format Specification

### Basic Structure

Extended INO files use a comment block with TOML configuration embedded between delimiter markers:

```cpp
// /// extended-ino
// [build]
// board = "arduino:avr:uno"
// 
// [flags]
// build = ["-DDEBUG=1", "-O2"]
// link = ["-Wl,--gc-sections"]
// 
// [defines]
// DEBUG = "1"
// VERSION = "\"1.0.0\""
// ///

#include <FastLED.h>

void setup() {
    // Your Arduino code here
}

void loop() {
    // Your Arduino code here
}
```

### Delimiter Rules

- Start delimiter: `// /// extended-ino`
- End delimiter: `// ///`
- All configuration lines must be prefixed with `// ` (comment prefix + space)
- The content between delimiters must be valid TOML

### Required Fields

#### `[build]` Section

- `board` (string): Board Fully Qualified Name (FQBN)
  - Format: `package:architecture:board_id`
  - Example: `"arduino:avr:uno"`, `"esp32:esp32:esp32"`

### Optional Fields

#### `[flags]` Section

- `build` (array of strings): Compiler flags passed during compilation
- `link` (array of strings): Linker flags passed during linking
- `upload` (array of strings): Upload-specific flags

#### `[defines]` Section

Key-value pairs of preprocessor definitions:
- Keys become `#define` names
- Values become `#define` values
- String values should be quoted appropriately for C/C++

#### `[dependencies]` Section

- `libraries` (array of strings): Library dependencies
- `cores` (array of strings): Core dependencies
- `tools` (array of strings): Tool dependencies

#### `[settings]` Section

- `cpu_frequency` (string): CPU frequency setting
- `flash_frequency` (string): Flash frequency setting  
- `upload_speed` (integer): Upload baud rate
- `partition_scheme` (string): Flash partition scheme

## Multi-Board Support

Extended INO supports multiple board configurations using TOML table arrays:

```cpp
// /// extended-ino
// [[board]]
// fqbn = "arduino:avr:uno"
// 
// [board.flags]
// build = ["-DARDUINO_UNO=1"]
// 
// [board.defines]
// BOARD_TYPE = "\"UNO\""
// 
// [[board]]
// fqbn = "esp32:esp32:esp32"
// 
// [board.flags]
// build = ["-DARDUINO_ESP32=1"]
// 
// [board.defines]
// BOARD_TYPE = "\"ESP32\""
// ///
```

## Examples

### Simple Example

```cpp
// /// extended-ino
// [build]
// board = "arduino:avr:uno"
// 
// [defines]
// LED_PIN = "13"
// BAUD_RATE = "9600"
// ///

void setup() {
    pinMode(LED_PIN, OUTPUT);
    Serial.begin(BAUD_RATE);
}

void loop() {
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
    digitalWrite(LED_PIN, LOW);
    delay(1000);
}
```

### Advanced Example

```cpp
// /// extended-ino
// [build]
// board = "esp32:esp32:esp32"
// 
// [flags]
// build = [
//     "-DCORE_DEBUG_LEVEL=4",
//     "-DARDUINO_RUNNING_CORE=1",
//     "-DARDUINO_EVENT_RUNNING_CORE=1"
// ]
// link = ["-Wl,--gc-sections"]
// 
// [defines]
// WIFI_SSID = "\"MyNetwork\""
// WIFI_PASSWORD = "\"MyPassword\""
// DEBUG_ENABLED = "1"
// 
// [dependencies]
// libraries = ["WiFi", "WebServer", "ArduinoJson@>=6.0.0"]
// 
// [settings]
// cpu_frequency = "240MHz"
// flash_frequency = "80MHz"
// upload_speed = 921600
// partition_scheme = "default"
// ///

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

void setup() {
    Serial.begin(115200);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
    // Application code
}
```

### Multi-Board Example

```cpp
// /// extended-ino
// [[board]]
// fqbn = "arduino:avr:uno"
// 
// [board.defines]
// LED_COUNT = "50"
// DATA_PIN = "6"
// 
// [board.flags]
// build = ["-DAVR_BOARD=1"]
// 
// [[board]]
// fqbn = "esp32:esp32:esp32"
// 
// [board.defines]
// LED_COUNT = "100"
// DATA_PIN = "2"
// 
// [board.flags]
// build = ["-DESP32_BOARD=1"]
// 
// [board.settings]
// cpu_frequency = "240MHz"
// ///

#include <FastLED.h>

CRGB leds[LED_COUNT];

void setup() {
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, LED_COUNT);
}

void loop() {
    // LED animation code
}
```

## Implementation Considerations

### Parsing

1. **Comment Extraction**: Parse comment blocks between delimiters
2. **TOML Validation**: Validate embedded TOML syntax
3. **Field Validation**: Ensure required fields are present
4. **Board Resolution**: Validate FQBN format and availability

### Integration Points

Extended INO can integrate with:

- **Arduino CLI**: Generate temporary configuration files
- **PlatformIO**: Convert to `platformio.ini` format
- **Arduino IDE 2.x**: Plugin/extension support
- **VS Code**: Language server extensions

### Error Handling

- **Invalid TOML**: Clear syntax error messages with line numbers
- **Missing Required Fields**: Specify which fields are missing
- **Invalid FQBN**: Validate board availability
- **Conflicting Configurations**: Handle multi-board conflicts

### Backward Compatibility

- Files without Extended INO blocks remain valid Arduino sketches
- Existing build systems can ignore the comment blocks
- Gradual adoption possible

## Future Considerations

### Planned Extensions

1. **Variable Substitution**: Environment variable and computed value support
2. **Conditional Configuration**: Platform-specific sections
3. **Include Support**: Reference external configuration fragments
4. **Version Constraints**: Semantic versioning for dependencies
5. **Custom Build Steps**: Pre/post-build actions

### Example Future Syntax

```cpp
// /// extended-ino
// [build]
// board = "${BOARD_FQBN:-arduino:avr:uno}"
// 
// [conditional.esp32]
// condition = "platform == 'esp32'"
// 
// [conditional.esp32.defines]
// WIFI_ENABLED = "1"
// 
// [includes]
// common = "./common-config.toml"
// ///
```

## Tooling Support

### Required Tools

- **TOML Parser**: For configuration extraction and validation
- **Board Database**: FQBN validation and resolution
- **Build System Integration**: Convert to native build formats

### Recommended Workflow

1. **Parse**: Extract and validate Extended INO configuration
2. **Resolve**: Validate board FQBN and dependencies
3. **Generate**: Create appropriate build system files
4. **Build**: Execute standard Arduino build process
5. **Clean**: Remove temporary configuration files

## Conclusion

Extended INO provides a self-contained format for Arduino sketches that embeds build configuration alongside source code. This improves portability, reproducibility, and ease of sharing while maintaining backward compatibility with existing Arduino development workflows.

The format is designed to be:
- **Human-readable**: TOML syntax is clear and well-documented
- **Tool-friendly**: Structured format enables automated processing
- **Extensible**: Future features can be added without breaking changes
- **Compatible**: Works with existing Arduino ecosystems