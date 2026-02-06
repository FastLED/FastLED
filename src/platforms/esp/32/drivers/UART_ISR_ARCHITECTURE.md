# ESP32 UART ISR/FreeRTOS Boundary Architecture

## Overview

This document explains the **real architecture** of ESP-IDF UART at the driver/ISR/FreeRTOS boundary—not Arduino-level hand-waving.

The ESP32 UART driver sits at the boundary between:
- **Hardware** (UART peripheral, FIFO, DMA)
- **ISR** (Interrupt Service Routine in IRAM or flash)
- **FreeRTOS** (Task scheduling, queues, critical sections)
- **Application** (Your code calling uart_read_bytes/uart_write_bytes)

---

## Critical Parameters

When you call:
```cpp
uart_driver_install(
    port,
    rx_buf_size,      // 1️⃣ RX ring buffer
    tx_buf_size,      // 2️⃣ TX ring buffer
    queue_size,       // 3️⃣ Event queue
    queue_handle,     // 4️⃣ Queue handle
    intr_flags        // 5️⃣ Interrupt flags (THE CRITICAL ONE)
);
```

Two parameters control ISR behavior:
- **Event queue** (3️⃣/4️⃣) - How ISR communicates with your task
- **Interrupt flags** (5️⃣) - How/when the ISR is allowed to run

---

## 1️⃣ Event Queue - ISR → Task Communication

### What It Actually Is

The **event queue** is a **FreeRTOS queue** that the UART **ISR posts messages into**.

Messages are of type `uart_event_t`:
```c
typedef enum {
    UART_DATA,        // RX data available
    UART_FIFO_OVF,    // Hardware FIFO overflow
    UART_BUFFER_FULL, // Driver ring buffer full
    UART_BREAK,       // Break detected
    UART_PARITY_ERR,  // Parity error
    UART_FRAME_ERR    // Frame error
} uart_event_type_t;

typedef struct {
    uart_event_type_t type;
    size_t size;
} uart_event_t;
```

### The Communication Flow

```
UART HW Interrupt
    ↓
UART ISR (ESP-IDF)
    ↓
xQueueSendFromISR(uart_event_queue)
    ↓
Your FreeRTOS Task receives uart_event_t
    ↓
xQueueReceive(uart_queue, &evt, timeout)
```

### What Happens When You Pass `0, NULL`

```cpp
0,    // No event queue
NULL, // No event queue handle
```

This means:
- ❌ **No UART events delivered to tasks**
- ❌ **No overflow/error notifications**
- ❌ **No "data arrived" events**

But importantly:
- ✅ **RX/TX still works** (ring buffers still function)
- ✅ **`uart_read_bytes()` still works** (polling mode)
- ✅ **Lower ISR overhead** (no queue operations)

You are saying:
> "I will poll or block on reads myself. Don't notify me."

### When You SHOULD Use Event Queue

Use it when:
- ✅ Event-driven RX task (react to data arrival)
- ✅ Need overflow diagnostics
- ✅ Want to react to framing/parity errors
- ✅ Building a modem/AT command parser

Typical pattern:
```c
QueueHandle_t uart_queue;
uart_driver_install(UART_NUM_0, 256, 256, 10, &uart_queue, flags);

uart_event_t evt;
while (1) {
    if (xQueueReceive(uart_queue, &evt, portMAX_DELAY)) {
        switch (evt.type) {
            case UART_DATA:
                // Read evt.size bytes
                break;
            case UART_FIFO_OVF:
                // Handle overflow
                break;
        }
    }
}
```

### When You Should NOT Use It (Our Case)

Don't use it when:
- ✅ You already own the scheduling (FastLED loop)
- ✅ Tight loops/DMA draining
- ✅ Don't want ISR → queue latency
- ✅ Want minimal ISR overhead (debug UART)

**For debug/console UART: Skipping the queue is correct.**

---

## 2️⃣ Interrupt Flags - THE CRITICAL PARAMETER

The last parameter controls **how the UART ISR is registered** with the ESP32 interrupt controller:

```cpp
intr_flags  // How survivable your ISR is under load
```

This directly affects:
- ✅ ISR priority (can it preempt other interrupts?)
- ✅ Whether it runs when flash cache is disabled
- ✅ Whether it runs during Wi-Fi critical sections

### The Fragile Default (DON'T USE)

```cpp
0  // Default = ESP_INTR_FLAG_LEVEL1
```

