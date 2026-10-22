#pragma once

// IWYU pragma: private

#include "fl/stl/cstddef.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"

namespace fl {

// Owns the native TinyUSB device stack for the lifetime of the firmware.
// Arduino USB startup and other device-stack owners must be disabled at build time.
class UsbCdcEsp32 {
public:
    UsbCdcEsp32() FL_NO_EXCEPT;
    UsbCdcEsp32(const UsbCdcEsp32&) = delete;
    UsbCdcEsp32& operator=(const UsbCdcEsp32&) = delete;

    void begin(u32 baudRate) FL_NO_EXCEPT { (void)baudRate; }
    bool setBaudRate(u32 baudRate) FL_NO_EXCEPT { (void)baudRate; return true; }
    int available() FL_NO_EXCEPT;
    int peek() FL_NO_EXCEPT;
    int read() FL_NO_EXCEPT;
    size_t write(const u8* buffer, size_t size) FL_NO_EXCEPT;
    void write(const char* str) FL_NO_EXCEPT;
    void writeln(const char* str) FL_NO_EXCEPT;
    bool flush(u32 timeoutMs = 1000) FL_NO_EXCEPT;
    bool isConnected() const FL_NO_EXCEPT;
    bool isReady() const FL_NO_EXCEPT { return isConnected(); }
    bool isBuffered() const FL_NO_EXCEPT { return mInitialized; }

private:
    // Opaque SDK semaphore handle; owned with the firmware-lifetime USB stack.
    void* mTxMutex = nullptr;
    bool mInitialized = false;
};

} // namespace fl
