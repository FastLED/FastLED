// IWYU pragma: private

/// @file lpuart_peripheral_real.cpp.hpp
/// @brief Concrete ILPUARTPeripheral implementation on Teensy 4.x hardware.

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#include "platforms/arm/teensy/teensy4_common/drivers/lpuart/ilpuart_peripheral.h"
#include "platforms/arm/teensy/teensy4_common/drivers/lpuart/lpuart_driver.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/noexcept.h"

namespace fl {

// LPUARTInstanceReal: owns an RAII u8[] buffer sized for the ENCODED
// (wave8) UART byte stream. The engine pre-encodes raw WS2812 bytes
// -> 4 UART bytes each into getTxBuffer(); show() DMAs the encoded
// buffer to LPUARTn_DATA.
class LPUARTInstanceReal : public ILPUARTInstance {
public:
    LPUARTInstanceReal(u8 tx_pin, u32 total_leds, bool is_rgbw,
                       u32 t1_ns, u32 t2_ns, u32 t3_ns, u32 reset_us) FL_NO_EXCEPT
        : mTxPin(tx_pin),
          mEncodedBytes(total_leds * (is_rgbw ? 4u : 3u) * 4u),
          mResetUs(reset_us),
          mInitialized(false),
          mTxBuffer() {
        (void)t1_ns; (void)t2_ns; (void)t3_ns;
        mTxBuffer = fl::make_unique<u8[]>(mEncodedBytes);
        LpuartPinInfo info{};
        if (lpuart_lookup_pin(tx_pin, &info)) {
            mInitialized = lpuart_init(info, mResetUs);
        }
    }

    ~LPUARTInstanceReal() override {
        if (mInitialized) lpuart_deinit();
    }

    bool isInitialized() const FL_NO_EXCEPT { return mInitialized; }

    u8* getTxBuffer() FL_NO_EXCEPT override { return mTxBuffer.get(); }
    u32 getTxBufferSize() const FL_NO_EXCEPT override { return mEncodedBytes; }

    void show() FL_NO_EXCEPT override {
        if (!mInitialized) return;
        lpuart_show_encoded(mTxBuffer.get(), mEncodedBytes);
        lpuart_wait();
    }

private:
    u8 mTxPin;
    u32 mEncodedBytes;
    u32 mResetUs;
    bool mInitialized;
    fl::unique_ptr<u8[]> mTxBuffer;
};

class LPUARTPeripheralReal : public ILPUARTPeripheral {
public:
    LPUARTPeripheralReal() = default;
    ~LPUARTPeripheralReal() override = default;

    LPUARTPinResult validatePin(u8 pin) const FL_NO_EXCEPT override {
        LpuartPinInfo info{};
        if (lpuart_lookup_pin(pin, &info)) {
            return {true, nullptr};
        }
        return {false, "pin not LPUART-TX capable on iMXRT1062"};
    }

    fl::unique_ptr<ILPUARTInstance> createInstance(
            u8 tx_pin, u32 total_leds, bool is_rgbw,
            u32 t1_ns, u32 t2_ns, u32 t3_ns, u32 reset_us) FL_NO_EXCEPT override {
        if (!validatePin(tx_pin).valid) return nullptr;
        const u32 bytes_per_led = is_rgbw ? 4u : 3u;
        if (total_leds > (0xFFFFFFFFu / bytes_per_led) / 4u) return nullptr;
        auto instance = fl::make_unique<LPUARTInstanceReal>(
            tx_pin, total_leds, is_rgbw, t1_ns, t2_ns, t3_ns, reset_us);
        // CodeRabbit-flagged: if the hardware init inside the
        // constructor failed (bad pin lookup, DMA exhaustion, etc.)
        // return nullptr rather than handing back a non-functional
        // instance that would silently drop frames.
        if (!instance || !instance->isInitialized()) return nullptr;
        return instance;
    }
};

fl::shared_ptr<ILPUARTPeripheral> ILPUARTPeripheral::create() FL_NO_EXCEPT {
    return fl::make_shared<LPUARTPeripheralReal>();
}

} // namespace fl

#endif // FL_IS_TEENSY_4X
