// src/fl/channels/detail/validation/platform.cpp.hpp
//
// Platform-specific validation implementation

#include "platform.h"
#include "fl/stl/sstream.h"
#include "fl/log.h"

// Platform detection macros (FL_IS_ESP_32*) are defined via build system
// or in platform-specific headers that are automatically included

namespace fl {
namespace validation {

inline vector<string> getExpectedEngines() {
    fl::vector<fl::string> expected;

#if defined(FL_IS_ESP_32C6)
    // ESP32-C6: PARLIO, RMT (SPI disabled - only 1 host, RMT5 preferred)
    expected.push_back("PARLIO");
    expected.push_back("RMT");
#elif defined(FL_IS_ESP_32S3)
    // ESP32-S3: SPI, RMT, I2S (I2S uses LCD_CAM peripheral)
    expected.push_back("SPI");
    expected.push_back("RMT");
    expected.push_back("I2S");
#elif defined(FL_IS_ESP_32C3)
    // ESP32-C3: RMT (no PARLIO, SPI available but not prioritized)
    expected.push_back("RMT");
#elif defined(FL_IS_ESP_32DEV)
    // Original ESP32: SPI, RMT (no PARLIO)
    expected.push_back("SPI");
    expected.push_back("RMT");
    // expected.push_back("I2S");  // I2S support varies
#endif

    return expected;
}

inline bool validateExpectedEngines(const fl::vector<fl::DriverInfo>& available_drivers) {
    auto expected = getExpectedEngines();

    // If no expected engines defined (unknown platform), return true
    if (expected.empty()) {
        return true;
    }

    bool all_present = true;
    for (fl::size i = 0; i < expected.size(); i++) {
        const fl::string& expected_name = expected[i];
        bool found = false;

        for (fl::size j = 0; j < available_drivers.size(); j++) {
            if (available_drivers[j].name == expected_name) {
                found = true;
                break;
            }
        }

        if (!found) {
            all_present = false;
            break;
        }
    }

    return all_present;
}

inline void printEngineValidation(const fl::vector<fl::DriverInfo>& available_drivers) {
    auto expected = getExpectedEngines();

    // Print platform info
#if defined(FL_IS_ESP_32C6)
    FL_WARN("\n[VALIDATION] Platform: ESP32-C6");
#elif defined(FL_IS_ESP_32S3)
    FL_WARN("\n[VALIDATION] Platform: ESP32-S3");
#elif defined(FL_IS_ESP_32C3)
    FL_WARN("\n[VALIDATION] Platform: ESP32-C3");
#elif defined(FL_IS_ESP_32DEV)
    FL_WARN("\n[VALIDATION] Platform: ESP32 (classic)");
#else
    FL_WARN("\n[VALIDATION] Platform: Unknown ESP32 variant - skipping engine validation");
    return;
#endif

    // Print expected engines
    fl::sstream ss;
    ss << "[VALIDATION] Expected engines: " << expected.size() << "\n";
    for (fl::size i = 0; i < expected.size(); i++) {
        ss << "  - " << expected[i].c_str() << "\n";
    }
    FL_WARN(ss.str());

    // Validate each expected engine
    bool all_present = true;
    for (fl::size i = 0; i < expected.size(); i++) {
        const fl::string& expected_name = expected[i];
        bool found = false;

        for (fl::size j = 0; j < available_drivers.size(); j++) {
            if (available_drivers[j].name == expected_name) {
                found = true;
                break;
            }
        }

        if (!found) {
            FL_ERROR("Expected engine '" << expected_name.c_str() << "' is MISSING from available drivers!");
            all_present = false;
        }
    }

    if (all_present) {
        FL_WARN("[VALIDATION] ✓ All expected engines are available");
    } else {
        FL_ERROR("Engine validation FAILED - some expected engines are missing!");
    }
}

}  // namespace validation
}  // namespace fl
