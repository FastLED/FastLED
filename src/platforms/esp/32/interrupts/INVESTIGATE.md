# ESP32 Interrupt Assembly Investigation

## Overview

The ESP32 interrupt handling files in this directory contain **UNTESTED** assembly implementations that require thorough investigation and validation before use. This document provides a structured approach to research and implement proper assembly directives for each ESP32 architecture.

## Files Requiring Investigation

### 1. `xtensa_lx6.hpp` - Original ESP32 (Xtensa LX6)
**Target Chips:** ESP32, ESP32-D0WD, ESP32-D2WD, ESP32-S0WD, ESP32-PICO-D4
**Architecture:** Xtensa LX6 dual-core/single-core

### 2. `xtensa_lx7.hpp` - ESP32-S Series (Xtensa LX7)
**Target Chips:** ESP32-S2, ESP32-S3
**Architecture:** Xtensa LX7 single-core/dual-core

### 3. `riscv.hpp` - ESP32-C/H/P Series (RISC-V)
**Target Chips:** ESP32-C2, ESP32-C3, ESP32-C5, ESP32-C6, ESP32-C61, ESP32-H2, ESP32-P4
**Architecture:** RISC-V RV32IMC single-core/dual-core

## Investigation Tasks

### Phase 1: Architecture Research

#### For Xtensa LX6 (`xtensa_lx6.hpp`)
1. **Official Documentation**
   - [ ] Download Xtensa LX6 ISA Reference Manual
   - [ ] Review ESP32 Technical Reference Manual interrupt sections
   - [ ] Study ESP-IDF high-priority interrupt examples for original ESP32

2. **Key Questions to Answer**
   - [ ] Are interrupt vector addresses different between LX6 and LX7?
   - [ ] What are the correct register save/restore sequences for LX6?
   - [ ] Are there LX6-specific assembly directives or constraints?
   - [ ] How do interrupt levels 4-5 behave differently on LX6 vs LX7?

3. **Assembly Directives to Verify**
   - [ ] `.section .iram1,"ax"` - Correct for LX6?
   - [ ] Register naming conventions (a0-a15)
   - [ ] Stack frame alignment requirements
   - [ ] `rfi N` instruction behavior on LX6

#### For Xtensa LX7 (`xtensa_lx7.hpp`)
1. **Official Documentation**
   - [ ] Download Xtensa LX7 ISA Reference Manual
   - [ ] Review ESP32-S2/S3 Technical Reference Manuals
   - [ ] Study ESP-IDF high-priority interrupt examples for S2/S3

2. **Key Questions to Answer**
   - [ ] What enhancements does LX7 have over LX6 for interrupts?
   - [ ] Are there new registers or instruction variants in LX7?
   - [ ] How do single-core (S2) vs dual-core (S3) affect interrupt handling?
   - [ ] Are interrupt vector tables different between S2 and S3?

3. **Assembly Directives to Verify**
   - [ ] LX7-specific instruction extensions
   - [ ] Enhanced interrupt handling features
   - [ ] Register window differences from LX6
   - [ ] Memory protection unit interactions

#### For RISC-V (`riscv.hpp`)
1. **Official Documentation**
   - [ ] Download RISC-V Privileged Architecture Specification v1.12+
   - [ ] Review ESP32-C3/C6/H2 Technical Reference Manuals
   - [ ] Study PLIC (Platform-Level Interrupt Controller) specification
   - [ ] Research ESP-IDF RISC-V interrupt examples

2. **Key Questions to Answer**
   - [ ] What are the correct PLIC base addresses for each chip variant?
   - [ ] How do interrupt priorities map to PLIC configuration?
   - [ ] Are there chip-specific RISC-V extensions (custom instructions)?
   - [ ] How does the vectored interrupt mode work on ESP32 RISC-V?

3. **Assembly Directives to Verify**
   - [ ] RISC-V calling convention compliance
   - [ ] PLIC claim/complete register addresses
   - [ ] `mret` instruction usage
   - [ ] Stack pointer alignment (16-byte for RV32I)
   - [ ] CSR (Control and Status Register) access patterns

### Phase 2: Code Analysis

