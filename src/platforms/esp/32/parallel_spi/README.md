# ESP32-C3/C2 Parallel SPI ISR Driver

High-priority interrupt-based parallel soft-SPI driver for ESP32-C3 and ESP32-C2 RISC-V platforms.

# Test running environment

  * QEMU RISC-V pre-existing test
    * Find the current github action for C3
    * add the Esp32C3_SPI_ISR.ino test
    * update the test to produce output that the tester (toboozo) will regex for
