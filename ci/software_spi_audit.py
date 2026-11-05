"""Fail-closed software SPI source accounting and evidence foundation.

Inventory classification is review bookkeeping, never proof of an optimization.
In particular, no externally supplied JSON receipt is currently accepted as
execution evidence. Target runners must be wired before this audit can pass a
positive finding. Ordinary unit tests may pass while audit completion stays RED.
"""

import argparse
import hashlib
import json
import posixpath
import re
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from tempfile import NamedTemporaryFile

from running_process import CalledProcessError, RunningProcess, TimeoutExpired


ROOT = Path(__file__).resolve().parents[1]
BASELINE_COMMIT = "94dab9c6b720ea277ec9177c93129c7dc646c5c3"
BASELINE_SOURCE_COUNT = 2986


def _required(
    identity: str,
    issue: int,
    source: str | list[str],
    entrypoints: list[str],
    configurations: list[str],
    dependencies: list[str] | None = None,
    concerns: list[str] | None = None,
) -> dict[str, object]:
    return {
        "id": identity,
        "issue": issue,
        "sources": [source] if isinstance(source, str) else list(source),
        "entrypoints": entrypoints,
        "required_configurations": configurations,
        "dependencies": dependencies or [],
        "concerns": concerns or [],
    }


# Independent of editable audit receipts/findings. These are required evidence
# slots, NOT declarations that boards were compiled or behavior was measured.
# Mixed peripheral/GPIO headers are classified by actual entrypoint.
REQUIRED_FINDINGS = (
    _required(
        "avr-pins",
        5025,
        "src/platforms/avr/atmega/common/avr_pin.h",
        ["GenericSoftwareSPIOutput"],
        ["atmega328p", "atmega32u4", "atmega2560", "attiny", "atmega4809"],
    ),
    _required(
        "teensy4-pins",
        5026,
        "src/platforms/arm/teensy/teensy4_common/fastpin_arm_mxrt1062.h",
        ["SoftwareSPIOutput", "non-LPSPI SPIOutput"],
        ["teensy40-GPIO6", "teensy41-GPIO6-GPIO9"],
    ),
    _required(
        "teensy4-rate",
        5027,
        "src/platforms/shared/spi_bitbang/generic_software_spi.h",
        ["CLOCK_HI_DELAY", "CLOCK_LO_DELAY"],
        ["teensy41-sub-MHz", "teensy41-fractional-MHz"],
    ),
    _required(
        "esp32-software",
        5028,
        "src/platforms/shared/spi_bitbang/generic_software_spi.h",
        ["ESP32 writeBit"],
        [
            "esp32dev",
            "esp32s2",
            "esp32s3",
            "esp32c3",
            "esp32c6",
            "esp32p4",
            "upper-bank",
            "cross-bank",
        ],
    ),
    _required(
        "ci13xx-pins",
        5029,
        "src/platforms/ci13xx/fastpin_ci13xx.h",
        ["GenericSoftwareSPIOutput"],
        ["ci1302", "ci1303", "ci1306", "external-crystal"],
    ),
    _required(
        "apollo3-loop",
        5030,
        "src/platforms/apollo3/fastspi_apollo3.h",
        ["APOLLO3HardwareSPIOutput::writeBit"],
        ["apollo3_red", "apollo3_thing_explorable"],
    ),
    _required(
        "production-gate",
        5031,
        "src/platforms/shared/spi_bitbang/generic_software_spi.h",
        [
            "scalar",
            "bulk",
            "repeat",
            "pixels",
            "start-bit",
            "callbacks",
            "stream",
            "atomic",
        ],
        ["host-register-models", "vendor-integration", "restored-defect"],
    ),
    _required(
        "default-delay",
        5032,
        "src/platforms/shared/spi_bitbang/bitbang_pin.h",
        ["BitbangPin::delay"],
        ["default-no-LTO-AVR", "default-no-LTO-ARM", "default-no-LTO-CI13XX"],
    ),
    _required(
        "samd21",
        5033,
        "src/platforms/arm/d21/fastpin_arm_d21.h",
        ["non-SERCOM GenericSoftwareSPIOutput"],
        ["arduino-zero", "feather-m0", "xiao-samd21"],
    ),
    _required(
        "samd51",
        5034,
        "src/platforms/arm/d51/fastpin_arm_d51.h",
        ["non-SERCOM GenericSoftwareSPIOutput"],
        ["itsybitsy-m4", "feather-m4", "metro-m4", "grand-central-m4"],
    ),
    _required(
        "k20",
        5035,
        "src/platforms/arm/teensy/teensy31_32/fastpin_arm_k20.h",
        ["SoftwareSPIOutput"],
        ["teensy30", "teensy31", "teensy32"],
    ),
    _required(
        "rp2040",
        5036,
        "src/platforms/arm/rp/rp2040/fastpin_arm_rp2040.h",
        ["legacy SoftwareSPIOutput"],
        ["pico-arduino", "pico-w", "pico-mbed", "dual-core-atomic"],
    ),
    _required(
        "k64-k66",
        5037,
        "src/platforms/arm/teensy/teensy36/fastpin_arm_k66.h",
        ["SoftwareSPIOutput"],
        ["teensy35", "teensy36"],
    ),
    _required(
        "rp2350",
        5038,
        "src/platforms/arm/rp/rp2350/fastpin_arm_rp2350.h",
        ["legacy SoftwareSPIOutput"],
        [
            "pico2-low-bank",
            "pico2-w",
            "rp2350b-high-bank",
            "cross-bank",
            "dual-core-atomic",
        ],
    ),
    _required(
        "kl26",
        5039,
        "src/platforms/arm/teensy/teensy_lc/fastpin_arm_kl26.h",
        ["SoftwareSPIOutput"],
        ["teensy-lc"],
    ),
    _required(
        "renesas",
        5040,
        "src/platforms/arm/renesas/fastpin_arm_renesas.h",
        ["GenericSoftwareSPIOutput"],
        ["uno-r4-minima", "uno-r4-wifi", "ra6m5", "portenta"],
    ),
    _required(
        "mgm240",
        5041,
        "src/platforms/arm/mgm240/fastpin_arm_mgm240.h",
        ["GenericSoftwareSPIOutput"],
        ["nano-matter-PA", "nano-matter-PB", "cross-port"],
    ),
    _required(
        "lpc-modern",
        5042,
        "src/platforms/arm/lpc/fastpin_arm_lpc.h",
        ["GenericSoftwareSPIOutput"],
        ["lpc845", "lpc804", "lpc11u24", "lpc11u35", "lpc1517", "lpc1549"],
    ),
    _required(
        "lpc-legacy",
        5043,
        "src/platforms/arm/lpc/fastpin_arm_lpc11_legacy.h",
        ["GenericSoftwareSPIOutput masked write", "full DATA address"],
        ["lpc1114", "lpc1115", "lpc1112", "vendor-CMSIS", "fallback-layout"],
    ),
    _required(
        "same53",
        5044,
        "src/platforms/arm/same53/fastpin_arm_same53.h",
        ["inverted GenericSoftwareSPIOutput"],
        ["clearcore-IO0-IO1", "clearcore-IO2-IO3", "connector-polarity"],
    ),
    _required(
        "stm32",
        5045,
        "src/platforms/arm/stm32/fastspi_arm_stm32.h",
        ["STM32SPIOutput non-HAL writeBit", "explicit SoftwareSPIOutput"],
        [
            "f103-libmaple",
            "f411",
            "f401",
            "f446",
            "g070",
            "u585",
            "photon-f205",
            "giga-h747",
        ],
    ),
    _required(
        "sam3x",
        5046,
        "src/platforms/arm/sam/fastspi_arm_sam.h",
        ["SAMHardwareSPIOutput::writeBit", "explicit SoftwareSPIOutput"],
        ["arduino-due", "digix", "PIO-OWSR"],
    ),
    _required(
        "nrf51",
        5047,
        "src/platforms/arm/nrf51/fastspi_arm_nrf51.h",
        ["NRF51SPIOutput::writeBit", "pixel start-bit", "explicit SoftwareSPIOutput"],
        ["microbit-v1", "rfduino", "forced-software-pins"],
    ),
    _required(
        "nrf52",
        5048,
        "src/platforms/arm/nrf52/fastspi_arm_nrf52.h",
        [
            "NRF52HardwareSPIOutput::writeBit",
            "pixel start-bit",
            "explicit SoftwareSPIOutput",
        ],
        ["nrf52832-P0", "nrf52840-P0", "nrf52840-P1", "cross-port", "softdevice"],
    ),
    _required(
        "runtime-manager",
        5049,
        "src/platforms/shared/spi_manager.cpp.hpp",
        ["SPIBusManager::softwareSPIWrite", "SOFT_SPI transmit"],
        ["forced-SOFT_SPI", "hardware-fallback", "ESP32", "ESP8266-GPIO16", "CI13XX"],
    ),
    _required(
        "runtime-channel",
        5050,
        "src/platforms/shared/bitbang/bitbang_channel_driver.cpp.hpp",
        ["BitBangChannelDriver::transmitSpi"],
        ["BIT_BANG-SPI-1-to-8-lanes", "unequal-length", "mixed-bank", "clock-groups"],
    ),
    _required(
        "esp32-rate",
        5051,
        "src/fastspi.h",
        ["SoftwareSPIOutput DATA_RATE_MHZ", "SoftwareSPIOutput DATA_RATE_KHZ"],
        ["esp32-Xtensa", "esp32-RISCV", "CPU-frequency", "literal-compatibility"],
    ),
    _required(
        "avr-native-gpio",
        5052,
        "src/platforms/avr/fastspi_avr.h",
        ["all five native SPI/UART writeBit implementations"],
        ["SPI-GPIO-methods", "UART-GPIO-methods", "all-bits-variable-values"],
    ),
    _required(
        "samd-sercom-gpio",
        5053,
        "src/platforms/arm/sam/fastspi_arm_sam.h",
        [
            "SAMDHardwareSPIOutput::writeBit",
            "SM16716 header",
            "free pixel writer start-bit",
        ],
        ["SAMD21-SERCOM-GPIO", "SAMD51-SERCOM-GPIO"],
        dependencies=["src/platforms/arm/sam/fastspi_arm_samd.hpp"],
    ),
    _required(
        "avr-native-handoff",
        5055,
        "src/platforms/avr/fastspi_avr.h",
        [
            "AVRUSART1SPIOutput::writeBit [UBRR1]",
            "AVRUSART0SPIOutput::writeBit [UBRR0]",
            "AVRHardwareSPIOutput::writeBit [SPI0_CTRLA]",
            "AVRHardwareSPIOutput::writeBit [SPSR]",
            "AVRHardwareSPIOutput::writeBit [SPSR0]",
        ],
        ["UBRR1", "UBRR0", "SPI0_CTRLA", "SPSR", "SPSR0"],
        dependencies=[
            "src/fl/chipsets/sm16716.h",
            "src/platforms/shared/spi_pixel_writer.h",
        ],
        concerns=[
            "prior-byte-shifter-drain",
            "peripheral-mux-release",
            "prior-state-restore",
            "following-byte-transfer",
        ],
    ),
    _required(
        "forced-software-no-pinmap",
        5056,
        "src/fl/system/fastpin_base.h",
        [
            "FastPin<>::_init [FASTLED_FORCE_SOFTWARE_PINS]",
            "FastPin<> GPIO operations [FASTLED_NO_PINMAP]",
            "GenericSoftwareSPIOutput scalar/bulk/pixel forced-pin paths",
        ],
        [
            "ClearCore-forced-NoPinMap",
            "Arduino-NoPinMap-before-FastLED",
            "Arduino-NoPinMap-after-FastLED",
            "Arduino-pinmap-forced-control",
        ],
        dependencies=[
            "src/fastled_config.h",
            "src/fl/system/arduino.h",
            "src/fl/system/forced_software_pin.h",
            "src/platforms/fastpin.h",
            "src/platforms/shared/spi_bitbang/generic_software_spi.h",
            "src/platforms/arm/same53/led_sysdefs_arm_same53.h",
        ],
        concerns=[
            "safe-Arduino-GPIO-fallback",
            "no-null-register-snapshots",
            "no-unproven-whole-port-streaming",
            "Arduino-declarations-include-order",
        ],
    ),
    _required(
        "generic-init-clock-low",
        5057,
        "src/platforms/shared/spi_bitbang/generic_software_spi.h",
        ["GenericSoftwareSPIOutput::init"],
        [
            "normal-same-port",
            "normal-independent-ports",
            "inverted-same-port",
            "inverted-independent-ports",
            "stream-8bit",
            "stream-16bit",
            "stream-32bit",
        ],
        dependencies=["src/fl/system/fastpin.h"],
        concerns=[
            "logical-idle-low-before-release",
            "primed-high-first-byte-eight-rises",
            "normal-and-inverted-pin-semantics",
            "scalar-bulk-stream-independent-port",
        ],
    ),
    _required(
        "kl26-native-bit",
        5059,
        [
            "src/platforms/arm/teensy/teensy_lc/fastspi_arm_kl26.h",
            "src/platforms/arm/kl26/fastspi_arm_kl26.h",
        ],
        [
            "ARMHardwareSPIOutput::writeBit [teensy_lc canonical]",
            "ARMHardwareSPIOutput::writeBit [kl26 legacy]",
            "Teensy LC SPIOutput<11,13> -> SPI0 0x40076000",
        ],
        [
            "teensy-lc-native-all-bits-all-256-values",
            "teensy-lc-native-SM16716-start-bits",
            "teensy-lc-native-SM16716-header",
            "kl26-legacy-separate",
            "mux-drain-following-byte",
        ],
        dependencies=[
            "src/platforms/arm/kl26/fastspi_arm_kl26.h",
            "src/platforms/spi_output_template.h",
            "src/platforms/arm/teensy/teensy_lc/spi_output_template.h",
            "src/fl/chipsets/sm16716.h",
            "src/platforms/shared/spi_pixel_writer.h",
        ],
        concerns=[
            "single-bit-GPIO-waveform-or-whole-protocol-fallback",
            "prior-byte-shifter-drain",
            "peripheral-mux-release-and-restore",
            "following-byte-transfer",
            "SM16716-one-start-bit-per-pixel",
            "SM16716-two-header-bits",
            "legacy-implementation-independent-validation",
        ],
    ),
    _required(
        "esp32-native-single-bit",
        5060,
        [
            "src/platforms/esp/32/drivers/spi/spi_device_proxy.h",
            "src/platforms/esp/32/core/fastspi_esp32_idf.h",
            "src/platforms/arduino/spi/fastspi_esp32_arduino.h",
        ],
        [
            "SPIDeviceProxy::writeBit [single/quad]",
            "ESP32SPIOutput::writeBit [ESP-IDF]",
            "ESP32SPIOutput::writeBit [Arduino]",
        ],
        [
            "all-BIT0-7-all-256-values",
            "ESP32-proxy-single",
            "ESP32-proxy-quad",
            "IDF-buffer-and-flush",
            "bit-byte-bit",
            "consecutive-frames",
            "Xtensa-IDF",
            "RISC-V-IDF",
            "Arduino-FL_ESP32_SPI_ARDUINO",
        ],
        dependencies=[
            "src/platforms/spi_output_template.h",
            "src/platforms/esp/32/drivers/spi/spi_output_template.h",
            "src/platforms/esp/32/core/fastspi_esp32.h",
            "src/fl/chipsets/sm16716.h",
            "src/platforms/shared/spi_pixel_writer.h",
        ],
        concerns=[
            "one-clock-per-writeBit",
            "SM16716-25-bits-per-pixel-and-50-bit-header",
            "separate-method-restored-defect-mutations",
            "byte-only-APA102-preservation",
            "true-single-bit-or-whole-protocol-fallback-or-rejection",
        ],
    ),
    _required(
        "arduino-core-native-single-bit",
        5061,
        "src/platforms/fastspi_arduino_core.h",
        [
            "ArdunioCoreSPIOutput::writeBit",
            "ArdunioCoreSPIOutput::writePixels [FLAG_START_BIT]",
        ],
        [
            "ARDUNIO_CORE_SPI-defined",
            "D51-guard-and-include-route",
            "public-direct-instantiation-on-real-Arduino-SDK",
            "BIT0-BIT3-BIT7-zero-one-payloads",
            "FLAG_START_BIT-pixels",
            "bit-byte-bit-and-state-restoration",
        ],
        dependencies=[
            "src/platforms/arm/d51/fastpin_arm_d51.h",
            "src/platforms/arm/d51/fastled_arm_d51.h",
            "src/platforms/spi_output_template.h",
            "src/fastspi_types.h",
            "src/pixel_controller.h",
        ],
        concerns=[
            "guarded-public-callable-positive",
            "no-current-in-tree-selector-not-a-negative",
            "one-clock-per-writeBit",
            "true-single-bit-or-explicit-compile-time-rejection",
            "peripheral-drain-mux-ownership-and-restoration",
            "restored-empty-body-mutation-must-fail",
        ],
    ),
    _required(
        "parallel-spi-target-backend",
        5062,
        ["src/platforms/shared/spi_bitbang/spi_platform.h"],
        [
            "FL_GPIO_WRITE_SET / FL_GPIO_WRITE_CLEAR target selection",
            "SpiBlock16::transmit",
            "SpiBlock1::transmit",
            "SpiBlock2::transmit",
            "SpiBlock32::transmit",
            "SpiBlock4::transmit",
            "SpiBlock8::transmit",
            "fl_parallel_spi_isr GPIO writes",
        ],
        [
            "all-six-blocking-callers-and-shared-ISR-engine",
            "empty-final-repeated-transfer-and-unrelated-GPIO",
            "explicit-custom-backend-contract",
            "non-ESP-target-explicit-rejection",
            "real-ESP32-C3-C2-RISC-V-vendor-registers",
            "real-classic-ESP32-Xtensa-vendor-registers",
        ],
        dependencies=[
            "src/fl/channels/spi/parallel_device.cpp.hpp",
            "src/fl/channels/spi/parallel_device.h",
            "src/platforms/shared/spi_bitbang/spi_block_1.h",
            "src/platforms/shared/spi_bitbang/spi_block_16.h",
            "src/platforms/shared/spi_bitbang/spi_block_2.h",
            "src/platforms/shared/spi_bitbang/spi_block_32.h",
            "src/platforms/shared/spi_bitbang/spi_block_4.h",
            "src/platforms/shared/spi_bitbang/spi_block_8.h",
            "src/platforms/shared/spi_bitbang/spi_isr_engine.cpp.hpp",
            "src/platforms/shared/spi_bitbang/spi_isr_engine.h",
        ],
        concerns=[
            "actual-vendor-register-identity-bank-and-operation-width",
            "host-simulation-does-not-prove-target-addresses",
            "independent-caller-configuration-evidence",
            "public-direct-callable-no-active-ParallelDevice-producer-claim",
            "restored-unconditional-C3-C2-address-mutation-must-fail",
        ],
    ),
    _required(
        "parallel-spi-pin-mapping",
        5063,
        [
            "src/platforms/shared/spi_bitbang/spi_block_1.h",
            "src/platforms/shared/spi_bitbang/spi_block_16.h",
            "src/platforms/shared/spi_bitbang/spi_block_2.h",
            "src/platforms/shared/spi_bitbang/spi_block_32.h",
            "src/platforms/shared/spi_bitbang/spi_block_4.h",
            "src/platforms/shared/spi_bitbang/spi_block_8.h",
            "src/platforms/shared/spi_bitbang/spi_isr_1.h",
            "src/platforms/shared/spi_bitbang/spi_isr_2.h",
            "src/platforms/shared/spi_bitbang/spi_isr_4.h",
        ],
        [
            "SpiBlock16::setPinMapping",
            "SpiBlock1::setPinMapping",
            "SpiBlock2::setPinMapping",
            "SpiBlock32::setPinMapping",
            "SpiBlock4::setPinMapping",
            "SpiBlock8::setPinMapping",
            "SpiIsr1::setPinMapping",
            "SpiIsr1::setPinMappingWithMask",
            "SpiIsr2::setPinMapping",
            "SpiIsr2::setPinMappingWithMask",
            "SpiIsr4::setPinMapping",
            "SpiIsr4::setPinMappingWithMask",
        ],
        [
            "32-independent-data-plus-clock-one-bank-rejection-or-real-bank-model",
            "ISR8-16-32-separate-raw-mask-contracts",
            "all-six-blocking-and-ISR1-2-4-pin-and-mask-variants",
            "duplicate-data-and-data-clock-collisions",
            "failed-reconfiguration-preserves-prior-valid-state",
            "pin-indices-31-32-255-and-unsigned-int-width-boundaries",
            "real-vendor-bank-width-and-host-undefined-shift-detection",
            "valid-edges-unrelated-pins-empty-and-repeated-transfer",
        ],
        dependencies=[
            "src/fl/channels/spi/parallel_device.cpp.hpp",
            "src/fl/channels/spi/parallel_device.h",
            "src/platforms/shared/spi_bitbang/spi_isr_16.h",
            "src/platforms/shared/spi_bitbang/spi_isr_32.h",
            "src/platforms/shared/spi_bitbang/spi_isr_8.h",
            "src/platforms/shared/spi_bitbang/spi_isr_engine.cpp.hpp",
            "src/platforms/shared/spi_bitbang/spi_isr_engine.h",
            "src/platforms/shared/spi_bitbang/spi_platform.h",
        ],
        concerns=[
            "independent-implementations-are-not-forwarding-aliases",
            "invalid-input-must-not-write-GPIO-or-alias-bits",
            "public-direct-callable-no-active-ParallelDevice-producer-claim",
            "raw-mask-APIs-are-not-pin-index-APIs",
            "restored-unchecked-mapping-mutations-must-fail-independently",
            "validate-before-shift-index-state-change-or-GPIO",
        ],
    ),
    _required(
        "teensy4-native-pending-bits",
        5064,
        [
            "src/platforms/arm/mxrt1062/fastspi_arm_mxrt1062.h",
            "src/platforms/arm/teensy/teensy4_common/fastspi_arm_mxrt1062.h",
        ],
        [
            "Teensy4HardwareSPIOutput::endTransaction",
            "Teensy4HardwareSPIOutput::finalizeTransmission",
            "Teensy4HardwareSPIOutput::init",
            "Teensy4HardwareSPIOutput::release",
            "Teensy4HardwareSPIOutput::select",
            "Teensy4HardwareSPIOutput::waitFully",
            "Teensy4HardwareSPIOutput::writeBit",
            "Teensy4HardwareSPIOutput::writeByte",
            "Teensy4HardwareSPIOutput::writeBytes",
            "Teensy4HardwareSPIOutput::writeBytesValue",
            "Teensy4HardwareSPIOutput::writeBytesValueRaw",
            "Teensy4HardwareSPIOutput::writePixels",
            "Teensy4HardwareSPIOutput::writeWord",
        ],
        [
            "all-BIT0-7-zero-one-payloads",
            "all-supported-LPSPI-indices-or-explicit-rejection",
            "bit-byte-bit-word-repeat-raw-bulk-and-pixel-start-bits",
            "both-independent-canonical-and-legacy-headers-real-Teensy-SDK",
            "byte-only-preservation-and-unrelated-peripheral-pin-state",
            "direct-backend-SM16716-two-bit-six-byte-two-bit-header",
            "partial-counts1-7-8-9-and-larger-tails",
            "wait-finalize-release-end-reselect-and-reinitialize",
        ],
        dependencies=[
            "src/fl/chipsets/sm16716.h",
            "src/platforms/arm/mxrt1062/spi_device_proxy.h",
            "src/platforms/arm/teensy/is_teensy.h",
            "src/platforms/arm/teensy/teensy4_common/lpspi/lpspi_bus.h",
            "src/platforms/arm/teensy/teensy4_common/lpspi/lpspi_hardware.h",
            "src/platforms/shared/spi_pixel_writer.h",
            "src/platforms/spi_output_template.h",
        ],
        concerns=[
            "complete-pending-bits-before-select-release-or-explicitly-reject",
            "exact-bit-count-order-drain-and-no-cross-frame-pending-state",
            "independent-header-bodies-are-not-forwarding-aliases",
            "public-callable-positive-without-active-proxy-writeBit-producer",
            "real-LPSPI-frame-width-ownership-and-SDK-evidence",
            "restored-no-flush-and-pending-state-bypass-mutations-fail-separately",
        ],
    ),
    _required(
        "objectfled-spi-pin-restore",
        5065,
        "src/platforms/arm/teensy/teensy4_common/drivers/objectfled/objectfled_spi_mode.cpp.hpp",
        [
            "fl::objectfled_spi_init",
            "fl::objectfled_spi_show",
            "fl::objectfled_spi_wait",
            "fl::objectfled_spi_deinit",
            "AutoResearch::objectfledSpiSelfTest-RPC",
        ],
        [
            "real-Teensy4-SDK-GPR26-and-CORE_PIN14_PORTREG-GPIO6",
            "both-selected-pads-GPIO6-and-other-supported-prior-states",
            "GPR26-mux-pad-GDIR-latch-restore-and-unrelated-bits",
            "actual-init-show-wait-deinit-and-repeated-frames",
            "allocation-failure-partial-init-and-timeout-cleanup",
            "ordinary-pinMode-digitalWrite-and-FastPin-reuse-after-deinit",
            "DMA-drain-before-safe-bank-and-pin-state-restoration",
        ],
        dependencies=[
            "src/platforms/arm/teensy/teensy4_common/drivers/objectfled/objectfled_spi_mode.h",
            "src/platforms/arm/teensy/teensy4_common/drivers/objectfled/channel_engine_objectfled.cpp.hpp",
            "src/platforms/arm/teensy/teensy4_common/fastpin_arm_mxrt1062.h",
            "src/platforms/arm/teensy/is_teensy.h",
            "src/third_party/object_fled/src/ObjectFLEDDmaManager.h",
            "src/third_party/object_fled/src/ObjectFLEDDmaManager.cpp.hpp",
        ],
        concerns=[
            "actual-AutoResearch-direct-init-show-deinit-producer",
            "channel-canHandle-lookup-is-not-an-emitting-producer",
            "hardware-assisted-GPIO-serializer-not-FIFO-only-exclusion",
            "real-vendor-pinMode-does-not-restore-GPR26",
            "restored-no-GPR-restore-and-omitted-pad-state-mutations-fail-separately",
            "whole-bank-concurrent-writer-question-remains-separate-unproven",
            "SDK-source-configuration-and-producer-identity-bound-no-physical-claim",
        ],
    ),
)

