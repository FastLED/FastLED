// Implementation of RISC-V interrupt helpers for ESP32-C3/C6
//
// This file implements the interrupt installation and RMT integration functions
// declared in riscv.hpp for ESP32-C3/C6 (RISC-V) platforms.
//
// NOTE: This file should only be compiled when targeting ESP32-C3/C6 platforms.
// On other platforms (host, other ESP32 variants), this file is a no-op.

// Only compile this file for ESP32-C3/C6 RISC-V platforms

// ok no namespace fl
#if defined(ESP32) && (defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6))

#include "riscv.hpp"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "soc/soc.h"
#include "fl/dbg.h"
#include "fl/log.h"

//=============================================================================
// INTERRUPT INSTALLATION HELPERS
//=============================================================================

esp_err_t fastled_riscv_install_interrupt(
    int source,
    int priority,
    void (*handler)(void *),
    void *arg,
    intr_handle_t *handle
) {
    if (priority < 1 || priority > FASTLED_RISCV_MAX_PRIORITY) {
        FL_LOG_INTERRUPT("Invalid priority level: " << priority <<
               " (must be 1-" << FASTLED_RISCV_MAX_PRIORITY << ")");
        return ESP_ERR_INVALID_ARG;
    }

    if (handler == nullptr || handle == nullptr) {
        FL_LOG_INTERRUPT("Invalid arguments: handler=" << (void*)handler <<
               " handle=" << (void*)handle);
        return ESP_ERR_INVALID_ARG;
    }

    // Configure interrupt flags
    // - ESP_INTR_FLAG_IRAM: Handler must be in IRAM (flash-safe)
    // - Level priorities: 1 (lowest) to 7 (highest)
    int flags = ESP_INTR_FLAG_IRAM;

    // Note: esp_intr_alloc doesn't have a direct priority parameter
    // Priority is set through flags: ESP_INTR_FLAG_LEVEL1 through ESP_INTR_FLAG_LEVEL7
    // But these are only documented for levels 1-3 in official API
    switch (priority) {
        case 1: flags |= ESP_INTR_FLAG_LEVEL1; break;
        case 2: flags |= ESP_INTR_FLAG_LEVEL2; break;
        case 3: flags |= ESP_INTR_FLAG_LEVEL3; break;
        case 4: flags |= ESP_INTR_FLAG_LEVEL4; break;
        case 5: flags |= ESP_INTR_FLAG_LEVEL5; break;
        case 6: flags |= ESP_INTR_FLAG_LEVEL6; break;
        case 7: flags |= ESP_INTR_FLAG_NMI; break; // Level 7 may be NMI
        default:
            FL_LOG_INTERRUPT("Unsupported priority level: " << priority);
            return ESP_ERR_NOT_SUPPORTED;
    }

    FL_LOG_INTERRUPT("Installing interrupt source=" << source <<
           " priority=" << priority << " flags=0x" << flags);

    esp_err_t err = esp_intr_alloc(source, flags, handler, arg, handle);

    if (err != ESP_OK) {
        FL_LOG_RMT("Failed to allocate interrupt: " << esp_err_to_name(err));
        return err;
    }

    FL_LOG_INTERRUPT("Interrupt installed successfully");
    return ESP_OK;
}

esp_err_t fastled_riscv_install_official_interrupt(
    int source,
    void (*handler)(void *),
    void *arg,
    intr_handle_t *handle
) {
    FL_LOG_INTERRUPT("Installing official FastLED interrupt (priority " <<
           FASTLED_RISCV_PRIORITY_RECOMMENDED << ")");
    return fastled_riscv_install_interrupt(
        source,
        FASTLED_RISCV_PRIORITY_RECOMMENDED,
        handler,
        arg,
        handle
    );
}

esp_err_t fastled_riscv_install_experimental_interrupt(
    int source,
    int priority,
    void (*handler)(void *),
    void *arg,
    intr_handle_t *handle
) {
    if (priority < 4 || priority > 7) {
        FL_LOG_RMT("Experimental priority must be 4-7, got " << priority);
        return ESP_ERR_INVALID_ARG;
    }

    // CRITICAL: According to official ESP-IDF documentation, priority levels 4-7
    // require ASSEMBLY handlers on BOTH Xtensa AND RISC-V architectures.
    // The ESP-IDF docs state: "handlers must be nullptr when an interrupt of level >3
    // is requested, because these types of interrupts aren't C-callable."
    //
    // This function CANNOT work as written because:
    // 1. esp_intr_alloc() will reject C function pointers for priority 4-7
    // 2. Assembly trampolines are required (not yet implemented for RISC-V)
    // 3. FastLED RMT driver doesn't support bypassing official RMT API
    //
    // TODO: Implement RISC-V assembly interrupt stubs (*.S files) if Level 4+
    // is proven necessary. For now, use priority 3 with official driver.
    FL_LOG_RMT("CANNOT INSTALL: Priority 4-7 requires ASSEMBLY handlers (not C)");
    FL_LOG_RMT("ESP-IDF docs: handlers must be nullptr for levels >3");
    FL_LOG_RMT("Use fastled_riscv_install_official_interrupt() for priority 1-3");

    return ESP_ERR_NOT_SUPPORTED;
}

