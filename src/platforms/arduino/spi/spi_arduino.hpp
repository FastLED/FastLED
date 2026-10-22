#pragma once

// IWYU pragma: private
// Explicit application adapter; never included by native ESP implementations.

#include "platforms/shared/spi_hw_1.h"
#include "platforms/esp/32/drivers/spi/spi_hw_base.h"
#include "fl/system/fastpin_base.h"
#include "fl/stl/memory.h"
// IWYU pragma: begin_keep
#include <SPI.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
// IWYU pragma: end_keep

namespace fl {
namespace platforms {

// An application mounts/configures its SPIClass first, then supplies the actual
// logical host and pin metadata here. Arduino exposes no pin getters. Every
// application client sharing this SPIClass must use this transaction guard.
class ArduinoSpiBus {
    fl::shared_ptr<SPIClass> mBus;
    SemaphoreHandle_t mMutex;
    u8 mHost;
    i8 mDataPin;
    i8 mClockPin;
public:
    ArduinoSpiBus(fl::shared_ptr<SPIClass> bus, u8 host, i8 dataPin,
                  i8 clockPin) FL_NO_EXCEPT
        : mBus(fl::move(bus)), mMutex(xSemaphoreCreateMutex()), mHost(host),
          mDataPin(dataPin), mClockPin(clockPin) {}
    ~ArduinoSpiBus() FL_NO_EXCEPT {
        if (mMutex) vSemaphoreDelete(mMutex);
    }
    ArduinoSpiBus(const ArduinoSpiBus&) = delete;
    ArduinoSpiBus& operator=(const ArduinoSpiBus&) = delete;

    bool valid() const FL_NO_EXCEPT {
        return mBus && mMutex && (mHost == 2 || (FASTLED_ESP32_HAS_SPI3 && mHost == 3)) &&
               GPIO_IS_VALID_OUTPUT_GPIO(mDataPin) && GPIO_IS_VALID_OUTPUT_GPIO(mClockPin) &&
               mDataPin != mClockPin;
    }
    u8 host() const FL_NO_EXCEPT { return mHost; }
    bool matches(const SpiHw1::Config& config) const FL_NO_EXCEPT {
        return valid() && config.bus_num == mHost &&
               config.data_pin == mDataPin && config.clock_pin == mClockPin &&
               config.clock_speed_hz && config.max_transfer_sz;
    }
    bool ready() FL_NO_EXCEPT {
        if (!valid() || xSemaphoreTake(mMutex, portMAX_DELAY) != pdTRUE) return false;
        const bool result = mBus->bus() != nullptr;
        xSemaphoreGive(mMutex);
        return result;
    }

    // Wrap an application library operation that manages its own Arduino
    // transactions (including reads). Return its success/failure. All clients
    // must share this guard; do not recursively enter it from the operation.
    template<class Operation>
    bool withBus(Operation operation) FL_NO_EXCEPT {
        if (!valid() || xSemaphoreTake(mMutex, portMAX_DELAY) != pdTRUE) return false;
        const bool result = mBus->bus() && operation(*mBus);
        xSemaphoreGive(mMutex);
        return result;
    }