# Semantic candidates missed by the original five query families. Keep them
# named independently so token removal does not erase a required source row.
SEMANTIC_REQUIRED_PATHS = frozenset(
    [
        f"src/platforms/shared/spi_bitbang/spi_block_{width}.h"
        for width in (1, 2, 4, 8, 16, 32)
    ]
    + [
        f"src/platforms/shared/spi_bitbang/spi_isr_{width}.h"
        for width in (1, 2, 4, 8, 16, 32)
    ]
    + [
        "src/platforms/shared/spi_bitbang/spi_isr_engine.cpp.hpp",
        "src/platforms/shared/spi_bitbang/spi_platform.h",
        "src/platforms/shared/bitbang/bus_traits.h",
        "src/fl/system/pins.h",
        "src/fl/system/pins.cpp.hpp",
        "src/platforms/spi_output_template.h",
        "src/platforms/fastpin.h",
        "src/platforms/arm/k66/clockless_arm_k66.h",
        "src/platforms/arm/teensy/teensy31_32/clockless_arm_k20.h",
        "src/platforms/arm/teensy/teensy36/clockless_arm_k66.h",
        "src/platforms/arm/teensy/teensy4_common/drivers/flexio/channel_engine_flexio.cpp.hpp",
        "src/platforms/arm/teensy/teensy4_common/drivers/objectfled/channel_engine_objectfled.cpp.hpp",
        "src/platforms/avr/README.md",
    ]
)

