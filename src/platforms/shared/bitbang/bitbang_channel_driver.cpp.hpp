// IWYU pragma: private

/// @file bitbang_channel_driver.cpp.hpp
/// @brief Implementation of BitBangChannelDriver

#include "platforms/shared/bitbang/bitbang_channel_driver.h"
#include "fl/system/delay.h"
#include "fl/log.h"
#include "fl/pin.h"
#include "fl/stl/algorithm.h"
#include "platforms/avr/is_avr.h"

namespace fl {

BitBangChannelDriver::BitBangChannelDriver()
    : mNumActiveSlots(0), mActiveSlotMask(0) {
    for (int i = 0; i < 256; ++i) {
        mSlotForPin[i] = -1;
    }
    for (int i = 0; i < 8; ++i) {
        mPinForSlot[i] = -1;
    }
}

BitBangChannelDriver::~BitBangChannelDriver() = default;

bool BitBangChannelDriver::canHandle(const ChannelDataPtr& /*data*/) const {
    return true;
}

void BitBangChannelDriver::enqueue(ChannelDataPtr channelData) {
    if (channelData) {
        mEnqueuedChannels.push_back(fl::move(channelData));
    }
}

IChannelDriver::DriverState BitBangChannelDriver::poll() {
    return DriverState(DriverState::READY);
}

fl::string BitBangChannelDriver::getName() const {
    return fl::string::from_literal("BITBANG");
}

IChannelDriver::Capabilities BitBangChannelDriver::getCapabilities() const {
    return Capabilities(true, true);
}

void BitBangChannelDriver::rebuildPinConfig(
    fl::span<const ChannelDataPtr> channels) {
    // Clear previous mapping
    for (int i = 0; i < 256; ++i) {
        mSlotForPin[i] = -1;
    }
    for (int i = 0; i < 8; ++i) {
        mPinForSlot[i] = -1;
    }
    mNumActiveSlots = 0;
    mActiveSlotMask = 0;

    // Discover unique data pins
    for (fl::size i = 0; i < channels.size(); ++i) {
        const auto& ch = channels[i];
        if (!ch) continue;

        int pin = ch->getPin();
        if (pin < 0 || pin > 255) {
            FL_WARN("BitBangChannelDriver: pin " << pin << " out of range, skipping");
            continue;
        }

        if (mSlotForPin[pin] >= 0) {
            continue;  // already mapped
        }

        if (mNumActiveSlots >= 8) {
            FL_WARN("BitBangChannelDriver: more than 8 unique data pins, "
                     "pin " << pin << " will be skipped");
            continue;
        }

        int slot = mNumActiveSlots++;
        mSlotForPin[pin] = slot;
        mPinForSlot[slot] = pin;
        mActiveSlotMask |= (1 << slot);

        fl::pinMode(pin, PinMode::Output);
    }

    // Also set up clock pins for SPI channels
    for (fl::size i = 0; i < channels.size(); ++i) {
        const auto& ch = channels[i];
        if (!ch || !ch->isSpi()) continue;

        const auto* spiCfg = ch->getChipset().ptr<SpiChipsetConfig>();
        if (spiCfg && spiCfg->clockPin >= 0) {
            fl::pinMode(spiCfg->clockPin, PinMode::Output);
            fl::digitalWrite(spiCfg->clockPin, PinValue::Low);
        }
    }

    // Build DigitalMultiWrite8 LUT
    Pins8 pins8;
    for (int i = 0; i < 8; ++i) {
        pins8.pins[i] = mPinForSlot[i];
    }
    mMultiWriter.init(pins8);
}

void BitBangChannelDriver::transmitClocklessBit(u8 onesMask, u32 t1_ns,
                                                  u32 t2_ns, u32 t3_ns) {
    // Phase 1: ALL active lines HIGH
    mMultiWriter.writeByte(mActiveSlotMask);

    // Phase 2: After T1, '0'-bit lanes go LOW (only '1' lanes stay HIGH)
    fl::delayNanoseconds(t1_ns);
    mMultiWriter.writeByte(onesMask);

    // Phase 3: After T2, ALL lines go LOW
    fl::delayNanoseconds(t2_ns);
    mMultiWriter.writeByte(0x00);

    // Phase 4: Low tail
    fl::delayNanoseconds(t3_ns);
}

void BitBangChannelDriver::transmitClockless(
    fl::span<const ChannelDataPtr> channels) {
    if (channels.empty()) return;

    // Group channels by timing config.
    // For simplicity, process one timing group at a time by iterating and
    // collecting channels that share the same timing.
    fl::vector<bool> processed(channels.size(), false);

    for (fl::size i = 0; i < channels.size(); ++i) {
        if (processed[i] || !channels[i] || !channels[i]->isClockless()) continue;

        const ChipsetTimingConfig& timing = channels[i]->getTiming();
        u32 t1 = timing.t1_ns;
        u32 t2 = timing.t2_ns;
        u32 t3 = timing.t3_ns;

        // Collect all channels in this timing group
        fl::vector<fl::size> groupIndices;
        groupIndices.push_back(i);
        processed[i] = true;

        for (fl::size j = i + 1; j < channels.size(); ++j) {
            if (processed[j] || !channels[j] || !channels[j]->isClockless())
                continue;
            const ChipsetTimingConfig& otherTiming = channels[j]->getTiming();
            if (otherTiming == timing) {
                groupIndices.push_back(j);
                processed[j] = true;
            }
        }

        // Find max data size in group
        size_t maxBytes = 0;
        for (fl::size gi = 0; gi < groupIndices.size(); ++gi) {
            size_t sz = channels[groupIndices[gi]]->getSize();
            if (sz > maxBytes) maxBytes = sz;
        }

        // Disable interrupts for timing-critical section
#if defined(FL_IS_AVR)
        cli();
#endif

        // Transmit all bytes, all bits
        for (size_t byteIdx = 0; byteIdx < maxBytes; ++byteIdx) {
            // Gather current byte from each lane
            u8 laneBytes[8] = {0};
            for (fl::size gi = 0; gi < groupIndices.size(); ++gi) {
                const auto& ch = channels[groupIndices[gi]];
                int pin = ch->getPin();
                if (pin < 0 || pin > 255) continue;
                int slot = mSlotForPin[pin];
                if (slot < 0) continue;

                const auto& data = ch->getData();
                if (byteIdx < data.size()) {
                    laneBytes[slot] = data[byteIdx];
                }
            }

            // Send 8 bits MSB-first
            for (int bit = 7; bit >= 0; --bit) {
                // Build bitmask: bit i = 1 if lane i's current bit is 1
                u8 onesMask = 0;
                for (int slot = 0; slot < 8; ++slot) {
                    if ((laneBytes[slot] >> bit) & 1) {
                        onesMask |= (1 << slot);
                    }
                }
                transmitClocklessBit(onesMask, t1, t2, t3);
            }
        }

        // Re-enable interrupts
#if defined(FL_IS_AVR)
        sei();
#endif

        // Reset pulse: all LOW for reset_us microseconds
        mMultiWriter.writeByte(0x00);
        if (timing.reset_us > 0) {
            fl::delayMicroseconds(timing.reset_us);
        }
    }
}

void BitBangChannelDriver::transmitSpi(
    fl::span<const ChannelDataPtr> channels) {
    if (channels.empty()) return;

    // Group channels by clock pin
    fl::vector<bool> processed(channels.size(), false);

    for (fl::size i = 0; i < channels.size(); ++i) {
        if (processed[i] || !channels[i] || !channels[i]->isSpi()) continue;

        const auto* spiCfg =
            channels[i]->getChipset().ptr<SpiChipsetConfig>();
        if (!spiCfg) continue;

        int clockPin = spiCfg->clockPin;
        u32 clockHz = spiCfg->timing.clock_hz;
        u32 halfPeriodNs = (clockHz > 0) ? (500000000u / clockHz) : 500u;

        // Collect all channels sharing this clock pin
        fl::vector<fl::size> groupIndices;
        groupIndices.push_back(i);
        processed[i] = true;

        for (fl::size j = i + 1; j < channels.size(); ++j) {
            if (processed[j] || !channels[j] || !channels[j]->isSpi()) continue;
            const auto* otherSpi =
                channels[j]->getChipset().ptr<SpiChipsetConfig>();
            if (otherSpi && otherSpi->clockPin == clockPin) {
                groupIndices.push_back(j);
                processed[j] = true;
            }
        }

        // Find max data size in group
        size_t maxBytes = 0;
        for (fl::size gi = 0; gi < groupIndices.size(); ++gi) {
            size_t sz = channels[groupIndices[gi]]->getSize();
            if (sz > maxBytes) maxBytes = sz;
        }

        // Bit-bang SPI: MSB-first, data before rising edge (mode 0)
        for (size_t byteIdx = 0; byteIdx < maxBytes; ++byteIdx) {
            for (int bit = 7; bit >= 0; --bit) {
                // Build data mask for all lanes
                u8 dataMask = 0;
                for (fl::size gi = 0; gi < groupIndices.size(); ++gi) {
                    const auto& ch = channels[groupIndices[gi]];
                    const auto* chSpi =
                        ch->getChipset().ptr<SpiChipsetConfig>();
                    if (!chSpi) continue;

                    int pin = chSpi->dataPin;
                    if (pin < 0 || pin > 255) continue;
                    int slot = mSlotForPin[pin];
                    if (slot < 0) continue;

                    const auto& data = ch->getData();
                    u8 byte = (byteIdx < data.size()) ? data[byteIdx] : 0;
                    if ((byte >> bit) & 1) {
                        dataMask |= (1 << slot);
                    }
                }

                // Set data pins (SPI mode 0: data before rising edge)
                mMultiWriter.writeByte(dataMask);

                // Clock rising edge
                fl::digitalWrite(clockPin, PinValue::High);
                fl::delayNanoseconds(halfPeriodNs);

                // Clock falling edge
                fl::digitalWrite(clockPin, PinValue::Low);
                fl::delayNanoseconds(halfPeriodNs);
            }
        }
    }
}

void BitBangChannelDriver::show() {
    if (mEnqueuedChannels.empty()) return;

    // Move enqueued to transmitting
    mTransmittingChannels = fl::move(mEnqueuedChannels);
    mEnqueuedChannels.clear();

    // Mark all as in-use
    for (fl::size i = 0; i < mTransmittingChannels.size(); ++i) {
        if (mTransmittingChannels[i]) {
            mTransmittingChannels[i]->setInUse(true);
        }
    }

    // Rebuild pin config from current channels
    rebuildPinConfig(mTransmittingChannels);

    // Separate clockless vs SPI
    fl::vector<ChannelDataPtr> clockless;
    fl::vector<ChannelDataPtr> spi;
    for (fl::size i = 0; i < mTransmittingChannels.size(); ++i) {
        const auto& ch = mTransmittingChannels[i];
        if (!ch) continue;
        int pin = ch->getPin();
        if (pin < 0 || pin > 255 || mSlotForPin[pin] < 0) {
            continue;  // skipped (>8 pins)
        }
        if (ch->isClockless()) {
            clockless.push_back(ch);
        } else if (ch->isSpi()) {
            spi.push_back(ch);
        }
    }

    // Clockless first (timing-critical), then SPI
    if (!clockless.empty()) {
        transmitClockless(clockless);
    }
    if (!spi.empty()) {
        transmitSpi(spi);
    }

    // Done — clear in-use flags
    for (fl::size i = 0; i < mTransmittingChannels.size(); ++i) {
        if (mTransmittingChannels[i]) {
            mTransmittingChannels[i]->setInUse(false);
        }
    }
    mTransmittingChannels.clear();
}

}  // namespace fl
