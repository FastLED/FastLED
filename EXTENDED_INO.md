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
- All relative paths in configuration are resolved relative to the sketch file's directory

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

- `libraries` (array of strings): Library dependencies with optional version constraints
- `additional_urls` (array of strings): Board Manager package index URLs for custom boards
- `cores` (array of strings): Core dependencies
- `tools` (array of strings): Tool dependencies

#### `[settings]` Section

- `cpu_frequency` (string): CPU frequency setting
- `flash_frequency` (string): Flash frequency setting  
- `upload_speed` (integer): Upload baud rate
- `partition_scheme` (string): Flash partition scheme

#### `[cli]` Section (Essential Arduino CLI Settings)

**Required for Build Process:**
- `directories_user` (string): Arduino sketchbook directory for libraries and hardware
- `directories_data` (string): Arduino CLI data directory for core installations
- `export_binaries` (boolean): Always save compiled binaries to sketch folder

**Required for Custom Boards:**
- `board_manager_additional_urls` (array of strings): URLs for additional board package indexes
- `enable_unsafe_library_install` (boolean): Allow installation from git URLs or zip files

#### `[cli_optional]` Section (Nice-to-Have Arduino CLI Settings)

**Build Performance:**
- `build_cache_path` (string): Custom location for build cache files
- `build_cache_extra_paths` (array of strings): Additional directories to include in build cache
- `build_cache_ttl` (string): Time-to-live for cached compilation units (e.g., "720h")

**Development Experience:**
- `logging_level` (string): Log verbosity level ("trace", "debug", "info", "warn", "error", "fatal")
- `logging_file` (string): Log file path for persistent logging
- `output_no_color` (boolean): Disable colored output for terminals that don't support it
- `locale` (string): Language setting for Arduino CLI messages

**Network Configuration:**
- `network_proxy` (string): HTTP/HTTPS proxy server URL
- `connection_timeout` (string): Network timeout duration (e.g., "10s")

**Advanced Features:**
- `daemon_port` (string): gRPC port for Arduino CLI daemon mode
- `metrics_enabled` (boolean): Enable telemetry collection
- `updater_enable_notification` (boolean): Check for Arduino CLI updates

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
// additional_urls = ["https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"]
// 
// [settings]
// cpu_frequency = "240MHz"
// flash_frequency = "80MHz"
// upload_speed = 921600
// partition_scheme = "default"
// 
// [cli]
// directories_user = "/home/user/Arduino"
// export_binaries = true
// board_manager_additional_urls = ["https://adafruit.github.io/arduino-board-index/package_adafruit_index.json"]
// 
// [cli_optional]
// build_cache_ttl = "720h"
// logging_level = "info"
// output_no_color = false
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

### Arduino CLI Configuration Example

```cpp
// /// extended-ino
// [build]
// board = "esp32:esp32:esp32dev"
// 
// [dependencies]
// libraries = ["FastLED@>=3.6.0", "WiFiManager", "ArduinoJson@^6.21.0"]
// additional_urls = [
//     "https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json",
//     "https://adafruit.github.io/arduino-board-index/package_adafruit_index.json"
// ]
// 
// [defines]
// FASTLED_INTERNAL = "1"
// LED_COUNT = "100"
// 
// [cli]
// directories_user = "./libraries"       # Relative to sketch directory
// directories_data = "./arduino-data"    # Relative to sketch directory  
// export_binaries = true
// enable_unsafe_library_install = true
// 
// [cli_optional]
// build_cache_path = "./build-cache"     # Relative to sketch directory
// build_cache_ttl = "168h"
// logging_level = "debug"
// logging_file = "./arduino-cli.log"     # Relative to sketch directory
// ///

#include <FastLED.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>

CRGB leds[LED_COUNT];

void setup() {
    FastLED.addLeds<WS2812B, 2, GRB>(leds, LED_COUNT);
    WiFiManager wifiManager;
    wifiManager.autoConnect("FastLED_Setup");
}

void loop() {
    // LED effects and WiFi communication
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

1. **Automatic Discovery**: Detect Extended INO configuration in any `.ino` file being processed (no path argument required)
2. **Comment Extraction**: Parse comment blocks between delimiters within the sketch file
3. **TOML Validation**: Validate embedded TOML syntax
4. **Field Validation**: Ensure required fields are present
5. **Board Resolution**: Validate FQBN format and availability
6. **Path Resolution**: Use sketch file directory as base path for all relative paths and generated files

### Integration Points

Extended INO can integrate with:

- **Arduino CLI**: Generate temporary `arduino-cli.yaml` configuration files from `[cli]` and `[cli_optional]` sections
- **PlatformIO**: Convert to `platformio.ini` format using board, flags, and dependencies
- **Arduino IDE 2.x**: Plugin/extension support for parsing and applying Extended INO configuration
- **VS Code**: Language server extensions with Arduino CLI integration

### Arduino CLI Configuration Generation

Extended INO tools can generate Arduino CLI configuration files from embedded metadata:

```yaml
# Generated arduino-cli.yaml from Extended INO [cli] section
board_manager:
  additional_urls:
    - "https://adafruit.github.io/arduino-board-index/package_adafruit_index.json"

