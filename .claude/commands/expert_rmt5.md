---
description: ESP-IDF v5.x RMT5 API expert for LED protocols, encoders, and multi-channel control
---

Get expert guidance on ESP-IDF v5.x RMT (Remote Control Transceiver) peripheral programming.

Use the 'expert-rmt5-agent' sub-agent to receive specialized help with:

## What This Expert Covers

### Implementation & Design
- **LED Strip Control**: WS2812, SK6812, APA106, and custom LED protocols
- **Encoder Architecture**: Copy, Bytes, Simple Callback, and Custom Composite encoders
- **Multi-Channel Sync**: Coordinated transmission across multiple outputs
- **IR Remote Control**: NEC, RC5, and other infrared protocols with carrier modulation
- **Custom Protocols**: Design patterns for complex waveform generation

### Migration & Modernization
- **RMT4 → RMT5 Migration**: Convert legacy code to new encoder-based architecture
- **API Translation**: Channel handles, symbol structures, clock configuration
- **Breaking Changes**: Removed types, new concepts, callback signatures

### Debugging & Troubleshooting
- **Common Errors**:
  - "encoding artifacts can't exceed hw memory block for loop transmission"
  - "clock source mismatch"
  - Timing inaccuracies, corrupted data, incomplete transmission
- **System Crashes**: Cache safety, IRAM placement, flash operation conflicts
- **Performance Issues**: DMA configuration, encoder optimization, memory sizing

### Configuration & Optimization
- **Timing Calculations**: Resolution selection, duration encoding, precision vs range
- **DMA Setup**: When to enable, buffer sizing, alignment requirements
- **Memory Management**: `mem_block_symbols` sizing, transaction queues
- **Clock Sources**: APB, XTAL, REF_TICK selection and implications
- **Hardware Limits**: Chip-specific capabilities (ESP32, S2, S3, C3, C6, H2)

## Expert Capabilities

The RMT5 expert agent has comprehensive knowledge of:
- Complete RMT5 API reference (channels, encoders, transmission, reception)
- Encoder state machine design patterns
- Hardware capabilities across ESP32 family
- LED protocol specifications (timing requirements, reset sequences)
- Multi-channel synchronization mechanisms
- Power management and sleep mode integration
- Thread safety and ISR considerations

## Example Use Cases

**"Help me implement WS2812 LED strip control with 100 LEDs"**
→ Complete encoder design, timing calculations, DMA configuration

**"Migrate my RMT4 code from ESP-IDF 4.4 to 5.x"**
→ Step-by-step API translation with code examples

**"Why do I get 'encoding artifacts can't exceed hw memory block'?"**
→ Root cause analysis and multiple solution options

**"How do I synchronize 3 LED strips to update simultaneously?"**
→ Sync manager setup with complete working example

**"Optimize my RMT implementation for power consumption"**
→ Clock source selection, DMA trade-offs, sleep mode integration

## What You'll Get

- **Complete code examples** with detailed comments
- **Timing calculations** showing duration ↔ resolution conversions
- **State machine diagrams** for custom encoders
- **Hardware compatibility** analysis for your target chip
- **Testing strategies** with edge cases and validation steps
- **References** to official documentation and RMT5.md sections

The expert agent will create a work plan (using TodoWrite) for complex tasks and provide structured, actionable guidance throughout.