//=============================================================================
// INTERRUPT HANDLER IMPLEMENTATIONS
//=============================================================================

void FL_IRAM fastled_riscv_official_handler(void *arg) {
    // Official handler implementation (priority 1-3)
    // This is a placeholder - actual RMT handling would go here
    (void)arg;

    // TODO: Implement actual RMT handling logic
    // For now, this is just a stub to verify interrupt installation works
}

void FL_IRAM fastled_riscv_experimental_handler(void *arg) {
    // CRITICAL: This C handler CANNOT be used for priority 4-7 interrupts.
    // According to ESP-IDF docs, priority >3 requires ASSEMBLY handlers.
    // This function exists only for API completeness but will never be called
    // because fastled_riscv_install_experimental_interrupt() returns ESP_ERR_NOT_SUPPORTED.
    //
    // TODO: If Level 4+ is needed, implement RISC-V assembly interrupt stub (*.S file)
    (void)arg;
}

//=============================================================================
// RMT INTEGRATION FUNCTIONS
//=============================================================================

esp_err_t fastled_riscv_rmt_init_official(
    int channel,
    int gpio_num,
    uint32_t resolution_hz,
    size_t mem_block_symbols,
    int priority_level
) {
    if (priority_level < 1 || priority_level > FASTLED_RISCV_PRIORITY_OFFICIAL_MAX) {
        FL_LOG_RMT("Official RMT priority must be 1-" <<
                FASTLED_RISCV_PRIORITY_OFFICIAL_MAX << ", got " << priority_level);
        return ESP_ERR_INVALID_ARG;
    }

    FL_LOG_INTERRUPT("Initializing RMT channel " << channel <<
           " on GPIO " << gpio_num <<
           " with priority " << priority_level);

    // TODO: Implement RMT initialization using official driver API
    // This would use rmt_tx_channel_config_t with intr_priority field
    // For now, return not implemented

    FL_LOG_RMT("fastled_riscv_rmt_init_official: NOT YET IMPLEMENTED");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t fastled_riscv_rmt_init_experimental(
    int channel,
    int gpio_num,
    uint32_t resolution_hz,
    size_t mem_block_symbols,
    int priority_level
) {
    if (priority_level < 4 || priority_level > 7) {
        FL_LOG_RMT("Experimental RMT priority must be 4-7, got " << priority_level);
        return ESP_ERR_INVALID_ARG;
    }

    // CRITICAL: Cannot implement because priority 4-7 requires ASSEMBLY handlers.
    // According to ESP-IDF documentation (v5.0+):
    // "Levels 1-3 can be handled in C. For levels 4-6... High level interrupts...
    //  Need to be handled in assembly."
    //
    // Implementation would require:
    // 1. RISC-V assembly interrupt stubs (*.S files) - NOT IMPLEMENTED
    // 2. Bypassing official RMT driver API - HIGH RISK
    // 3. Direct RMT register manipulation - FRAGILE
    // 4. Testing on real hardware with oscilloscope - NOT AVAILABLE
    //
    // Recommendation: Use fastled_riscv_rmt_init_official() with priority 1-3.
    // Test if Level 3 double-buffering is sufficient before pursuing Level 4+.
    FL_LOG_RMT("CANNOT IMPLEMENT: Priority 4-7 requires ASSEMBLY handlers");
    FL_LOG_RMT("Use fastled_riscv_rmt_init_official() with priority 1-3 instead");
    return ESP_ERR_NOT_SUPPORTED;
}

void FL_IRAM fastled_riscv_rmt_official_handler(void *arg) {
    // RMT official handler (priority 1-3)
    (void)arg;

    // TODO: Implement RMT-specific interrupt handling
    // This would handle RMT peripheral interrupts for LED timing
}

void FL_IRAM fastled_riscv_rmt_experimental_handler(void *arg) {
    // CRITICAL: This C handler CANNOT be used for priority 4-7 interrupts.
    // Priority >3 requires ASSEMBLY handlers per ESP-IDF documentation.
    // This function exists only for API completeness but will never be called.
    //
    // TODO: If Level 4+ is needed, implement RISC-V assembly interrupt stub (*.S file)
    (void)arg;
}

#endif // ESP32 && (ESP32-C3 || ESP32-C6)
