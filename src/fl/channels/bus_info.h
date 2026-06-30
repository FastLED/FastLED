#pragma once

/// @file bus_info.h
/// @brief Portable bus/device introspection.

#include "fl/channels/bus.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"

namespace fl {

struct DeviceRuntime {
    bool available = false;
    bool initialized = false;
    bool ready = false;
    bool busy = false;
    const char* error = "";

    DeviceRuntime() FL_NO_EXCEPT = default;

    DeviceRuntime(bool availableValue, bool initializedValue,
                  bool readyValue, bool busyValue,
                  const char* errorValue) FL_NO_EXCEPT
        : available(availableValue),
          initialized(initializedValue),
          ready(readyValue),
          busy(busyValue),
          error(errorValue) {}
};

struct DeviceInfo {
    Bus bus = Bus::AUTO;
    fl::u8 which = 0;
    const char* bus_name = "AUTO";
    const char* vendor_name = "NOOP";
    const char* device_name = "no-op";
    const char* notes = "";
    bool is_noop = true;
    DeviceRuntime runtime;

    DeviceInfo() FL_NO_EXCEPT = default;

    DeviceInfo(Bus busValue, fl::u8 whichValue, const char* busNameValue,
               const char* vendorNameValue, const char* deviceNameValue,
               const char* notesValue, bool isNoopValue,
               const DeviceRuntime& runtimeValue) FL_NO_EXCEPT
        : bus(busValue),
          which(whichValue),
          bus_name(busNameValue),
          vendor_name(vendorNameValue),
          device_name(deviceNameValue),
          notes(notesValue),
          is_noop(isNoopValue),
          runtime(runtimeValue) {}
};

namespace detail {

inline DeviceRuntime availableRuntime() FL_NO_EXCEPT {
    return DeviceRuntime{true, false, true, false, ""};
}

inline DeviceRuntime unavailableRuntime() FL_NO_EXCEPT {
    return DeviceRuntime{false, false, false, false, "unavailable"};
}

inline DeviceInfo makeInfo(Bus bus, fl::u8 which, const char* vendor,
                              const char* device, const char* notes = "") FL_NO_EXCEPT {
    return DeviceInfo{bus, which, busName(bus, which), vendor, device, notes,
                      false, availableRuntime()};
}

inline DeviceInfo makeNoop(Bus bus, fl::u8 which, const char* notes = "") FL_NO_EXCEPT {
    return DeviceInfo{bus, which, busName(bus, which), "NOOP", "no-op", notes,
                      true, unavailableRuntime()};
}

template<Bus B, fl::u8 Which>
struct DeviceInfoResolver {
    static inline DeviceInfo get() FL_NO_EXCEPT {
        return makeNoop(B, Which);
    }
};

template<fl::u8 Which>
struct DeviceInfoResolver<Bus::AUTO, Which> {
    static inline DeviceInfo get() FL_NO_EXCEPT {
        return DeviceInfo{Bus::AUTO, Which, "AUTO", "AUTO",
                          "(auto -- resolves per chipset)", "",
                          false, availableRuntime()};
    }
};

template<fl::u8 Which>
struct DeviceInfoResolver<Bus::BIT_BANG, Which> {
    static inline DeviceInfo get() FL_NO_EXCEPT {
        return Which == 0
            ? makeInfo(Bus::BIT_BANG, Which, "BIT_BANG", "portable bit-bang")
            : makeNoop(Bus::BIT_BANG, Which, "out of range");
    }
};

#if defined(FL_IS_TEENSY_4X)

template<> struct DeviceInfoResolver<Bus::FLEX_IO, 0> {
    static inline DeviceInfo get() FL_NO_EXCEPT {
        return makeInfo(Bus::FLEX_IO, 0, "OBJECT_FLED", "ObjectFLED");
    }
};

template<> struct DeviceInfoResolver<Bus::FLEX_IO, 1> {
    static inline DeviceInfo get() FL_NO_EXCEPT {
        return makeInfo(Bus::FLEX_IO, 1, "FLEXIO", "FlexIO2");
    }
};

template<> struct DeviceInfoResolver<Bus::SPI, 0> {
    static inline DeviceInfo get() FL_NO_EXCEPT {
        return makeInfo(Bus::SPI, 0, "LPSPI", "LPSPI4");
    }
};

template<> struct DeviceInfoResolver<Bus::SPI, 1> {
    static inline DeviceInfo get() FL_NO_EXCEPT {
        return makeInfo(Bus::SPI, 1, "LPSPI", "LPSPI1");
    }
};

template<> struct DeviceInfoResolver<Bus::SPI, 2> {
    static inline DeviceInfo get() FL_NO_EXCEPT {
        return makeInfo(Bus::SPI, 2, "LPSPI", "LPSPI2");
    }
};

template<> struct DeviceInfoResolver<Bus::SPI, 3> {
    static inline DeviceInfo get() FL_NO_EXCEPT {
        return makeInfo(Bus::SPI, 3, "LPSPI", "LPSPI3");
    }
};

template<> struct DeviceInfoResolver<Bus::UART, 0> {
    static inline DeviceInfo get() FL_NO_EXCEPT {
        return makeInfo(Bus::UART, 0, "LPUART", "LPUART parallel");
    }
};

#elif defined(FL_IS_ESP32)

template<> struct DeviceInfoResolver<Bus::RMT, 0> {
    static inline DeviceInfo get() FL_NO_EXCEPT {
#if FASTLED_HAS_RMT5
        return makeInfo(Bus::RMT, 0, "RMT5", "ESP32 RMT5");
#elif FASTLED_HAS_RMT4
        return makeInfo(Bus::RMT, 0, "RMT4", "ESP32 RMT4");
#else
        return makeNoop(Bus::RMT, 0, "RMT unavailable");
#endif
    }
};

template<> struct DeviceInfoResolver<Bus::FLEX_IO, 0> {
    static inline DeviceInfo get() FL_NO_EXCEPT {
#if FASTLED_HAS_PARLIO
        return makeInfo(Bus::FLEX_IO, 0, "PARLIO", "ESP32 PARLIO");
#elif FASTLED_HAS_LCD_SPI || FASTLED_HAS_LCD_CAM_CLOCKLESS
        return makeInfo(Bus::FLEX_IO, 0, "LCD_CAM", "ESP32-S3 LCD_CAM");
#elif FASTLED_HAS_I2S_SPI
        return makeInfo(Bus::FLEX_IO, 0, "I2S", "ESP32 I2S parallel SPI");
#else
        return makeNoop(Bus::FLEX_IO, 0, "parallel I/O unavailable");
#endif
    }
};

template<> struct DeviceInfoResolver<Bus::FLEX_IO, 1> {
    static inline DeviceInfo get() FL_NO_EXCEPT {
#if FASTLED_HAS_LCD_CAM_CLOCKLESS || FASTLED_HAS_LCD_SPI
        return makeInfo(Bus::FLEX_IO, 1, "LCD_CAM", "ESP32-S3 LCD_CAM secondary");
#else
        return makeNoop(Bus::FLEX_IO, 1, "secondary parallel I/O unavailable");
#endif
    }
};

template<> struct DeviceInfoResolver<Bus::SPI, 0> {
    static inline DeviceInfo get() FL_NO_EXCEPT {
        return makeInfo(Bus::SPI, 0, "ESP_SPI_HW", "ESP32 SPI");
    }
};

template<> struct DeviceInfoResolver<Bus::SPI, 1> {
    static inline DeviceInfo get() FL_NO_EXCEPT {
#if defined(FL_IS_ESP_32S3)
        return makeInfo(Bus::SPI, 1, "ESP_SPI_HW", "ESP32-S3 SPI2");
#else
        return makeNoop(Bus::SPI, 1, "out of range");
#endif
    }
};

template<> struct DeviceInfoResolver<Bus::UART, 0> {
    static inline DeviceInfo get() FL_NO_EXCEPT {
        return makeInfo(Bus::UART, 0, "ESP32_UART", "UART0");
    }
};

template<> struct DeviceInfoResolver<Bus::UART, 1> {
    static inline DeviceInfo get() FL_NO_EXCEPT {
#if defined(FL_IS_ESP_32S3) || defined(FL_IS_ESP_32C6)
        return makeInfo(Bus::UART, 1, "ESP32_UART", "UART1");
#else
        return makeNoop(Bus::UART, 1, "out of range");
#endif
    }
};

template<> struct DeviceInfoResolver<Bus::UART, 2> {
    static inline DeviceInfo get() FL_NO_EXCEPT {
#if defined(FL_IS_ESP_32S3)
        return makeInfo(Bus::UART, 2, "ESP32_UART", "UART2");
#else
        return makeNoop(Bus::UART, 2, "out of range");
#endif
    }
};

#endif

}  // namespace detail

template<Bus B, fl::u8 Which = 0>
inline DeviceInfo deviceInfo() FL_NO_EXCEPT {
    return detail::DeviceInfoResolver<B, Which>::get();
}

}  // namespace fl