PINNED_EXCLUSIONS = {
    "src/platforms/arm/k66/clockless_arm_k66.h": (
        "clockless",
        "Single-data-wire pulse timing; not SPI clock/data output.",
    ),
    "src/platforms/arm/teensy/teensy31_32/clockless_arm_k20.h": (
        "clockless",
        "K20 clockless pulse engine; software SPI pin policy is tracked separately.",
    ),
    "src/platforms/arm/teensy/teensy36/clockless_arm_k66.h": (
        "clockless",
        "K66 clockless pulse engine; software SPI pin policy is tracked separately.",
    ),
    "src/platforms/arm/teensy/teensy4_common/drivers/flexio/channel_engine_flexio.cpp.hpp": (
        "hardware",
        "Channel engine delegates to FlexIO peripheral output; not a GPIO SPI loop.",
    ),
    "src/platforms/arm/teensy/teensy4_common/drivers/objectfled/channel_engine_objectfled.cpp.hpp": (
        "hardware",
        "Channel engine delegates to ObjectFLED peripheral output; not a GPIO SPI loop.",
    ),
    "src/platforms/avr/README.md": (
        "documentation",
        "AVR platform documentation; live native GPIO methods tracked in #5052.",
    ),
}


# These catch a few semantic query misses; they are NOT the source denominator.
# Every path, including zero matches, must have a source-bound review row.
SEMANTIC_PATTERNS = (
    re.compile(rb"\*\s*[A-Za-z_]\w*\s*(?:\|=|&=|\^=|=(?!=))"),
    re.compile(rb"\b(?:writeBit|writeByte|fastset|digitalWrite)\s*(?:<|\()"),
    re.compile(rb"\b(?:GPIO|OUTSET|OUTCLR|W1TS|W1TC|MASKED_ACCESS)\b"),
)
DISPOSITIONS = frozenset(
    {
        "negative",
        "hardware",
        "clockless",
        "documentation",
        "alias",
        "positive",
        "dependency",
    }
)
SEMANTIC_DISPOSITIONS = DISPOSITIONS - {"negative", "documentation"}


