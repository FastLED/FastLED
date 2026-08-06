#pragma once

// IWYU pragma: private

/// @file rp_pio_peripheral_mock.h
/// @brief Host-testable RP PIO TX/SPI peripheral implementations.
///
/// These mocks implement the same peripheral seams used by
/// `ChannelEngineRpPio`. They deliberately carry no global state: each test
/// owns its peripheral, can inject a lifecycle failure, and can inspect the
/// exact words submitted to DMA.

#include "fl/stl/vector.h"
#include "platforms/arm/rp/rpcommon/irp_pio_spi_peripheral.h"
#include "platforms/arm/rp/rpcommon/irp_pio_tx_peripheral.h"

namespace fl {

class RpPioTxPeripheralMock final : public IRpPioTxPeripheral {
  public:
    bool configure(const RpPioTxConfig& config) FL_NO_EXCEPT override {
        lastConfig = config;
        ++configureCalls;
        return configureOk;
    }

    bool startTxDma(const u32* words, size_t word_count) FL_NO_EXCEPT override {
        ++startCalls;
        firstWord = word_count == 0 ? 0 : words[0];
        wordCount = word_count;
        capturedWords.clear();
        for (size_t index = 0; index < word_count; ++index) {
            capturedWords.push_back(words[index]);
        }
        dmaBusy = startOk;
        return startOk;
    }

    bool isDmaBusy() const FL_NO_EXCEPT override { return dmaBusy; }
    bool isTerminalComplete() const FL_NO_EXCEPT override { return terminal; }
    bool hasError() const FL_NO_EXCEPT override { return error; }
    u32 nowMicros() const FL_NO_EXCEPT override { return timeUs; }

    void abort() FL_NO_EXCEPT override {
        ++abortCalls;
        dmaBusy = false;
    }

    void deinitialize() FL_NO_EXCEPT override { ++deinitializeCalls; }

    void reset() FL_NO_EXCEPT {
        lastConfig = RpPioTxConfig();
        configureOk = true;
        startOk = true;
        dmaBusy = false;
        terminal = false;
        error = false;
        configureCalls = 0;
        startCalls = 0;
        abortCalls = 0;
        deinitializeCalls = 0;
        wordCount = 0;
        firstWord = 0;
        timeUs = 0;
        capturedWords.clear();
    }

    RpPioTxConfig lastConfig;
    bool configureOk = true;
    bool startOk = true;
    bool dmaBusy = false;
    bool terminal = false;
    bool error = false;
    int configureCalls = 0;
    int startCalls = 0;
    int abortCalls = 0;
    int deinitializeCalls = 0;
    size_t wordCount = 0;
    u32 firstWord = 0;
    u32 timeUs = 0;
    fl::vector<u32> capturedWords;
};

class RpPioSpiPeripheralMock final : public IRpPioSpiPeripheral {
  public:
    bool configure(const RpPioSpiConfig& config) FL_NO_EXCEPT override {
        lastConfig = config;
        ++configureCalls;
        return configureOk;
    }

    bool startTxDma(const u32* words, size_t word_count) FL_NO_EXCEPT override {
        ++startCalls;
        capturedWords.clear();
        for (size_t index = 0; index < word_count; ++index) {
            capturedWords.push_back(words[index]);
        }
        dmaBusy = startOk;
        return startOk;
    }

    bool isDmaBusy() const FL_NO_EXCEPT override { return dmaBusy; }
    bool isTerminalComplete() const FL_NO_EXCEPT override { return terminal; }
    bool hasError() const FL_NO_EXCEPT override { return error; }
    u32 nowMicros() const FL_NO_EXCEPT override { return timeUs; }

    void abort() FL_NO_EXCEPT override {
        ++abortCalls;
        dmaBusy = false;
    }

    void deinitialize() FL_NO_EXCEPT override { ++deinitializeCalls; }

    void reset() FL_NO_EXCEPT {
        lastConfig = RpPioSpiConfig();
        configureOk = true;
        startOk = true;
        dmaBusy = false;
        terminal = false;
        error = false;
        configureCalls = 0;
        startCalls = 0;
        abortCalls = 0;
        deinitializeCalls = 0;
        timeUs = 0;
        capturedWords.clear();
    }

    RpPioSpiConfig lastConfig;
    bool configureOk = true;
    bool startOk = true;
    bool dmaBusy = false;
    bool terminal = false;
    bool error = false;
    int configureCalls = 0;
    int startCalls = 0;
    int abortCalls = 0;
    int deinitializeCalls = 0;
    u32 timeUs = 0;
    fl::vector<u32> capturedWords;
};

}  // namespace fl