#### Compare with ESP-IDF Examples
1. **Locate Official Examples**
   - [ ] Find ESP-IDF high-priority interrupt examples
   - [ ] Analyze esp-idf/examples/system/high_priority_interrupts/
   - [ ] Study components/hal/*/interrupt_*.c files
   - [ ] Review components/esp_hw_support/intr_alloc.c

2. **Extract Patterns**
   - [ ] Document official assembly patterns used by ESP-IDF
   - [ ] Identify differences between architectures
   - [ ] Note ESP-IDF version compatibility requirements
   - [ ] Extract proper build configuration requirements

#### Hardware Testing Requirements
1. **Test Platforms Needed**
   - [ ] ESP32 (LX6) development board
   - [ ] ESP32-S2 (LX7 single-core) development board
   - [ ] ESP32-S3 (LX7 dual-core) development board
   - [ ] ESP32-C3 (RISC-V) development board
   - [ ] ESP32-C6 (RISC-V) development board

2. **Test Scenarios**
   - [ ] Basic high-priority interrupt installation
   - [ ] Register save/restore verification
   - [ ] Interrupt latency measurements
   - [ ] Stability under Wi-Fi load
   - [ ] Multi-core interference testing (where applicable)

### Phase 3: Implementation Validation

#### Assembly Code Verification
1. **Syntax Validation**
   - [ ] Compile assembly with xtensa-esp32-elf-gcc
   - [ ] Compile assembly with riscv32-esp-elf-gcc
   - [ ] Verify linker script compatibility
   - [ ] Check IRAM section placement

2. **Runtime Validation**
   - [ ] Test interrupt installation without crashes
   - [ ] Verify register preservation across interrupts
   - [ ] Measure interrupt response times
   - [ ] Test under high interrupt load

#### Integration Testing
1. **FastLED Integration**
   - [ ] Test with actual WS2812 LED strips
   - [ ] Measure timing accuracy under load
   - [ ] Verify no visual artifacts or flicker
   - [ ] Test multiple strip configurations

2. **System Stability**
   - [ ] Long-duration testing (24+ hours)
   - [ ] Wi-Fi + Bluetooth + LED operation
   - [ ] Memory leak detection
   - [ ] Crash/exception monitoring

## Research Resources

### Official Documentation
- [ESP32 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)
- [ESP32-S2 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-s2_technical_reference_manual_en.pdf)
- [ESP32-S3 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
- [ESP32-C3 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-c3_technical_reference_manual_en.pdf)
- [ESP32-C6 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-c6_technical_reference_manual_en.pdf)
- [RISC-V Privileged Architecture Specification](https://github.com/riscv/riscv-isa-manual/releases)
- [Xtensa ISA Reference](https://www.cadence.com/content/dam/cadence-www/global/en_US/documents/tools/silicon-solutions/compute-ip/isa-summary.pdf)

### ESP-IDF References
- [High Priority Interrupts Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/hlinterrupts.html)
- [Interrupt Allocation API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/intr_alloc.html)
- [ESP-IDF GitHub Repository](https://github.com/espressif/esp-idf)

### Community Resources
- [ESP32 Developer Community](https://esp32.com/)
- [FastLED Issues and Discussions](https://github.com/FastLED/FastLED/issues)
- [ESP32 Arduino Core Issues](https://github.com/espressif/arduino-esp32/issues)

## Validation Checklist

Before marking any assembly implementation as "tested and validated":

### Code Review
- [ ] Assembly syntax verified against official ISA documentation
- [ ] Register usage follows architecture ABI conventions
- [ ] Stack management preserves 16-byte alignment
- [ ] Interrupt entry/exit sequences match hardware requirements
- [ ] IRAM placement directives are correct
- [ ] No hardcoded addresses without validation

### Compilation Testing
- [ ] Code compiles without warnings on target toolchain
- [ ] Linker produces correct memory layout
- [ ] Object code disassembly matches expectations
- [ ] No undefined symbols or references

### Hardware Validation
- [ ] Code runs without crashes on target hardware
- [ ] Interrupts fire and return correctly
- [ ] No register corruption detected
- [ ] Performance meets timing requirements
- [ ] Stable under continuous operation

### Integration Testing
- [ ] Compatible with official ESP-IDF versions (list tested versions)
- [ ] No conflicts with existing ESP32 drivers
- [ ] FastLED timing requirements satisfied
- [ ] Multi-core safety verified (where applicable)

## Warning Labels

Until this investigation is complete, **ALL** assembly implementations in these files should be considered:

- ⚠️ **UNTESTED** - May not work at all
- ⚠️ **POTENTIALLY DANGEROUS** - Could crash system or corrupt data
- ⚠️ **ARCHITECTURE-SPECIFIC** - May not work across different ESP32 variants
- ⚠️ **PLACEHOLDER CODE** - Based on assumptions, not validated facts

## Contributing Investigation Results

When contributing validated assembly implementations:

1. **Document Sources**: Reference all official documentation used
2. **Test Results**: Include detailed test reports with hardware specifics
3. **Version Compatibility**: Specify ESP-IDF and toolchain versions tested
4. **Performance Data**: Provide interrupt latency and timing measurements
5. **Limitations**: Document any known restrictions or compatibility issues

## Priority Order

Recommend investigating in this order based on FastLED usage patterns:

1. **RISC-V (`riscv.hpp`)** - Newer chips, simpler architecture, growing adoption
2. **Xtensa LX7 (`xtensa_lx7.hpp`)** - ESP32-S3 very popular for advanced projects
3. **Xtensa LX6 (`xtensa_lx6.hpp`)** - Original ESP32, but mature ecosystem exists

---

**Remember**: When in doubt, use the officially supported interrupt levels (1-3) rather than experimental high-priority assembly implementations. The goal is reliable LED timing, not maximum theoretical performance.