def source_digest(source: bytes) -> str:
    return hashlib.sha256(source).hexdigest()


def _pure_forwarded_include(source: bytes) -> str | None:
    """Recognize one unconditional include, optionally pragma-once/commented.

    This is deliberately narrower than C++ delegation. Any declarations,
    independent code, extra includes or unsupported conditional dispatch need
    another explicit disposition/review rather than an alias exemption.
    """
    try:
        text = source.decode("utf-8")
    except UnicodeDecodeError:
        return None
    # Universal logical newlines precede comment recognition. Unsupported line
    # splices/trigraphs need review; do not simulate their translation phases.
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    if "\\" in text or "??" in text:
        return None

    def remove_comment(match: re.Match[str]) -> str:
        token = match[0]
        if token.startswith('"'):
            return token
        return "\n" * token.count("\n") or " "

    # Earliest-token order matters: /* inside a // comment cannot consume live
    # following lines, and comment-shaped text inside a quoted header is literal.
    text = re.sub(
        r'"[^"\n]*"|//[^\n]*|/\*.*?\*/',
        remove_comment,
        text,
        flags=re.DOTALL,
    )
    lines = [line.strip() for line in text.split("\n") if line.strip()]
    if lines and re.fullmatch(r"#\s*pragma\s+once", lines[0]):
        lines = lines[1:]
    if len(lines) != 1:
        return None
    include = re.fullmatch(r'#\s*include\s*"([^"\n]+)"', lines[0])
    return include[1] if include else None


