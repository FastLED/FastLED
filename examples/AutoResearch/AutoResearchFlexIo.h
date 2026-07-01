/// @file AutoResearchFlexIo.h
/// @brief `Bus::FLEX_IO` runtime-binding introspection RPC handler
///        (FastLED#3515 Phase B).
///
/// Follows the `Bus::FLEX_IO` runtime-selection convention documented
/// in `agents/docs/cpp-standards.md` -> "Runtime driver selection is
/// `Bus::FLEX_IO` — legacy chipset templates are not (REQUIRED)". The
/// harness is peripheral-agnostic:
///
/// - On classic ESP32 `Bus::FLEX_IO, 0` currently routes to the I2S-SPI
///   driver (`I2S_SPI`); the Yves parallel-out clockless driver joins
///   the same slot once FastLED#3512 Phase 4 lands.
/// - On ESP32-P4 / C6 / H2 / C5 it routes to `PARLIO`.
/// - On ESP32-S3 it routes to `LCD_CLOCKLESS` or `LCD_RGB` depending
///   on which is registered.
/// - On Teensy 4.x it routes to `FLEX_IO` (FlexIO2) or `OBJECT_FLED`
///   depending on instance number.
///
/// The `flexIoDeviceInfo` RPC returns a device-info JSON structure the
/// host can inspect to see which concrete peripheral is bound.
/// Subtype details (I2S port, DMA channels, pin routing) are the
/// responsibility of a future driver-specific `getDeviceInfo()` method
/// — this v1 uses the existing `IChannelDriver::getName()` + build-time
/// preprocessor macros.

#pragma once

#include "FastLED.h"
#include "fl/channels/bus.h"
#include "fl/channels/manager.h"
#include "fl/stl/int.h"
#include "fl/stl/json.h"
#include "fl/stl/string.h"
#include "platforms/is_platform.h"

namespace autoresearch {
namespace flex_io {

/// Platform identification string derived from build-time macros.
inline fl::string siliconName() FL_NO_EXCEPT {
#if defined(FL_IS_ESP_32DEV)
    return fl::string::from_literal("ESP32");
#elif defined(FL_IS_ESP_32S2)
    return fl::string::from_literal("ESP32-S2");
#elif defined(FL_IS_ESP_32S3)
    return fl::string::from_literal("ESP32-S3");
#elif defined(FL_IS_ESP_32C3)
    return fl::string::from_literal("ESP32-C3");
#elif defined(FL_IS_ESP_32C5)
    return fl::string::from_literal("ESP32-C5");
#elif defined(FL_IS_ESP_32C6)
    return fl::string::from_literal("ESP32-C6");
#elif defined(FL_IS_ESP_32H2)
    return fl::string::from_literal("ESP32-H2");
#elif defined(FL_IS_ESP_32P4)
    return fl::string::from_literal("ESP32-P4");
#elif defined(FL_IS_TEENSY_4X)
    return fl::string::from_literal("Teensy-4.x");
#elif defined(FL_IS_STUB)
    return fl::string::from_literal("Host-stub");
#else
    return fl::string::from_literal("Unknown");
#endif
}

/// ESP-IDF version string, or "n/a" on non-ESP32 targets.
inline fl::string idfVersion() FL_NO_EXCEPT {
#if defined(FL_IS_ESP32) && defined(ESP_IDF_VERSION_MAJOR)
    fl::sstream s;
    s << ESP_IDF_VERSION_MAJOR;
#if defined(ESP_IDF_VERSION_MINOR)
    s << '.' << ESP_IDF_VERSION_MINOR;
#endif
#if defined(ESP_IDF_VERSION_PATCH)
    s << '.' << ESP_IDF_VERSION_PATCH;
#endif
    return s.str();
#else
    return fl::string::from_literal("n/a");
#endif
}

/// Return the list of registered driver names. The FLEX_IO slot's
/// binding is inferred by name — callers can match against the
/// silicon-specific expected name (`I2S_SPI` / `PARLIO` / `LCD_RGB`
/// / `LCD_CLOCKLESS` / etc.) or just report the entire list.
///
/// Direct `Bus::FLEX_IO, instance` introspection requires the driver
/// to grow a `getBus() / getInstance()` method — currently
/// `IChannelDriver::getName()` is the only public identity. When those
/// getters arrive, this helper collapses to a one-line lookup; for now
/// it returns all registered names so the host can pick.
inline fl::vector<fl::string> registeredDriverNames() FL_NO_EXCEPT {
    fl::vector<fl::string> names;
    auto& manager = fl::channelManager();
    auto infos = manager.getDriverInfos();
    for (const auto& info : infos) {
        if (!info.name.empty()) {
            names.push_back(info.name);
        }
    }
    return names;
}

/// Convenience — first non-empty driver name, or "<none>" if empty.
inline fl::string firstDriverName() FL_NO_EXCEPT {
    auto names = registeredDriverNames();
    if (names.empty()) {
        return fl::string::from_literal("<none>");
    }
    return names[0];
}

/// Build the device-info JSON returned by the `flexIoDeviceInfo` RPC.
///
/// Shape:
/// ```json
/// {
///   "bus": "FLEX_IO",
///   "instance": 0,
///   "driverName": "I2S_SPI",          // whatever IChannelDriver::getName() returns
///   "silicon": "ESP32",
///   "idfVersion": "5.5.4",
///   "supportedByBuild": true          // false when no BusTraits<FLEX_IO, N> for this silicon
/// }
/// ```
inline fl::json buildDeviceInfo(fl::u8 instance) FL_NO_EXCEPT {
    fl::json info = fl::json::object();
    info.set("bus", "FLEX_IO");
    info.set("instance", static_cast<int64_t>(instance));
    // firstDriverName gives the most-preferred registered driver's
    // name — on classic ESP32 this is typically "I2S_SPI" for the
    // FLEX_IO slot; on P4/C6/etc. "PARLIO"; on S3 "LCD_CLOCKLESS".
    info.set("driverName", firstDriverName());
    // Full driver list so the host can pick the FLEX_IO-bound one by
    // name if firstDriverName isn't the FLEX_IO owner.
    fl::json names_arr = fl::json::array();
    for (const auto& n : registeredDriverNames()) {
        names_arr.push_back(n);
    }
    info.set("registeredDriverNames", names_arr);
    info.set("silicon", siliconName());
    info.set("idfVersion", idfVersion());
    info.set("supportedByBuild",
             firstDriverName() != fl::string::from_literal("<none>"));
    return info;
}

}  // namespace flex_io
}  // namespace autoresearch