This means:
- ❌ **Lowest priority** (can be masked by everything)
- ❌ **Cannot run when flash cache is disabled** (ISR code in flash)
- ❌ **Subject to Wi-Fi/BLE interrupt blocking**
- ❌ **Can lose data during SPI flash operations**

**Result:** Your debug prints vanish when you need them most.

### ESP32 Reality Check

ESP32 does:
- 🔥 Cache disables during flash writes
- 🔥 Long Wi-Fi critical sections (>10ms)
- 🔥 Flash reads in foreground tasks
- 🔥 FreeRTOS scheduler stalls

Without proper ISR configuration:
- ❌ FIFO overflow
- ❌ "Mysterious UART gaps"
- ❌ Debug prints during crash = nothing
- ❌ Corrupted binary streams

### The Robust Configuration (USE THIS)

```cpp
ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3
```

#### ESP_INTR_FLAG_IRAM (0x400)
**Purpose:** Place ISR in IRAM (internal RAM)

**Effect:**
- ✅ ISR runs **even when flash cache is disabled**
- ✅ No gaps during SPI flash operations
- ✅ Debug prints work during OTA updates
- ✅ Essential for "never drop bytes" designs

**Without IRAM:**
```
[Your code triggers flash write]
    ↓
Flash cache disabled
    ↓
UART interrupt fires
    ↓
ISR code in flash → CANNOT EXECUTE
    ↓
UART FIFO overflows → BYTES LOST
```

**With IRAM:**
```
[Your code triggers flash write]
    ↓
Flash cache disabled
    ↓
UART interrupt fires
    ↓
ISR code in IRAM → EXECUTES NORMALLY
    ↓
Bytes safely moved to ring buffer
```

#### ESP_INTR_FLAG_LEVEL1-5
**Purpose:** Set ISR scheduling priority

| Level   | Priority | Use Case                    |
|---------|----------|-----------------------------|
| LEVEL1  | Lowest   | ❌ Can be masked (fragile)  |
| LEVEL2  | Low      | Background tasks            |
| LEVEL3  | Medium   | ✅ **Debug/console UART**   |
| LEVEL4  | High     | Time-critical sensors       |
| LEVEL5  | Highest  | Safety-critical hardware    |

**LEVEL3 is ideal for debug UART:**
- ✅ High enough to preempt most code
- ✅ Doesn't starve other critical ISRs
- ✅ Debug prints work during execution

### Our Configuration (Console UART)

```cpp
// UartConfig defaults
intrPriority = UartIntrPriority::LEVEL3;  // High priority
intrFlags = UartIntrFlags::IRAM;          // ISR in IRAM

// Converts to:
ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3
```

This ensures:
- ✅ Debug prints work **during Wi-Fi operations**
- ✅ Debug prints work **during flash writes**
- ✅ Debug prints work **during FreeRTOS scheduler stalls**
- ✅ Debug prints work **during critical sections**
- ✅ Debug prints work **when you need them most**

---

## 3️⃣ How Event Queue + Interrupt Flags Interact

| Configuration           | Behavior                       |
|-------------------------|--------------------------------|
| No queue + LEVEL1       | ❌ Fast but fragile            |
| Queue + LEVEL1          | ❌ Event-driven but lossy      |
| No queue + IRAM+LEVEL3  | ✅ **Best for debug UART**     |
| Queue + IRAM+LEVEL3     | ✅ Best for diagnostics+safety |

---

## 4️⃣ Buffer Architecture

### The Full Data Path

```
UART Hardware
    ↓
Hardware FIFO (128 bytes)
    ↓
[UART ISR fires when FIFO threshold reached]
    ↓
ISR copies FIFO → RX Ring Buffer (256+ bytes)
    ↓
[ISR posts to event queue if configured]
    ↓
Application calls uart_read_bytes()
    ↓
Reads from RX Ring Buffer
```

### Why Ring Buffers Matter

**Without ring buffer:**
- FIFO overflows after 128 bytes
- ISR must run every ~1.1ms @ 115200 baud
- Extreme timing sensitivity

**With ring buffer:**
- 256+ byte cushion
- ISR can be delayed ~2.2ms
- Much more forgiving

### Buffer Sizing

```cpp
config.rxBufferSize = 256;  // Default
```

**How to calculate:**
```
Bytes/second = baudRate / 10
Bytes/ms = baudRate / 10000

At 115200 baud:
= 11.52 bytes/ms
= ~11520 bytes/second

256 byte buffer = 22ms cushion
512 byte buffer = 44ms cushion
1024 byte buffer = 88ms cushion
```

