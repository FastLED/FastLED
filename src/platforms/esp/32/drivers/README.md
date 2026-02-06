# ESP32 Hardware Drivers

This directory contains centralized hardware driver implementations for ESP32 platforms.

## Architecture

**Drivers are bonafide peripheral abstractions** that encapsulate ESP-IDF hardware APIs and provide:
- Clean, object-oriented interfaces
- Automatic initialization and fallback handling
- Thread-safe operations (leveraging ESP-IDF internal mutexes)
- Platform-independent usage patterns

## Available Drivers

### Console UART Driver (`uart_esp32.h` / `uart_esp32.cpp.hpp`)

**Purpose:** Buffered serial console I/O using ESP-IDF UART peripheral

**⚠️ Note:** This is separate from the LED UART driver (`uart/` subdirectory) which uses UART for WS2812 LED control.

**Features:**
- Automatic driver installation with Arduino compatibility detection
- Buffered TX/RX (256 bytes each, Arduino default)
- Fallback to ROM UART if driver installation fails
- Non-blocking read operations (timeout=0)
- Thread-safe (ESP-IDF provides internal mutex)

**Usage:**
```cpp
#include "platforms/esp/32/drivers/uart_esp32.h"

fl::UartEsp32 uart(UART_NUM_0);  // Console UART

// Output
uart.write("Hello, world!");
uart.writeln("With newline");

// Input
int available = uart.available();  // Bytes in RX buffer
int byte = uart.read();            // Read single byte (-1 if no data)

// Status
bool buffered = uart.isBuffered(); // true if driver installed, false if ROM fallback
```

**Architecture:**
- **Buffered mode:** Uses `uart_driver_install()` with RX/TX ring buffers
- **Fallback mode:** Uses ROM UART (`esp_rom_output_tx_one_char`) for output only
- **Initialization:** Lazy (on first use via Singleton pattern)
- **Detection:** Tests if Arduino or other code already installed driver

**Files:**
- `uart_esp32.h` - Driver interface
- `uart_esp32.cpp.hpp` - Driver implementation

**Used by:**
- `io_esp_idf.hpp` - EspIO class delegates all I/O to UartEsp32
- External API: `fl::print()`, `fl::println()`, `fl::available()`, `fl::read()`

### LED UART Driver (`uart/` subdirectory)

**Purpose:** WS2812 LED control using UART waveform generation

**Architecture:**
- Uses UART's automatic start/stop bit insertion for LED protocol encoding
- 2-bit LUT encoding (4:1 expansion vs 8:1 in traditional wave8)
- DMA-based transmission with minimal CPU usage
- Supports multiple strips via multiple UART peripherals (UART1, UART2)

**Key difference from console UART:**
- Console UART: Text I/O at 115200 baud (UART0)
- LED UART: Waveform generation at 3.2 Mbps (UART1/UART2)
- These can coexist - console uses UART0, LEDs use UART1/UART2

**Files:**
- `uart/iuart_peripheral.h` - Virtual interface for testability
- `uart/uart_peripheral_esp.h` - Real hardware implementation
- `uart/channel_engine_uart.h` - LED control engine

**Documentation:** See `uart/README.md` for complete LED UART documentation

## Design Principles

### 1. Separation of Concerns
- **Drivers:** Hardware peripheral management (UART, SPI, I2C, etc.)
- **I/O Layer:** Platform abstraction (`io_esp_idf.hpp` uses drivers)
- **Application:** Uses platform-independent API (`fl::print()`, etc.)

### 2. Encapsulation
- All ESP-IDF API calls live in driver implementations
- Public interfaces expose only high-level operations
- Internal state (buffers, config) is private

### 3. Reusability
- Drivers can be used independently of EspIO
- Multiple instances supported (e.g., UART1, UART2)
- Clean interfaces enable testing and mocking

### 4. Safety
- Automatic fallback handling (e.g., ROM UART if driver fails)
- Null pointer checks and bounds validation
- Thread-safe by leveraging ESP-IDF mutexes

## Adding New Drivers

When adding a new driver (SPI, I2C, Timer, etc.):

1. **Create driver files:**
   - `<peripheral>_esp32.h` - Driver interface
   - `<peripheral>_esp32.cpp.hpp` - Driver implementation

2. **Follow naming conventions:**
   - Class: `<Peripheral>Esp32` (e.g., `SpiEsp32`, `I2cEsp32`)
   - Namespace: `fl::`
   - Methods: Use standard names (e.g., `read()`, `write()`, `transfer()`)

3. **Include proper headers:**
   - Use `FL_EXTERN_C_BEGIN/END` for ESP-IDF C headers
   - Include `fl/compiler_control.h` for macros
   - Include `platforms/esp/esp_version.h` for version detection

4. **Document:**
   - Add architecture notes (buffered vs. unbuffered, fallback behavior)
   - Document thread safety guarantees
   - Provide usage examples

5. **Update this README:**
   - Add driver to "Available Drivers" section
   - Document dependencies and usage patterns

## Version Compatibility

All drivers handle ESP-IDF version differences:
- IDF 3.x - Legacy API (no source_clk field)
- IDF 4.x - UART_SCLK_APB clock source
- IDF 5.x+ - UART_SCLK_DEFAULT clock source

Use `ESP_IDF_VERSION` macros for version-dependent code:
```cpp
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    uart_config.source_clk = UART_SCLK_DEFAULT;
#elif ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
    uart_config.source_clk = UART_SCLK_APB;
#endif
// IDF 3.x: source_clk field doesn't exist - skip it
```

## Thread Safety

**All drivers leverage ESP-IDF's internal thread safety:**
- UART driver: Internal mutex protects all operations
- Safe to call from multiple FreeRTOS tasks
- No additional locking needed in driver code

**Example (UART):**
```cpp
// Task 1
uart.write("Task 1\n");

// Task 2 (concurrent)
uart.write("Task 2\n");

// Result: Both writes succeed, ESP-IDF serializes them internally
```

## Memory Management

**Drivers use minimal heap allocation:**
- UART: 256 bytes RX buffer + 256 bytes TX buffer = 512 bytes total
- Buffers allocated once during driver installation
- No per-operation allocations (zero-copy when possible)

**Stack usage:**
- Drivers use stack-local variables for temporary operations
- Typical stack usage: < 256 bytes per operation

## Testing

**Drivers should be testable in isolation:**
- Create mock ESP-IDF APIs for unit testing
- Use dependency injection where possible
- Provide status queries (`isInitialized()`, `isBuffered()`, etc.)

**Integration testing:**
- Use hardware-in-the-loop validation (`bash validate`)
- Test both buffered and fallback modes
- Verify thread safety with concurrent operations

## References

- **ESP-IDF UART Documentation:** https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/uart.html
- **FastLED I/O Abstraction:** `src/fl/stl/cstdio.h`
- **Platform I/O Layer:** `src/platforms/esp/io_esp.cpp.hpp`