def _check_alias(
    path: str,
    row: Mapping[str, object],
    current: Mapping[str, bytes],
    classifications: Mapping[str, object],
) -> list[str]:
    canonical = row.get("canonical")
    if not isinstance(canonical, str) or canonical not in current:
        return [f"alias requires an existing canonical source: {path}"]
    if canonical == path:
        return [f"alias cannot forward to itself: {path}"]
    include = _pure_forwarded_include(current.get(path, b""))
    resolved = None
    if include:
        # Match quoted-header search order for the supported repository model:
        # the forwarding file's directory first, then the project src root.
        candidates = (
            posixpath.normpath(str(PurePosixPath(path).parent / include)),
            posixpath.normpath("src/" + include),
        )
        resolved = next((target for target in candidates if target in current), None)
    if canonical != resolved:
        return [f"alias source is not a pure forwarder to its canonical source: {path}"]

    visited: set[str] = set()
    cursor = path
    while True:
        if cursor in visited:
            return [f"alias cycle does not terminate in an implementation: {path}"]
        visited.add(cursor)
        target_row = classifications.get(cursor)
        if not isinstance(target_row, Mapping):
            return [
                f"alias canonical source requires classification: {path} -> {cursor}"
            ]
        if target_row.get("disposition") != "alias":
            return []
        target = target_row.get("canonical")
        if not isinstance(target, str) or target not in current:
            return [f"alias chain has an unavailable canonical source: {path}"]
        cursor = target


def _check_positive_findings(path: str, row: Mapping[str, object]) -> list[str]:
    bindings = row.get("findings")
    if (
        not isinstance(bindings, list)
        or not bindings
        or any(not isinstance(identity, str) or not identity for identity in bindings)
    ):
        return [f"positive source requires a nonempty findings list: {path}"]
    if len(set(bindings)) != len(bindings):
        return [f"positive source findings must be unique: {path}"]
    known = {finding["id"] for finding in REQUIRED_FINDINGS}
    unknown = set(bindings) - known
    if unknown:
        return [f"positive source has unknown findings {sorted(unknown)}: {path}"]
    return []


def check_inventory(
    baseline: Mapping[str, bytes],
    current: Mapping[str, bytes],
    classifications: Mapping[str, object],
) -> list[str]:
    """Check the entire supplied source universe, independently of token hits.

    Baseline and current must be obtained from independent source enumeration by
    the caller. No query, trait or manifest entry can remove a source from their
    union. A source change intentionally remains unresolved here: refreshing a
    review hash alone must not authorize a change or certify behavioral proof.
    """
    errors: list[str] = []
    if not isinstance(classifications, Mapping) or any(
        not isinstance(path, str) for path in classifications
    ):
        return ["classification schema requires a mapping of string paths"]
    for path in sorted(set(baseline) | set(current)):
        if path not in baseline:
            errors.append(f"new source requires review and evidence: {path}")
        elif path not in current:
            errors.append(f"deleted source requires disposition: {path}")
        elif baseline[path] != current[path]:
            errors.append(f"changed source requires review and evidence: {path}")

        row = classifications.get(path)
        if row is None:
            errors.append(f"unclassified source: {path}")
            continue
        if not isinstance(row, Mapping):
            errors.append(f"classification schema requires an object row: {path}")
            continue
        source = current.get(path, baseline.get(path, b""))
        if row.get("source_sha256") != source_digest(source):
            errors.append(f"stale classification source hash: {path}")
        disposition = row.get("disposition")
        if not isinstance(disposition, str) or disposition not in DISPOSITIONS:
            errors.append(f"unknown disposition: {path}")
        reason = row.get("reason")
        if not isinstance(reason, str) or not reason.strip():
            errors.append(f"missing classification reason: {path}")
        if any(pattern.search(source) for pattern in SEMANTIC_PATTERNS):
            if (
                not isinstance(disposition, str)
                or disposition not in SEMANTIC_DISPOSITIONS
            ):
                errors.append(f"semantic candidate needs explicit disposition: {path}")
        if disposition == "alias":
            errors.extend(_check_alias(path, row, current, classifications))
        if disposition == "positive":
            errors.extend(_check_positive_findings(path, row))

    for path in sorted(set(classifications) - set(baseline) - set(current)):
        errors.append(f"classification outside source universe: {path}")
    return errors


def check_evidence(
    findings: Sequence[Mapping[str, object]],
    claims: Sequence[Mapping[str, object]],
    current: Mapping[str, bytes],
) -> list[str]:
    """Keep each real implementation/configuration RED until runners exist.

    The foundation deliberately does not trust claimed success, command strings,
    closed issues, flags, model traces, filenames or hashes as executed proof.
    Later runner adapters must execute allowlisted production tests/target builds
    and restored-defect mutations, bind their full inputs, and distinguish host,
    target instruction and physical evidence. Merely adding receipts cannot make
    this function certify an unresolved positive.
    """
    errors: list[str] = []
    seen: set[str] = set()
    for finding in findings:
        if not isinstance(finding, Mapping):
            errors.append("finding schema requires an object record")
            continue
        identity = finding.get("id")
        if not isinstance(identity, str) or not identity:
            errors.append("finding missing implementation identity")
            continue
        if identity in seen:
            errors.append(f"duplicate finding identity: {identity}")
        seen.add(identity)

        paths = finding.get("sources")
        if not isinstance(paths, list) or not paths:
            errors.append(f"{identity}: missing production source binding")
        else:
            for path in paths:
                if not isinstance(path, str) or path not in current:
                    errors.append(f"{identity}: unavailable production source: {path}")

        configurations = finding.get("required_configurations")
        if not isinstance(configurations, list) or not configurations:
            errors.append(f"{identity}: missing real configuration matrix")
            continue
        for configuration in configurations:
            errors.append(
                f"{identity}/{configuration}: unresolved production execution, "
                "target configuration and restored-defect evidence (runner not wired)"
            )

    for claim in claims:
        if not isinstance(claim, Mapping):
            errors.append("evidence schema requires an object record")
            continue
        identity = claim.get("finding", "unknown")
        errors.append(
            f"{identity}: unverified external claim is not execution evidence"
        )
    return errors