directories:
  user: "./libraries"
  data: "./arduino-data"

sketch:
  always_export_binaries: true

library:
  enable_unsafe_install: true

build_cache:
  path: "./build-cache"
  ttl: "168h"

logging:
  level: "debug"
  file: "./arduino-cli.log"
```

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

## Tool Invocation

Extended INO tools operate directly on sketch files without requiring explicit path arguments:

```bash
# Tool discovers configuration from the sketch file automatically
extended-ino-build MySketch.ino

# Working directory becomes the sketch's directory
extended-ino-compile MyProject/MySketch.ino

# All generated files appear in the sketch's directory
extended-ino-upload ./examples/Blink/Blink.ino
```

**Key Behaviors:**
- **Implicit Path Discovery**: Tools automatically detect Extended INO configuration in the specified `.ino` file
- **Working Directory**: The sketch file's directory becomes the base path for all operations
- **Generated Files**: All temporary files (`arduino-cli.yaml`, `platformio.ini`, build cache) are created relative to the sketch location
- **No Path Arguments**: Tools never require separate path or directory arguments beyond the sketch file itself

## Tooling Support

### Required Tools

- **TOML Parser**: For configuration extraction and validation from sketch files
- **Board Database**: FQBN validation and resolution
- **Build System Integration**: Convert to native build formats using sketch file location as working directory

### Recommended Workflow

1. **Parse**: Extract and validate Extended INO configuration from comment block in the sketch file
2. **Resolve**: Validate board FQBN and check dependency availability  
3. **Configure**: Generate temporary `arduino-cli.yaml` from `[cli]` and `[cli_optional]` sections in the sketch directory
4. **Dependencies**: Install libraries and board packages as specified
5. **Generate**: Create appropriate build system files (platformio.ini, compile_commands.json) in the sketch directory
6. **Build**: Execute Arduino CLI build process with generated configuration using the sketch file path
7. **Export**: Save binaries to sketch directory if `export_binaries` is enabled
8. **Clean**: Remove temporary configuration files and build artifacts from sketch directory

## Conclusion

Extended INO provides a self-contained format for Arduino sketches that embeds build configuration alongside source code. This improves portability, reproducibility, and ease of sharing while maintaining backward compatibility with existing Arduino development workflows.

The format integrates comprehensive Arduino CLI configuration options, enabling complete control over the build environment, dependency management, and development experience through embedded TOML metadata.

The format is designed to be:
- **Human-readable**: TOML syntax is clear and well-documented
- **Tool-friendly**: Structured format enables automated processing and Arduino CLI integration
- **Extensible**: Future features can be added without breaking changes
- **Compatible**: Works with existing Arduino ecosystems and CLI tools
- **Comprehensive**: Covers essential build settings and optional developer experience enhancements