**Recommendation:**
- Console/debug UART: 256 bytes (default)
- High-speed data: 512-1024 bytes
- Binary protocols: 1024-2048 bytes

---

## 5️⃣ Practical Examples

### Console UART (Our Default)

```cpp
UartConfig config = UartConfig::console();
// Defaults to:
//   intrPriority = LEVEL3 (high priority)
//   intrFlags = IRAM (survives flash ops)
//   eventQueueSize = 0 (polling mode)
//   rxBufferSize = 256 (22ms cushion @ 115200)

UartEsp32 uart(config);
```

**Result:**
- ✅ Debug prints ALWAYS work
- ✅ No event queue overhead
- ✅ Simple polling with uart_read_bytes()

### High-Reliability Binary Protocol

```cpp
UartConfig config;
config.port = UartPort::UART1;
config.baudRate = 460800;  // High speed
config.intrPriority = UartIntrPriority::LEVEL4;  // Very high priority
config.intrFlags = UartIntrFlags::IRAM;  // IRAM ISR
config.rxBufferSize = 2048;  // Large buffer
config.eventQueueSize = 10;  // Event-driven
config.flowControl = UartFlowControl::RTS_CTS;  // Hardware flow control

UartEsp32 uart(config);
```

**Result:**
- ✅ Very high priority ISR
- ✅ Large buffer for burst data
- ✅ Event-driven RX
- ✅ Hardware flow control prevents overrun

### Low-Overhead (NOT RECOMMENDED)

```cpp
UartConfig config = UartConfig::lowOverhead();
// Defaults to:
//   intrPriority = LEVEL1 (lowest)
//   intrFlags = NONE (no IRAM)
```

**Result:**
- ⚠️  Fragile under load
- ⚠️  ISR blocked during flash ops
- ⚠️  Can lose data
- ⚠️  Only use for non-critical streams

---

## 6️⃣ ESP32 Variant Differences

### ESP32 Classic
- 3 UART peripherals (UART0/1/2)
- All support full feature set
- UART0 typically console

### ESP32-S3
- 3 UART peripherals
- Better interrupt controller
- IRAM ISR highly recommended

### ESP32-C6 (RISC-V)
- 2 UART peripherals (UART0/1)
- Different interrupt architecture
- IRAM ISR essential

---

## 7️⃣ Debugging ISR Issues

### Symptoms of Fragile ISR Configuration

- ⚠️  "UART data corruption at high load"
- ⚠️  "Debug prints missing during Wi-Fi"
- ⚠️  "Gaps in serial output"
- ⚠️  "FIFO overflow errors"

### Solution Checklist

1. ✅ **Enable IRAM ISR**
   ```cpp
   config.intrFlags = UartIntrFlags::IRAM;
   ```

2. ✅ **Increase ISR priority**
   ```cpp
   config.intrPriority = UartIntrPriority::LEVEL3;
   ```

3. ✅ **Increase buffer size**
   ```cpp
   config.rxBufferSize = 512;  // or larger
   ```

4. ✅ **Add hardware flow control**
   ```cpp
   config.flowControl = UartFlowControl::RTS_CTS;
   ```

---

## 8️⃣ TL;DR

### For Debug/Console UART (Our Default)

```cpp
UartConfig config = UartConfig::console();
// Automatically sets:
// - LEVEL3 priority (high)
// - IRAM ISR (survives flash ops)
// - No event queue (polling)
// - 256 byte buffers
```

**This configuration ensures debug output is rock-solid.**

### Why It Matters

- **Event queue** = ISR → FreeRTOS message channel
  - Passing `0, NULL` = "don't notify me, I'll poll" ✅

- **Interrupt flags** = How survivable your ISR is under load
  - `0` = Lowest priority, safest, but **weakest** ❌
  - `ESP_INTR_FLAG_IRAM | LEVEL3` = Essential for reliability ✅

### The Critical Insight

On ESP32, the **default ISR configuration is dangerously fragile**.

For debug UART, you **MUST** use:
- ✅ IRAM ISR (ESP_INTR_FLAG_IRAM)
- ✅ High priority (LEVEL3 or higher)

Otherwise, your debug prints will vanish exactly when you need them most.

---

## References

- **ESP-IDF UART Driver:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/uart.html
- **ESP-IDF Interrupt Allocation:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/intr_alloc.html
- **FreeRTOS Queue Documentation:** https://www.freertos.org/a00018.html