def check_required_findings(
    required: Sequence[Mapping[str, object]],
    proposed: Sequence[Mapping[str, object]],
    classifications: Mapping[str, object],
) -> list[str]:
    """Bind editable findings to an independently pinned entrypoint registry.

    Declarations passing this check are still not execution proof. Multiple
    entrypoints may share one source file: source-level hardware/alias labels
    cannot erase a GPIO-only method inside an otherwise peripheral driver.
    """
    errors: list[str] = []
    by_id: dict[str, Mapping[str, object]] = {}
    for finding in proposed:
        if not isinstance(finding, Mapping):
            errors.append("proposed finding schema requires an object record")
            continue
        identity = finding.get("id")
        if not isinstance(identity, str) or not identity:
            errors.append("proposed finding missing identity")
        elif identity in by_id:
            errors.append(f"duplicate proposed finding: {identity}")
        else:
            by_id[identity] = finding
    required_ids = {entry.get("id") for entry in required}
    for identity in sorted(set(by_id) - required_ids):
        errors.append(f"new finding requires registry review: {identity}")

    for entry in required:
        identity = str(entry.get("id", "unknown"))
        finding = by_id.get(identity)
        if finding is None:
            errors.append(f"required production entrypoint missing: {identity}")
            continue
        for field in (
            "issue",
            "sources",
            "required_configurations",
            "entrypoints",
            "dependencies",
            "concerns",
        ):
            if finding.get(field) != entry.get(field):
                errors.append(f"required {field} changed or removed: {identity}")
        paths = entry.get("sources", [])
        if not isinstance(paths, list):
            errors.append(f"registry source binding malformed: {identity}")
            continue
        for path in paths:
            if not isinstance(path, str):
                errors.append(f"registry source path malformed: {identity}")
                continue
            row = classifications.get(path, {})
            if not isinstance(row, Mapping):
                errors.append(
                    f"required source classification schema is not an object: {path}"
                )
                continue
            bindings = row.get("findings", [row.get("finding")])
            if (
                row.get("disposition") != "positive"
                or not isinstance(bindings, list)
                or identity not in bindings
            ):
                errors.append(
                    f"required GPIO entrypoint declassified: {identity} in {path}"
                )
    return errors


def _baseline_objects(root: Path) -> dict[str, str]:
    """Read all baseline paths directly from the pinned Git tree, not a query."""
    result = RunningProcess.run(
        ["git", "ls-tree", "-r", "-z", BASELINE_COMMIT, "--", "src"],
        cwd=root,
        check=True,
        timeout=30,
        capture_output=True,
        encoding="utf-8",
        errors="strict",
    )
    objects: dict[str, str] = {}
    for entry in result.stdout.split("\0"):
        if not entry:
            continue
        metadata, path = entry.split("\t", 1)
        mode, kind, object_id = metadata.split()
        if kind != "blob" or mode == "120000":
            raise ValueError(f"unexpected source object in pinned baseline: {path}")
        objects[path] = object_id
    if len(objects) != BASELINE_SOURCE_COUNT:
        raise ValueError(
            f"baseline universe mismatch: {len(objects)} != {BASELINE_SOURCE_COUNT}"
        )
    return objects


def _current_sources(root: Path) -> dict[str, bytes]:
    """Include tracked and nonignored untracked source, not generated deps.

    --cached retains tracked paths even when a rule matches them. No source
    extension, SPI token or directory-name discovery filter is used here.
    Ignored source boundaries/policy are independently checked by the CLI.
    """
    result = RunningProcess.run(
        [
            "git",
            "ls-files",
            "-z",
            "--cached",
            "--others",
            "--exclude-standard",
            "--",
            "src",
        ],
        cwd=root,
        check=True,
        timeout=30,
        capture_output=True,
        encoding="utf-8",
        errors="strict",
    )
    sources: dict[str, bytes] = {}
    for name in sorted(set(result.stdout.split("\0")) - {""}):
        path = root / name
        if path.is_symlink():
            raise ValueError(f"source symlink requires explicit audit: {path}")
        if path.is_file():
            sources[name] = path.read_bytes()
    return sources


def _ignored_boundaries(root: Path) -> list[str]:
    result = RunningProcess.run(
        [
            "git",
            "ls-files",
            "-z",
            "--others",
            "--ignored",
            "--exclude-standard",
            "--directory",
            "--no-empty-directory",
            "--",
            "src",
        ],
        cwd=root,
        check=True,
        timeout=30,
        capture_output=True,
        encoding="utf-8",
        errors="strict",
    )
    return sorted(set(result.stdout.split("\0")) - {""})


def _check_ignored_boundaries(boundaries: Sequence[str]) -> list[str]:
    generated_prefixes = (
        "src/platforms/wasm/compiler/node_modules/",
        "src/platforms/wasm/compiler/dist/",
    )
    return [
        f"ignored src boundary requires independent review: {path}"
        for path in boundaries
        if not path.startswith(generated_prefixes)
    ]


def _check_ignore_policy(root: Path) -> list[str]:
    result = RunningProcess.run(
        ["git", "rev-parse", f"{BASELINE_COMMIT}:.gitignore"],
        cwd=root,
        check=True,
        timeout=30,
        capture_output=True,
        encoding="utf-8",
        errors="strict",
    )
    policy = root / ".gitignore"
    if (
        not policy.is_file()
        or _git_blob_digest(policy.read_bytes()) != result.stdout.strip()
    ):
        return [
            "root .gitignore policy changed; ignored source exclusion requires review"
        ]
    return []


def _git_blob_digest(source: bytes) -> str:
    # The pinned repository uses Git SHA-1 object identities. This is an object
    # integrity comparison, not a cryptographic authentication/security scheme.
    header = f"blob {len(source)}\0".encode("ascii")
    return hashlib.sha1(header + source, usedforsecurity=False).hexdigest()


def _source_changes(
    baseline: Mapping[str, str], current: Mapping[str, bytes]
) -> list[str]:
    errors: list[str] = []
    for path in sorted(set(baseline) | set(current)):
        if path not in baseline:
            errors.append(f"new source requires run-backed disposition: {path}")
        elif path not in current:
            errors.append(f"deleted source requires run-backed disposition: {path}")
        elif _git_blob_digest(current[path]) != baseline[path]:
            errors.append(
                f"changed/rerouted source requires run-backed disposition: {path}"
            )
    return errors


def _read_baseline_blob(root: Path, object_id: str) -> bytes:
    """Read actual binary blob content, never substitute the Git OID string."""
    if not isinstance(object_id, str) or not re.fullmatch(r"[0-9a-f]{40}", object_id):
        raise ValueError("invalid pinned baseline Git blob identity")
    cache_dir = root / ".build" / "software-spi-baseline-blobs"
    cache_dir.mkdir(parents=True, exist_ok=True)
    cache = cache_dir / f"{object_id}.blob"
    if cache.is_file():
        cached = cache.read_bytes()
        if _git_blob_digest(cached) == object_id:
            return cached

    # The installed RunningProcess capture API reconstructs line output, even
    # in binary mode. Redirect through existing Bash instead of decoding a blob.
    # Each call owns a closed staging file; concurrent readers never share it.
    with NamedTemporaryFile(
        dir=cache_dir, prefix=f".{object_id}-", suffix=".stage", delete=False
    ) as staging_file:
        staging = Path(staging_file.name)
    try:
        RunningProcess.run(
            [
                "bash",
                "-c",
                'git cat-file blob "$1" > "$2"',
                "spi-audit-read-blob",
                object_id,
                staging.resolve().as_posix(),
            ],
            cwd=root,
            check=True,
            timeout=30,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
        )
        source = staging.read_bytes()
        if _git_blob_digest(source) != object_id:
            raise ValueError(f"pinned baseline blob byte integrity failed: {object_id}")
        staging.replace(cache)
        return source
    finally:
        if staging.exists():
            # One failed artifact per OID bounds diagnostic output; unique
            # partial staging files do not accumulate after failed reads.
            staging.replace(cache_dir / f"{object_id}.failed")