    // One lock and one Arduino transaction for the entire span, including
    // optional application-wired CS/gating. No begin/end or pin takeover.
    // Arduino's void writeBytes cannot report a hardware partial transfer.
    bool write(fl::span<const u8> bytes, u32 clockHz, u8 bitOrder = SPI_MSBFIRST,
               u8 mode = SPI_MODE0,
               fl::shared_ptr<Selectable> select = {}) FL_NO_EXCEPT {
        if (!valid() || !clockHz || (bitOrder != SPI_MSBFIRST && bitOrder != SPI_LSBFIRST) ||
            (mode != SPI_MODE0 && mode != SPI_MODE1 && mode != SPI_MODE2 && mode != SPI_MODE3)) return false;
        if (!bytes.empty() && !bytes.data()) return false;
        return withBus([&](SPIClass& bus) FL_NO_EXCEPT -> bool {
            if (bytes.empty()) return true;
            bus.beginTransaction(SPISettings(clockHz, bitOrder, mode));
            if (select) select->select();
            while (!bytes.empty()) {
                const fl::size count = bytes.size() > 65536u ? 65536u : bytes.size();
                bus.writeBytes(bytes.data(), static_cast<u32>(count));
                bytes = bytes.subspan(count);
            }
            if (select) select->release();
            bus.endTransaction();
            return true;
        });
    }
};

// SpiHw1's ASYNC parameter is a hint. Arduino writes finish synchronously;
// keeping the staging buffer until waitComplete preserves the common lifetime
// contract without pretending that Arduino's blocking API provides async DMA.
class ArduinoSpiHw1 : public SpiHw1 {
    fl::shared_ptr<ArduinoSpiBus> mOwner;
    fl::shared_ptr<Selectable> mSelect;
    Config mConfig;
    DMABuffer mBuffer;
    bool mInitialized = false;
    bool mBufferReady = false;
public:
    ArduinoSpiHw1(fl::shared_ptr<ArduinoSpiBus> owner,
                  fl::shared_ptr<Selectable> select) FL_NO_EXCEPT
        : mOwner(fl::move(owner)), mSelect(fl::move(select)) {}
    ~ArduinoSpiHw1() FL_NO_EXCEPT override { end(); }
    bool begin(const Config& config) FL_NO_EXCEPT override {
        if (!mOwner || !mOwner->matches(config) || !mOwner->ready()) return false;
        if (mInitialized && (mConfig.bus_num != config.bus_num ||
            mConfig.data_pin != config.data_pin || mConfig.clock_pin != config.clock_pin ||
            mConfig.clock_speed_hz != config.clock_speed_hz ||
            mConfig.max_transfer_sz != config.max_transfer_sz)) return false;
        mConfig = config;
        mInitialized = true;
        return true;
    }
    void end() FL_NO_EXCEPT override {
        mBuffer.reset();
        mBufferReady = false;
        mInitialized = false;
    }
    DMABuffer acquireDMABuffer(fl::size bytes) FL_NO_EXCEPT override {
        if (!mInitialized) return DMABuffer(SPIError::NOT_INITIALIZED);
        if (bytes > mConfig.max_transfer_sz) return DMABuffer(SPIError::BUFFER_TOO_LARGE);
        mBuffer = DMABuffer(bytes);
        mBufferReady = mBuffer.ok() && mBuffer.size() == bytes;
        if (!mBufferReady) { mBuffer.reset(); return DMABuffer(SPIError::ALLOCATION_FAILED); }
        return mBuffer;
    }
    bool transmit(TransmitMode mode = TransmitMode::ASYNC) FL_NO_EXCEPT override {
        (void)mode;
        if (!mInitialized || !mBufferReady) return false;
        mBufferReady = false;
        return mOwner->write(mBuffer.data(), mConfig.clock_speed_hz,
                             SPI_MSBFIRST, SPI_MODE0, mSelect);
    }
    bool waitComplete(u32 timeout = (fl::numeric_limits<u32>::max)()) FL_NO_EXCEPT override {
        (void)timeout;
        mBuffer.reset();
        mBufferReady = false;
        return true;
    }
    bool isBusy() const FL_NO_EXCEPT override { return false; }
    bool isInitialized() const FL_NO_EXCEPT override { return mInitialized; }
    int getBusId() const FL_NO_EXCEPT override { return mOwner ? mOwner->host() : -1; }
    const char* getName() const FL_NO_EXCEPT override { return "ARDUINO_SPI"; }
};

inline fl::shared_ptr<SpiHw1> makeArduinoSpiHw1(
    fl::shared_ptr<ArduinoSpiBus> owner,
    fl::shared_ptr<Selectable> select = {}) FL_NO_EXCEPT {
    if (!owner || !owner->valid()) return {};
    return fl::make_shared<ArduinoSpiHw1>(fl::move(owner), fl::move(select));
}

} // namespace platforms
} // namespace fl
