// IWYU pragma: private

/// @file platforms/esp/8266/hspi_channel_driver.cpp.hpp
/// @brief Implementation of the ESP8266 hardware HSPI channel driver.

#include "platforms/esp/8266/hspi_channel_driver.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP8266)

#include "fl/channels/config.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/noexcept.h"
#include "platforms/esp/8266/spi_hspi_clock.h"

// IWYU pragma: begin_keep
#include <esp8266_peri.h>  // SDK SPI1/GPIO register map
// IWYU pragma: end_keep

namespace fl {

namespace {

/// SPI1W0..15 = 16 words = 512 bits = 64 bytes per hardware transaction.
constexpr fl::u16 kHspiMaxBatchBytes = 64;

/// Switch GPIO13/14 to the HSPI alternate function and program the SPI1
/// controller for byte-oriented MOSI transfers (mirrors the Arduino core's
/// `pinMode(pin, SPECIAL)` + `setDataBits()` sequence).
void hspiConfigure(fl::u32 clock_hz) FL_NO_EXCEPT {
    GPC(13) = (GPC(13) & (0xFUL << GPCI));
    GPC(14) = (GPC(14) & (0xFUL << GPCI));
    GPEC = (1UL << 13) | (1UL << 14);
    GPF13 = GPFFS(GPFFS_BUS(13));
    GPF14 = GPFFS(GPFFS_BUS(14));

    while (SPI1CMD & SPIBUSY) {}
    SPI1C = 0;   // MSB-first, no delay chains
    SPI1C1 = 0;
    GPMUX &= ~(1UL << 9);  // use SPI1CLK's divider fields, not the system clock
    SPI1CLK = esp8266::hspiClockFor(clock_hz).reg_value;
    SPI1U = SPIUMOSI | SPIUDUPLEX | SPIUSSE;
    // 8-bit phase; MISO is unused but kept in step with MOSI.
    SPI1U1 = (7UL << SPILMOSI) | (7UL << SPILMISO);
}

/// Program the transfer length. MOSI and MISO lengths stay equal.
void hspiSetTransferBits(fl::u32 nbits) FL_NO_EXCEPT {
    const fl::u32 bits = nbits - 1;
    const fl::u32 mask = (SPIMMOSI << SPILMOSI) | (SPIMMISO << SPILMISO);
    SPI1U1 = (SPI1U1 & ~mask) | (bits << SPILMOSI) | (bits << SPILMISO);
}

/// Stage up to 64 bytes into the FIFO and shift them out, blocking until done.
void hspiWriteChunk(const fl::u8* data, fl::u16 nbytes) FL_NO_EXCEPT {
    while (SPI1CMD & SPIBUSY) {}
    hspiSetTransferBits(static_cast<fl::u32>(nbytes) * 8);

    // SPI1W0..15 are contiguous; word byte 0 is emitted first, so pack the
    // staged bytes little-endian into each FIFO word.
    volatile fl::u32* fifo = &SPI1W0;
    const fl::u16 nwords = static_cast<fl::u16>((nbytes + 3) / 4);
    for (fl::u16 i = 0; i < nwords; ++i) {
        const fl::u16 b = static_cast<fl::u16>(i * 4);
        fl::u32 word = data[b];
        if (b + 1 < nbytes) word |= static_cast<fl::u32>(data[b + 1]) << 8;
        if (b + 2 < nbytes) word |= static_cast<fl::u32>(data[b + 2]) << 16;
        if (b + 3 < nbytes) word |= static_cast<fl::u32>(data[b + 3]) << 24;
        fifo[i] = word;
    }

    __sync_synchronize();
    SPI1CMD |= SPIBUSY;
    while (SPI1CMD & SPIBUSY) {}
}

}  // namespace

Esp8266HspiChannelDriver::Esp8266HspiChannelDriver() FL_NO_EXCEPT {}
Esp8266HspiChannelDriver::~Esp8266HspiChannelDriver() = default;

bool Esp8266HspiChannelDriver::canHandle(
    const ChannelDataPtr& data) const FL_NO_EXCEPT {
    if (!data || !data->isSpi()) {
        return false;
    }
    // The peripheral pins are fixed: only the HSPI MOSI/SCK pad pair (GPIO13 =
    // data, GPIO14 = clock, i.e. the default raw pin order) is served here.
    // Every other SPI pin pair falls through to the shared bit-bang bus.
    const auto* cfg = data->getChipset().ptr<SpiChipsetConfig>();
    return cfg != nullptr && cfg->dataPin == 13 && cfg->clockPin == 14;
}

void Esp8266HspiChannelDriver::enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT {
    if (channelData) {
        mEnqueuedChannels.push_back(fl::move(channelData));
    }
}

IChannelDriver::DriverState Esp8266HspiChannelDriver::poll() FL_NO_EXCEPT {
    return DriverState(DriverState::READY);
}

fl::string Esp8266HspiChannelDriver::getName() const FL_NO_EXCEPT {
    return fl::string::from_literal("SPI");
}

IChannelDriver::Capabilities Esp8266HspiChannelDriver::getCapabilities()
    const FL_NO_EXCEPT {
    return Capabilities(/*supportsClockless=*/false, /*supportsSpi=*/true);
}

void Esp8266HspiChannelDriver::transmit(
    const ChannelDataPtr& channel) FL_NO_EXCEPT {
    if (!channel) {
        return;
    }
    const auto* spiCfg = channel->getChipset().ptr<SpiChipsetConfig>();
    if (!spiCfg) {
        return;
    }

    hspiConfigure(spiCfg->timing.clock_hz);

    const fl::vector_psram<fl::u8>& bytes = channel->getData();
    const fl::size total = bytes.size();
    fl::size offset = 0;
    while (offset < total) {
        const fl::size remaining = total - offset;
        const fl::u16 chunk = static_cast<fl::u16>(
            remaining < kHspiMaxBatchBytes ? remaining : kHspiMaxBatchBytes);
        hspiWriteChunk(&bytes[offset], chunk);
        offset += chunk;
    }
}

void Esp8266HspiChannelDriver::show() FL_NO_EXCEPT {
    if (mEnqueuedChannels.empty()) {
        return;
    }

    mTransmittingChannels = fl::move(mEnqueuedChannels);
    mEnqueuedChannels.clear();

    for (fl::size i = 0; i < mTransmittingChannels.size(); ++i) {
        if (mTransmittingChannels[i]) {
            mTransmittingChannels[i]->setInUse(true);
        }
    }

    for (fl::size i = 0; i < mTransmittingChannels.size(); ++i) {
        transmit(mTransmittingChannels[i]);
    }

    for (fl::size i = 0; i < mTransmittingChannels.size(); ++i) {
        if (mTransmittingChannels[i]) {
            mTransmittingChannels[i]->setInUse(false);
        }
    }
    mTransmittingChannels.clear();
}

}  // namespace fl

#endif  // FL_IS_ESP8266