def _baseline_sources(
    root: Path, baseline: Mapping[str, str], current: Mapping[str, bytes]
) -> dict[str, bytes]:
    sources: dict[str, bytes] = {}
    for path, object_id in baseline.items():
        source = current.get(path)
        if source is None or _git_blob_digest(source) != object_id:
            source = _read_baseline_blob(root, object_id)
        sources[path] = source
    return sources


BASELINE_CLASSIFICATION_ROLES = frozenset(
    {
        "negative",
        "dependency",
        "forwarding-alias",
        "dispatch",
        "delay",
        "implementation",
        "pin-map",
    }
)


@dataclass
class BaselineClassifications:
    rows: dict[str, object]
    errors: list[str]
    versioned: bool = True


def _reviewed_files(document: object) -> Mapping[str, object]:
    if not isinstance(document, Mapping) or set(document) != {
        "schema_version",
        "baseline",
        "files",
    }:
        raise ValueError(
            "reviewed classification document requires exact schema_version/baseline/files keys"
        )
    if type(document["schema_version"]) is not int or document["schema_version"] != 1:
        raise ValueError("reviewed classification schema_version must be integer 1")
    if document["baseline"] != BASELINE_COMMIT:
        raise ValueError(
            "reviewed classification baseline must match the pinned Git commit"
        )
    files = document["files"]
    if not isinstance(files, Mapping) or any(
        not isinstance(path, str) for path in files
    ):
        raise ValueError("reviewed files must map string paths to classification rows")
    return files


def _check_reviewed_byte_anchor(anchor: Mapping[str, object], source: bytes) -> None:
    if set(anchor) != {"kind", "offset", "hex"} or anchor["kind"] != "bytes":
        raise ValueError("byte anchor requires exact kind/offset/hex keys")
    offset, encoded = anchor["offset"], anchor["hex"]
    if type(offset) is not int or offset < 0:
        raise ValueError("byte anchor offset must be a nonnegative integer")
    if (
        not isinstance(encoded, str)
        or re.fullmatch(r"(?:[0-9a-f]{2})+", encoded) is None
    ):
        raise ValueError(
            "byte anchor hex must be nonempty canonical lowercase byte pairs"
        )
    expected = bytes.fromhex(encoded)
    if (
        offset + len(expected) > len(source)
        or source[offset : offset + len(expected)] != expected
    ):
        raise ValueError("byte anchor must equal actual in-bounds baseline bytes")


def _check_reviewed_anchors(evidence: object, source: bytes) -> None:
    if not isinstance(evidence, list) or not evidence:
        raise ValueError("source_evidence must contain literal baseline anchors")
    lines = source.splitlines()
    for anchor in evidence:
        if isinstance(anchor, Mapping) and "kind" in anchor:
            _check_reviewed_byte_anchor(anchor, source)
            continue
        if not isinstance(anchor, Mapping) or set(anchor) != {"line", "text"}:
            raise ValueError("source anchor requires exact line/text keys")
        line, text = anchor["line"], anchor["text"]
        if type(line) is not int or line < 1 or line > len(lines):
            raise ValueError("source anchor line must identify an actual baseline line")
        if (
            not isinstance(text, str)
            or not text.strip()
            or text != lines[line - 1].decode("utf-8", errors="replace")
        ):
            raise ValueError("source anchor text must equal its literal baseline line")


def _reviewed_row(row: object, source: bytes, object_id: str) -> dict[str, object]:
    if not isinstance(row, Mapping):
        raise ValueError("reviewed classification row must be an object")
    fields = {
        "baseline_blob_oid",
        "baseline_sha256",
        "disposition",
        "role",
        "findings",
        "reason",
        "source_evidence",
    }
    if row.get("disposition") == "alias":
        fields.add("canonical")
    if set(row) != fields:
        raise ValueError(
            "reviewed row has missing/unknown keys; canonical belongs only to aliases"
        )
    if row["baseline_blob_oid"] != object_id or row["baseline_sha256"] != source_digest(
        source
    ):
        raise ValueError("reviewed row must bind the actual pinned Git blob and SHA256")
    disposition, role = row["disposition"], row["role"]
    if not isinstance(disposition, str) or disposition not in DISPOSITIONS | {
        "unknown"
    }:
        raise ValueError("reviewed row has an unknown disposition")
    if not isinstance(role, str) or role not in BASELINE_CLASSIFICATION_ROLES:
        raise ValueError("reviewed row has an unknown role")
    if not isinstance(row["reason"], str) or not row["reason"].strip():
        raise ValueError("reviewed row requires a nonempty reason")
    bindings = row["findings"]
    known = {finding["id"] for finding in REQUIRED_FINDINGS}
    if not isinstance(bindings, list) or any(
        not isinstance(identity, str) or identity not in known for identity in bindings
    ):
        raise ValueError("reviewed findings must be a list of known finding identities")
    if len(set(bindings)) != len(bindings) or (
        disposition == "positive" and not bindings
    ):
        raise ValueError("reviewed findings must be unique and nonempty for positives")
    if disposition == "alias" and (
        not isinstance(row["canonical"], str) or not row["canonical"]
    ):
        raise ValueError("reviewed alias requires a canonical source path")
    _check_reviewed_anchors(row["source_evidence"], source)
    return {**row, "source_sha256": source_digest(source)}


def load_baseline_classifications(
    root: Path, document: object, current: Mapping[str, bytes]
) -> BaselineClassifications:
    """Bind review bookkeeping to the independent Git tree, never execution proof."""
    rows: dict[str, object] = {}
    errors: list[str] = []
    objects = _baseline_objects(root)
    try:
        files = _reviewed_files(document)
    except ValueError as error:
        return BaselineClassifications(
            rows, [f"reviewed classification schema: {error}"]
        )
    for path in sorted(set(files) - set(objects)):
        errors.append(f"reviewed classification outside pinned baseline: {path}")
    sources = _baseline_sources(root, objects, current)
    for path, object_id in objects.items():
        source = sources[path]
        if _git_blob_digest(source) != object_id:
            raise ValueError(f"pinned baseline blob byte integrity failed: {path}")
        if path not in files:
            errors.append(f"missing reviewed baseline classification: {path}")
            continue
        try:
            rows[path] = _reviewed_row(files[path], source, object_id)
        except ValueError as error:
            errors.append(f"invalid reviewed classification {path}: {error}")
    for path, row in rows.items():
        if not isinstance(row, Mapping):
            continue
        if row["disposition"] == "unknown":
            errors.append(f"unresolved unknown baseline classification: {path}")
        elif row["disposition"] == "alias":
            errors.extend(_check_alias(path, row, sources, rows))
    errors.extend(_source_changes(objects, current))
    return BaselineClassifications(rows, errors)


def _unique_json_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key!r}")
        result[key] = value
    return result


def _read_audit_json(path: Path) -> object:
    return json.loads(
        path.read_text(encoding="utf-8"), object_pairs_hook=_unique_json_object
    )


def _classification_input(
    root: Path, path: Path | None, current: Mapping[str, bytes]
) -> BaselineClassifications:
    if path is None:
        return BaselineClassifications({}, [], versioned=False)
    document = _read_audit_json(path)
    # Retain the original diagnostic path-keyed input, with all existing gate
    # checks. A versioned/malformed document can never fall back to raw rows.
    if isinstance(document, dict) and all(
        isinstance(key, str) and key.startswith("src/") for key in document
    ):
        return BaselineClassifications(document, [], versioned=False)
    return load_baseline_classifications(root, document, current)


