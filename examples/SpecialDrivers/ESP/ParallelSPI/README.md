# ESP32 Parallel SPI Examples

This directory contains examples for ESP32 software-based parallel SPI implementations using GPIO bit-banging.

## 🎯 Overview

FastLED provides two software SPI implementation strategies:
- **Blocking SPI**: Inline bit-banging on main thread (simpler, lower overhead)
- **ISR-based SPI**: Async bit-banging via timer interrupts (non-blocking)

Both use identical bit-banging logic - only the execution context differs.

## 📁 Available Examples

### Blocking SPI Examples (Simple, Inline Execution)

| Example | Width | Description |
|---------|-------|-------------|
| `Esp32C3_SingleSPI_Blocking` | 1-way | Single data pin, inline bit-banging |
| `Esp32C3_DualSPI_Blocking` | 2-way | Two data pins in parallel |
| `Esp32C3_QuadSPI_Blocking` | 4-way | Four data pins in parallel |

**Key Features:**
- Simple API (just 3 methods: `setPinMapping`, `loadBuffer`, `transmit`)
- Blocks until transmission complete
- Lower overhead (no ISR setup/teardown)
- Better timing precision (no interrupt jitter)

### ISR-Based SPI Examples (Async, Non-Blocking)

| Example | Width | Description |
|---------|-------|-------------|
| `Esp32C3_SingleSPI_ISR` | 1-way | Single data pin via timer ISR |
| `Esp32C3_DualSPI_ISR` | 2-way | Two data pins via timer ISR |
| `Esp32C3_QuadSPI_ISR` | 4-way | Four data pins via timer ISR |
| `Esp32C3_SPI_ISR` | 8-way | Eight data pins via timer ISR |

**Key Features:**
- Non-blocking operation (main thread remains responsive)
- Timer ISR drives bit-banging at ~1.6MHz
- Good for complex applications with multiple tasks

## 🚀 Quick Start

### Using Blocking SPI (Recommended for Simple Applications)

```cpp
#include "platforms/esp/32/parallel_spi/parallel_spi_blocking_quad.hpp"

QuadSPI_Blocking_ESP32 spi;

void setup() {
    // Configure pins (D0-D3 + CLK)
    spi.setPinMapping(0, 1, 2, 3, 8);

    // Prepare data
    uint8_t data[4] = {0x0F, 0x0A, 0x05, 0x00};
    spi.loadBuffer(data, 4);

    // Transmit (blocks until done)
    spi.transmit();
}
```

### Using ISR-Based SPI (For Complex Applications)

```cpp
#include "platforms/esp/32/parallel_spi/parallel_spi_isr_quad_esp32c3.hpp"

QuadSPI_ISR_ESP32C3 spi;

void setup() {
    // Configure pins
    spi.setPinMapping(0, 1, 2, 3, 8);

    // Setup ISR
    spi.setupISR(1600000);  // 1.6MHz timer

    // Prepare and send data
    uint8_t data[4] = {0x0F, 0x0A, 0x05, 0x00};
    spi.loadBuffer(data, 4);

    QuadSPI_ISR_ESP32C3::visibilityDelayUs(10);
    spi.arm();

    // Wait for completion
    while (spi.isBusy()) {
        delay(1);
    }

    spi.ackDone();
    spi.stopISR();
}
```

## 🎓 Choosing Between Blocking and ISR

### Use Blocking SPI When:
✅ Simple LED update patterns
✅ Blocking during transmission is acceptable
✅ Lower overhead needed
✅ Better timing precision required
✅ Simpler code preferred

### Use ISR-Based SPI When:
✅ Non-blocking operation needed
✅ Main thread must remain responsive
✅ Complex application with multiple tasks
✅ Async operation preferred

## 🔧 Hardware Requirements

### Supported Platforms
- ESP32-C2
- ESP32-C3
- ESP32-C6
- ESP32-H2

### Pin Selection
Both implementations use software bit-banging, so any GPIO can be used:
- **Avoid GPIO 6-11** (reserved for flash on most modules)
- Any available GPIO works for data/clock pins
- No hardware peripheral restrictions

### Example Pin Configurations

**Single-SPI (1-way):**
```cpp
setPinMapping(0, 8);  // D0=GPIO0, CLK=GPIO8
```

**Dual-SPI (2-way):**
```cpp
setPinMapping(0, 1, 8);  // D0=GPIO0, D1=GPIO1, CLK=GPIO8
```

**Quad-SPI (4-way):**
```cpp
setPinMapping(0, 1, 2, 3, 8);  // D0-D3=GPIO0-3, CLK=GPIO8
```

## 📊 Performance Comparison

| Characteristic | Blocking | ISR-Based |
|----------------|----------|-----------|
| **Overhead** | None (inline) | ISR context switch |
| **Latency** | Zero (immediate) | ISR entry delay |
| **Jitter** | None | Interrupt scheduling |
| **Main Thread** | Blocks | Non-blocking |
| **API Complexity** | 3 methods | 7+ methods |
| **Bit Rate** | Higher potential | ~800kHz |

## 🧪 Testing

All examples can be compiled and tested:

```bash
# Compile blocking examples
uv run ci/ci-compile.py esp32c3 --examples Esp32C3_SingleSPI_Blocking
uv run ci/ci-compile.py esp32c3 --examples Esp32C3_DualSPI_Blocking
uv run ci/ci-compile.py esp32c3 --examples Esp32C3_QuadSPI_Blocking

# Compile ISR examples
uv run ci/ci-compile.py esp32c3 --examples Esp32C3_SingleSPI_ISR
uv run ci/ci-compile.py esp32c3 --examples Esp32C3_DualSPI_ISR
uv run ci/ci-compile.py esp32c3 --examples Esp32C3_QuadSPI_ISR
```

## 📚 Additional Documentation

- **Architecture Details**: `src/platforms/esp/32/parallel_spi/README.md`
- **Implementation Loop**: `LOOP.md` (project root)
- **Testing Guide**: `tests/AGENTS.md`

## 💡 Tips

1. **Start Simple**: Try the blocking single-SPI example first
2. **Test Patterns**: All examples include test patterns to verify correct GPIO output
3. **Timing Measurement**: Examples measure and display transmission timing
4. **Pin Validation**: Check serial output for pin configuration confirmation

## 🤝 Contributing

When creating new examples:
1. Follow existing example structure
2. Include pin configuration display
3. Add timing measurements
4. Test on real hardware when possible
5. Document any platform-specific behavior

---

**Last Updated**: 2025-10-13
**Status**: Active Development