def _native_support_input_paths(root: Path) -> list[str]:
    """All tracked/nonignored CI and test inputs plus root launch/build files."""
    result = RunningProcess.run(
        [
            "git",
            "ls-files",
            "-z",
            "--cached",
            "--others",
            "--exclude-standard",
            "--",
            "ci",
            "tests",
            "test",
            "test.py",
            "meson.build",
            "meson.options",
            "pyproject.toml",
            ".gitignore",
            "spi-audit",
            "ci-native",
        ],
        cwd=root,
        check=True,
        timeout=30,
        capture_output=True,
        encoding="utf-8",
        errors="strict",
    )
    return sorted(set(result.stdout.split("\0")) - {""})


def _native_input_hashes(root: Path, sources: Mapping[str, bytes]) -> dict[str, str]:
    inputs = {path: source_digest(data) for path, data in sorted(sources.items())}
    for name in _native_support_input_paths(root):
        path = root / name
        if path.is_symlink():
            raise ValueError(f"native support input symlink requires review: {name}")
        inputs[name] = source_digest(path.read_bytes())
    return inputs


def _check_native_inputs(root: Path, expected: Mapping[str, str]) -> None:
    try:
        actual = _native_input_hashes(root, _current_sources(root))
    except OSError as error:
        raise ValueError("native trace inputs changed or became unreadable") from error
    if actual != expected:
        raise ValueError("native trace inputs changed during execution")


def _run_native_traces(
    root: Path, sources: Mapping[str, bytes]
) -> list[dict[str, object]]:
    """Execute fixed in-repo tests; no command is accepted from a manifest."""
    commands = (
        ("bash", "test", "generic_software_spi"),
        ("bash", "test", "generic_software_spi_inverted", "--debug"),
        ("bash", "test", "forced_software_pin", "--debug"),
    )
    inputs = _native_input_hashes(root, sources)
    input_digest = source_digest(json.dumps(inputs, sort_keys=True).encode("utf-8"))
    records: list[dict[str, object]] = []

    def execute(command: tuple[str, ...]) -> int:
        result = RunningProcess.run(
            list(command),
            cwd=root,
            check=False,
            timeout=900,
            capture_output=True,
            encoding="utf-8",
            errors="replace",
        )
        mode = "debug" if "--debug" in command else "quick"
        log = root / ".build" / f"spi-audit-{command[2]}-{mode}.log"
        log.parent.mkdir(parents=True, exist_ok=True)
        output_bytes = result.stdout.encode("utf-8")
        log.write_bytes(output_bytes)
        records.append(
            {
                "kind": "host_production_gpio_trace",
                "command": list(command),
                "returncode": result.returncode,
                "input_sha256": input_digest,
                "log": log.relative_to(root).as_posix(),
                "log_sha256": source_digest(output_bytes),
                "scope": "host GPIO semantics only; no vendor, instruction or physical claim",
            }
        )
        # Source AND test/fixture changes invalidate the entire execution before
        # another target (or a debug rerun) can be mistaken for comparable proof.
        _check_native_inputs(root, inputs)
        return result.returncode

    _check_native_inputs(root, inputs)
    for command in commands:
        returncode = execute(command)
        if returncode != 0 and "--debug" not in command:
            # Keep the quick failure and its distinct log/record even if debug
            # succeeds. A diagnostic rerun cannot turn the original failure green.
            execute((*command, "--debug"))
    return records


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--classifications", type=Path, help="review rows; never execution proof"
    )
    parser.add_argument(
        "--findings",
        type=Path,
        help="editable finding rows checked against required registry",
    )
    parser.add_argument(
        "--run-native",
        action="store_true",
        help="run only fixed production GPIO trace targets",
    )
    parser.add_argument(
        "--report", type=Path, default=ROOT / ".build/software-spi-audit-report.json"
    )
    args = parser.parse_args()
    baseline: dict[str, str] = {}
    current: dict[str, bytes] = {}
    rows: dict[str, object] = {}
    ignored: list[str] = []
    executions: list[dict[str, object]] = []
    errors: list[str] = []
    ignore_policy_sha256: str | None = None
    try:
        baseline = _baseline_objects(ROOT)
        current = _current_sources(ROOT)
        baseline_sources = _baseline_sources(ROOT, baseline, current)
        ignored = _ignored_boundaries(ROOT)
        classifications = _classification_input(ROOT, args.classifications, current)
        findings = (
            _read_audit_json(args.findings)
            if args.findings
            else list(REQUIRED_FINDINGS)
        )
        if not isinstance(findings, list):
            raise ValueError(
                "classifications must be an object and findings must be a list"
            )
        rows = classifications.rows
        for path, (disposition, reason) in PINNED_EXCLUSIONS.items():
            if (
                not classifications.versioned
                and path in current
                and path in baseline
                and _git_blob_digest(current[path]) == baseline[path]
            ):
                rows.setdefault(
                    path,
                    {
                        "disposition": disposition,
                        "reason": reason,
                        "source_sha256": source_digest(current[path]),
                    },
                )
        errors = _source_changes(baseline, current)
        errors.extend(classifications.errors)
        errors.extend(_check_ignored_boundaries(ignored))
        errors.extend(_check_ignore_policy(ROOT))
        errors.extend(check_inventory(baseline_sources, current, rows))
        errors.extend(check_required_findings(REQUIRED_FINDINGS, findings, rows))
        for path in sorted(SEMANTIC_REQUIRED_PATHS):
            if path not in current or path not in rows:
                errors.append(f"required semantic source not classified: {path}")
        # Passing source declarations cannot turn absent behavioral/configuration
        # evidence into success. Actual fixed executions are recorded separately.
        errors.extend(check_evidence(REQUIRED_FINDINGS, [], current))
        executions = _run_native_traces(ROOT, current) if args.run_native else []
        for execution in executions:
            if execution["returncode"] != 0:
                errors.append(f"fixed production trace failed: {execution['command']}")
        if (ROOT / ".gitignore").is_file():
            ignore_policy_sha256 = source_digest((ROOT / ".gitignore").read_bytes())
    except (OSError, ValueError, CalledProcessError, TimeoutExpired) as error:
        errors.append(f"audit input/runtime error: {error}")

    report = {
        "baseline": BASELINE_COMMIT,
        "baseline_files": len(baseline),
        "current_files": len(current),
        "ignored_generated_boundaries": ignored,
        "ignore_policy_sha256": ignore_policy_sha256,
        "required_findings": list(REQUIRED_FINDINGS),
        "source_accounting": {
            path: {
                "baseline_git_blob": baseline.get(path),
                "current_sha256": source_digest(current[path])
                if path in current
                else None,
                "classification": rows.get(path, {"disposition": "unknown"}),
            }
            for path in sorted(set(baseline) | set(current))
        },
        "executions": executions,
        "errors": errors,
        "complete": not errors,
        "limitation": "Foundation: target/codegen/restored-defect runners and run-backed source-change resolution remain unwired.",
    }
    try:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_bytes((json.dumps(report, indent=2) + "\n").encode("utf-8"))
    except OSError as error:
        print(f"Software SPI audit RED: could not write report: {error}")
        return 1
    print(
        f"Software SPI audit {'RED' if errors else 'PASS'}: "
        f"{len(baseline)} baseline files, {len(current)} current files, "
        f"{len(REQUIRED_FINDINGS)} required findings, {len(errors)} unresolved checks."
    )
    print(f"Full source accounting and evidence gaps: {args.report}")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
