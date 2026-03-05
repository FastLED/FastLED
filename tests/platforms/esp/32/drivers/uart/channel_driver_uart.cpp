/// @file channel_driver_uart.cpp
/// @brief Consolidated UART channel driver tests
///
/// Tests cover:
/// - Wave8 encode/decode round-trip
/// - Mock peripheral lifecycle, write, waveform, timing, state
/// - Waveform alignment and LUT pattern verification
/// - Reset timing behavior
/// - ChannelEngineUART state machine, encoding, buffering

#include "test.h"

#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/delay.h"
#include "fl/rx_device.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/move.h"
#include "fl/stl/new.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"
#include "platforms/esp/32/drivers/uart/channel_driver_uart.h"
#include "platforms/esp/32/drivers/uart/iuart_peripheral.h"
#include "platforms/esp/32/drivers/uart/wave8_encoder_uart.h"
#include "platforms/esp/32/drivers/uart_esp32.h"
#include "platforms/shared/mock/esp/32/drivers/uart_peripheral_mock.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

/// UART bit duration at 4.0 Mbps = 250ns
constexpr uint32_t UART_BIT_NS = 250;

/// WS2812 timing constants for channel driver tests
constexpr uint32_t WS2812_T0H = 400;
constexpr uint32_t WS2812_T0L = 850;
constexpr uint32_t WS2812_T1H = 800;
constexpr uint32_t WS2812_T1L = 450;

/// Create a default UART test configuration at 4.0 Mbps
UartPeripheralConfig defaultConfig() {
    return UartPeripheralConfig(4000000, 17, -1, 4096, 0, 1, 1);
}

/// Convert UART waveform bits (from mock) into EdgeTime entries.
fl::vector<EdgeTime> waveformToEdges(const fl::vector<bool>& waveform, uint32_t bit_ns) {
    fl::vector<EdgeTime> edges;
    if (waveform.empty()) return edges;

    // Prepend idle HIGH period (UART idle = mark = HIGH)
    EdgeTime idle;
    idle.ns = bit_ns * 2;
    idle.high = 1;
    edges.push_back(idle);

    bool current_level = waveform[0];
    uint32_t run_length = 1;

    for (size_t i = 1; i < waveform.size(); i++) {
        if (waveform[i] == current_level) {
            run_length++;
        } else {
            EdgeTime edge;
            edge.ns = run_length * bit_ns;
            edge.high = current_level ? 1 : 0;
            edges.push_back(edge);
            current_level = waveform[i];
            run_length = 1;
        }
    }
    EdgeTime edge;
    edge.ns = run_length * bit_ns;
    edge.high = current_level ? 1 : 0;
    edges.push_back(edge);

    return edges;
}

/// Decode UART wave8 edges back to LED bytes using forward-only edge cursor.
size_t decodeUartWave8FromEdges(fl::span<const EdgeTime> edges, fl::span<uint8_t> out) {
    const uint32_t BIT_NS = UART_BIT_NS;
    const uint32_t HALF_BIT_NS = BIT_NS / 2;
    const uint32_t FRAME_NS = BIT_NS * 10;

    if (edges.empty()) return 0;

    size_t edge_idx = 0;
    uint32_t edge_start = 0;
    uint32_t edge_end = edges[0].ns;
    uint8_t edge_level = edges[0].high ? 1 : 0;
    bool exhausted = false;

    uint32_t total_ns = 0;
    for (size_t i = 0; i < edges.size(); i++) {
        total_ns += edges[i].ns;
    }

    auto advance_to = [&](uint32_t t_ns) -> uint8_t {
        while (t_ns >= edge_end && !exhausted) {
            edge_idx++;
            if (edge_idx >= edges.size()) {
                exhausted = true;
                return edge_level;
            }
            edge_start = edge_end;
            edge_end = edge_start + edges[edge_idx].ns;
            edge_level = edges[edge_idx].high ? 1 : 0;
        }
        return edge_level;
    };

    // Build the expected Wave10 LUT for the test timing
    // WS2812_T0H=400, WS2812_T0L=850, WS2812_T1H=800, WS2812_T1L=450
    // maps to ChipsetTimingConfig(t1=400, t2=850, t3=800)
    // which produces the same pulse counts as WS2812 (1 and 3)
    auto decode_lut = [](uint8_t b) -> int {
        switch (b) {
            case 0xEF: return 0;
            case 0x8F: return 1;
            case 0xEC: return 2;
            case 0x8C: return 3;
            default:   return -1;
        }
    };

    uint32_t scan_pos = 0;
    while (scan_pos < total_ns) {
        uint8_t level = advance_to(scan_pos);
        if (level == 1) break;
        scan_pos += HALF_BIT_NS;
    }

    size_t led_bytes = 0;
    int max_frames = static_cast<int>(out.size()) * 4 + 10;
    int frames_in_group = 0;
    uint8_t led_accum = 0;

    for (int frame = 0; frame < max_frames; frame++) {
        bool found = false;
        uint32_t limit = scan_pos + BIT_NS * 3;
        while (scan_pos < limit) {
            if (exhausted) break;
            uint8_t level = advance_to(scan_pos);
            if (level == 0) { found = true; break; }
            scan_pos += HALF_BIT_NS / 2;
        }
        if (!found) break;

        uint32_t frame_end = scan_pos + FRAME_NS;

        uint8_t byte_val = 0;
        for (int bit = 0; bit < 8; bit++) {
            uint32_t sample = scan_pos + BIT_NS + HALF_BIT_NS + bit * BIT_NS;
            if (exhausted) break;
            if (advance_to(sample)) byte_val |= (1 << bit);
        }

        int pair = decode_lut(byte_val);
        if (pair < 0) pair = 0;

        led_accum = static_cast<uint8_t>((led_accum << 2) | pair);
        frames_in_group++;

        if (frames_in_group == 4) {
            if (led_bytes < out.size()) {
                out[led_bytes++] = led_accum;
            }
            led_accum = 0;
            frames_in_group = 0;
        }

        scan_pos = frame_end;
    }

    return led_bytes;
}

/// Test fixture for ChannelEngineUART tests
class ChannelEngineUARTFixture {
public:
    ChannelEngineUARTFixture()
        : mMockPeripheralShared(fl::make_shared<UartPeripheralMock>()),
          mMockPeripheral(mMockPeripheralShared.get()),
          mDriver(mMockPeripheralShared) {}

    ChannelDataPtr createChannel(int pin, size_t num_leds) {
        ChipsetTimingConfig timing(WS2812_T0H, WS2812_T0L, WS2812_T1H, WS2812_T1L);
        fl::vector_psram<uint8_t> encodedData(num_leds * 3);
        for (size_t i = 0; i < encodedData.size(); i++) {
            encodedData[i] = static_cast<uint8_t>(i);
        }
        return fl::make_shared<ChannelData>(pin, timing, fl::move(encodedData));
    }

    bool pollUntilReady(uint32_t timeout_ms = 1000) {
        uint32_t elapsed = 0;
        while (elapsed < timeout_ms) {
            if (mDriver.poll() == IChannelDriver::DriverState::READY) return true;
            fl::delayMicroseconds(100);
            elapsed++;
        }
        return false;
    }

    fl::shared_ptr<UartPeripheralMock> mMockPeripheralShared;
    UartPeripheralMock* mMockPeripheral;
    ChannelEngineUART mDriver;
};

} // anonymous namespace

//=============================================================================
// Wave8 Encode/Decode Round-Trip
//=============================================================================

FL_TEST_CASE("UART wave8 round-trip: single byte 0xFF") {
    uint8_t input[] = {0xFF};
    uint8_t uart_buf[4];
    FL_REQUIRE(encodeLedsToUart(input, 1, uart_buf, sizeof(uart_buf)) == 4);

    UartPeripheralMock mock;
    FL_REQUIRE(mock.initialize(defaultConfig()));
    mock.writeBytes(uart_buf, 4);

    fl::vector<bool> waveform = mock.getWaveformWithFraming();
    FL_REQUIRE(waveform.size() == 40);

    fl::vector<EdgeTime> edges = waveformToEdges(waveform, UART_BIT_NS);

    uint8_t decoded[1] = {0};
    FL_CHECK_EQ(1u, decodeUartWave8FromEdges(edges, fl::span<uint8_t>(decoded, 1)));
    FL_CHECK_EQ(0xFF, decoded[0]);
}

FL_TEST_CASE("UART wave8 round-trip: single byte 0x00") {
    uint8_t input[] = {0x00};
    uint8_t uart_buf[4];
    FL_REQUIRE(encodeLedsToUart(input, 1, uart_buf, sizeof(uart_buf)) == 4);

    UartPeripheralMock mock;
    FL_REQUIRE(mock.initialize(defaultConfig()));
    mock.writeBytes(uart_buf, 4);

    fl::vector<EdgeTime> edges = waveformToEdges(mock.getWaveformWithFraming(), UART_BIT_NS);

    uint8_t decoded[1] = {0xFF};
    FL_CHECK_EQ(1u, decodeUartWave8FromEdges(edges, fl::span<uint8_t>(decoded, 1)));
    FL_CHECK_EQ(0x00, decoded[0]);
}

FL_TEST_CASE("UART wave8 round-trip: byte 0xE4") {
    uint8_t input[] = {0xE4};
    uint8_t uart_buf[4];
    FL_REQUIRE(encodeLedsToUart(input, 1, uart_buf, sizeof(uart_buf)) == 4);

    FL_CHECK_EQ(0x8C, uart_buf[0]);  // bits 7-6 = 11
    FL_CHECK_EQ(0xEC, uart_buf[1]);  // bits 5-4 = 10
    FL_CHECK_EQ(0x8F, uart_buf[2]);  // bits 3-2 = 01
    FL_CHECK_EQ(0xEF, uart_buf[3]);  // bits 1-0 = 00

    UartPeripheralMock mock;
    FL_REQUIRE(mock.initialize(defaultConfig()));
    mock.writeBytes(uart_buf, 4);

    fl::vector<EdgeTime> edges = waveformToEdges(mock.getWaveformWithFraming(), UART_BIT_NS);

    uint8_t decoded[1] = {0};
    FL_CHECK_EQ(1u, decodeUartWave8FromEdges(edges, fl::span<uint8_t>(decoded, 1)));
    FL_CHECK_EQ(0xE4, decoded[0]);
}

FL_TEST_CASE("UART wave8 round-trip: RGB pixel (3 bytes)") {
    uint8_t input[] = {0xAA, 0x55, 0xCC};
    uint8_t uart_buf[12];
    FL_REQUIRE(encodeLedsToUart(input, 3, uart_buf, sizeof(uart_buf)) == 12);

    UartPeripheralMock mock;
    FL_REQUIRE(mock.initialize(defaultConfig()));
    mock.writeBytes(uart_buf, 12);

    fl::vector<bool> waveform = mock.getWaveformWithFraming();
    FL_REQUIRE(waveform.size() == 120);

    fl::vector<EdgeTime> edges = waveformToEdges(waveform, UART_BIT_NS);

    uint8_t decoded[3] = {0};
    FL_CHECK_EQ(3u, decodeUartWave8FromEdges(edges, fl::span<uint8_t>(decoded, 3)));
    FL_CHECK_EQ(0xAA, decoded[0]);
    FL_CHECK_EQ(0x55, decoded[1]);
    FL_CHECK_EQ(0xCC, decoded[2]);
}

FL_TEST_CASE("UART wave8 round-trip: all 256 byte values") {
    UartPeripheralMock mock;
    FL_REQUIRE(mock.initialize(defaultConfig()));

    size_t failures = 0;
    for (int val = 0; val < 256; val++) {
        uint8_t input = static_cast<uint8_t>(val);
        uint8_t uart_buf[4];
        encodeLedsToUart(&input, 1, uart_buf, sizeof(uart_buf));

        mock.resetCapturedData();
        mock.writeBytes(uart_buf, 4);

        fl::vector<EdgeTime> edges = waveformToEdges(mock.getWaveformWithFraming(), UART_BIT_NS);

        uint8_t decoded = 0xFF;
        size_t count = decodeUartWave8FromEdges(edges, fl::span<uint8_t>(&decoded, 1));

        if (count != 1 || decoded != input) {
            failures++;
        }
    }
    FL_CHECK_EQ(0u, failures);
}

FL_TEST_CASE("UART wave8 round-trip: multi-LED strip (10 LEDs)") {
    uint8_t input[30];
    for (int i = 0; i < 30; i++) {
        input[i] = static_cast<uint8_t>(i * 7 + 13);
    }

    uint8_t uart_buf[120];
    FL_REQUIRE(encodeLedsToUart(input, 30, uart_buf, sizeof(uart_buf)) == 120);

    UartPeripheralMock mock;
    FL_REQUIRE(mock.initialize(defaultConfig()));
    mock.writeBytes(uart_buf, 120);

    fl::vector<bool> waveform = mock.getWaveformWithFraming();
    FL_REQUIRE(waveform.size() == 1200);

    fl::vector<EdgeTime> edges = waveformToEdges(waveform, UART_BIT_NS);

    uint8_t decoded[30] = {0};
    FL_CHECK_EQ(30u, decodeUartWave8FromEdges(edges, fl::span<uint8_t>(decoded, 30)));

    size_t mismatches = 0;
    for (int i = 0; i < 30; i++) {
        if (decoded[i] != input[i]) mismatches++;
    }
    FL_CHECK_EQ(0u, mismatches);
}

//=============================================================================
// Mock Peripheral Lifecycle
//=============================================================================

FL_TEST_CASE("UartPeripheralMock - Lifecycle") {
    UartPeripheralMock mock;

    FL_SUBCASE("Initial state") {
        FL_CHECK_FALSE(mock.isInitialized());
        FL_CHECK_FALSE(mock.isBusy());
        FL_CHECK(mock.getCapturedByteCount() == 0);
    }

    FL_SUBCASE("Initialize and deinitialize") {
        FL_CHECK(mock.initialize(defaultConfig()));
        FL_CHECK(mock.isInitialized());
        FL_CHECK(mock.getConfig().mBaudRate == 4000000);
        FL_CHECK(mock.getConfig().mTxPin == 17);
        mock.deinitialize();
        FL_CHECK_FALSE(mock.isInitialized());
    }

    FL_SUBCASE("Double initialization") {
        FL_CHECK(mock.initialize(defaultConfig()));
        FL_CHECK(mock.initialize(defaultConfig()));
        FL_CHECK(mock.isInitialized());
    }

    FL_SUBCASE("Invalid configuration - zero baud rate") {
        UartPeripheralConfig config = defaultConfig();
        config.mBaudRate = 0;
        FL_CHECK_FALSE(mock.initialize(config));
    }

    FL_SUBCASE("Invalid configuration - invalid TX pin") {
        UartPeripheralConfig config = defaultConfig();
        config.mTxPin = -1;
        FL_CHECK_FALSE(mock.initialize(config));
    }

    FL_SUBCASE("Invalid configuration - invalid stop bits") {
        UartPeripheralConfig config = defaultConfig();
        config.mStopBits = 0;
        FL_CHECK_FALSE(mock.initialize(config));
    }
}

//=============================================================================
// Mock Peripheral Data Transmission
//=============================================================================

FL_TEST_CASE("UartPeripheralMock - Transmission") {
    UartPeripheralMock mock;
    FL_CHECK(mock.initialize(defaultConfig()));

    FL_SUBCASE("Single byte") {
        uint8_t data = 0xA5;
        FL_CHECK(mock.writeBytes(&data, 1));
        FL_CHECK(mock.waitTxDone(1000));
        auto captured = mock.getCapturedBytes();
        FL_REQUIRE(captured.size() == 1);
        FL_CHECK(captured[0] == 0xA5);
    }

    FL_SUBCASE("Multiple single bytes") {
        uint8_t d1 = 0xAA, d2 = 0x55, d3 = 0xFF;
        FL_CHECK(mock.writeBytes(&d1, 1));
        FL_CHECK(mock.writeBytes(&d2, 1));
        FL_CHECK(mock.writeBytes(&d3, 1));
        FL_CHECK(mock.waitTxDone(1000));
        auto captured = mock.getCapturedBytes();
        FL_REQUIRE(captured.size() == 3);
        FL_CHECK(captured[0] == 0xAA);
        FL_CHECK(captured[1] == 0x55);
        FL_CHECK(captured[2] == 0xFF);
    }

    FL_SUBCASE("Byte array") {
        uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
        FL_CHECK(mock.writeBytes(data, 5));
        FL_CHECK(mock.waitTxDone(1000));
        auto captured = mock.getCapturedBytes();
        FL_REQUIRE(captured.size() == 5);
        for (size_t i = 0; i < 5; ++i) {
            FL_CHECK(captured[i] == data[i]);
        }
    }

    FL_SUBCASE("Large buffer streaming (50 RGB LEDs)") {
        fl::vector<uint8_t> data(150);
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] = static_cast<uint8_t>(i & 0xFF);
        }
        FL_CHECK(mock.writeBytes(data.data(), data.size()));
        FL_CHECK(mock.waitTxDone(5000));
        auto captured = mock.getCapturedBytes();
        FL_REQUIRE(captured.size() == data.size());
        for (size_t i = 0; i < data.size(); ++i) {
            FL_CHECK(captured[i] == data[i]);
        }
    }

    FL_SUBCASE("Write without initialization fails") {
        mock.deinitialize();
        uint8_t data = 0xA5;
        FL_CHECK_FALSE(mock.writeBytes(&data, 1));
    }

    FL_SUBCASE("Write with nullptr fails") {
        FL_CHECK_FALSE(mock.writeBytes(nullptr, 1));
    }

    FL_SUBCASE("Write with zero length fails") {
        uint8_t data = 0xA5;
        FL_CHECK_FALSE(mock.writeBytes(&data, 0));
    }
}

//=============================================================================
// Waveform Extraction and Alignment
//=============================================================================

FL_TEST_CASE("UartPeripheralMock - Waveform extraction (8N1)") {
    UartPeripheralMock mock;
    UartPeripheralConfig config = defaultConfig();
    config.mStopBits = 1;
    FL_CHECK(mock.initialize(config));

    FL_SUBCASE("Single byte waveform (0xA5)") {
        uint8_t data = 0xA5;  // 0b10100101
        FL_CHECK(mock.writeBytes(&data, 1));
        FL_CHECK(mock.waitTxDone(1000));
        auto waveform = mock.getWaveformWithFraming();
        FL_REQUIRE(waveform.size() == 10);

        // [START] [B0..B7] [STOP] = [0][1][0][1][0][0][1][0][1][1]
        FL_CHECK(waveform[0] == false);
        FL_CHECK(waveform[1] == true);
        FL_CHECK(waveform[2] == false);
        FL_CHECK(waveform[3] == true);
        FL_CHECK(waveform[4] == false);
        FL_CHECK(waveform[5] == false);
        FL_CHECK(waveform[6] == true);
        FL_CHECK(waveform[7] == false);
        FL_CHECK(waveform[8] == true);
        FL_CHECK(waveform[9] == true);
    }

    FL_SUBCASE("Multiple byte waveform") {
        uint8_t data[] = {0xFF, 0x00, 0xAA};
        FL_CHECK(mock.writeBytes(data, 3));
        FL_CHECK(mock.waitTxDone(1000));
        auto waveform = mock.getWaveformWithFraming();
        FL_REQUIRE(waveform.size() == 30);

        // First frame (0xFF): START=0, all data HIGH, STOP=1
        FL_CHECK(waveform[0] == false);
        for (int i = 1; i <= 8; ++i) FL_CHECK(waveform[i] == true);
        FL_CHECK(waveform[9] == true);

        // Second frame (0x00): START=0, all data LOW, STOP=1
        FL_CHECK(waveform[10] == false);
        for (int i = 11; i <= 18; ++i) FL_CHECK(waveform[i] == false);
        FL_CHECK(waveform[19] == true);
    }
}

FL_TEST_CASE("UartPeripheralMock - Waveform extraction (8N2)") {
    UartPeripheralMock mock;
    UartPeripheralConfig config = defaultConfig();
    config.mStopBits = 2;
    FL_CHECK(mock.initialize(config));

    FL_SUBCASE("Single byte with 2 stop bits") {
        uint8_t data = 0x55;
        FL_CHECK(mock.writeBytes(&data, 1));
        FL_CHECK(mock.waitTxDone(1000));
        auto waveform = mock.getWaveformWithFraming();
        FL_REQUIRE(waveform.size() == 11);
        FL_CHECK(waveform[0] == false);
        FL_CHECK(waveform[9] == true);
        FL_CHECK(waveform[10] == true);
    }

    FL_SUBCASE("Multiple bytes with 2 stop bits") {
        uint8_t data[] = {0xAA, 0x55};
        FL_CHECK(mock.writeBytes(data, 2));
        FL_CHECK(mock.waitTxDone(1000));
        auto waveform = mock.getWaveformWithFraming();
        FL_REQUIRE(waveform.size() == 22);
    }
}

FL_TEST_CASE("UART Waveform Alignment - All LUT patterns") {
    UartPeripheralMock mock;
    FL_REQUIRE(mock.initialize(defaultConfig()));

    FL_SUBCASE("Pattern 0b00 -> 0xEF") {
        uint8_t pattern = detail::encodeUart2Bits(0x00);
        FL_REQUIRE_EQ(pattern, 0xEF);
        mock.writeBytes(&pattern, 1);
        fl::vector<bool> waveform = mock.getWaveformWithFraming();
        // 0xEF = 0b11101111 LSB-first: 1-1-1-1-0-1-1-1
        FL_REQUIRE(waveform[0] == false);  // START
        FL_REQUIRE(waveform[1] == true);   // D0=1
        FL_REQUIRE(waveform[2] == true);   // D1=1
        FL_REQUIRE(waveform[3] == true);   // D2=1
        FL_REQUIRE(waveform[4] == true);   // D3=1
        FL_REQUIRE(waveform[5] == false);  // D4=0
        FL_REQUIRE(waveform[6] == true);   // D5=1
        FL_REQUIRE(waveform[7] == true);   // D6=1
        FL_REQUIRE(waveform[8] == true);   // D7=1
        FL_REQUIRE(waveform[9] == true);   // STOP
    }

    FL_SUBCASE("Pattern 0b01 -> 0x8F") {
        mock.resetCapturedData();
        uint8_t pattern = detail::encodeUart2Bits(0x01);
        FL_REQUIRE_EQ(pattern, 0x8F);
        mock.writeBytes(&pattern, 1);
        fl::vector<bool> waveform = mock.getWaveformWithFraming();
        // 0x8F = 0b10001111 LSB-first: 1-1-1-1-0-0-0-1
        FL_REQUIRE(waveform[0] == false);  // START
        FL_REQUIRE(waveform[1] == true);   // D0=1
        FL_REQUIRE(waveform[2] == true);   // D1=1
        FL_REQUIRE(waveform[3] == true);   // D2=1
        FL_REQUIRE(waveform[4] == true);   // D3=1
        FL_REQUIRE(waveform[5] == false);  // D4=0
        FL_REQUIRE(waveform[6] == false);  // D5=0
        FL_REQUIRE(waveform[7] == false);  // D6=0
        FL_REQUIRE(waveform[8] == true);   // D7=1
        FL_REQUIRE(waveform[9] == true);   // STOP
    }

    FL_SUBCASE("Pattern 0b10 -> 0xEC") {
        mock.resetCapturedData();
        uint8_t pattern = detail::encodeUart2Bits(0x02);
        FL_REQUIRE_EQ(pattern, 0xEC);
        mock.writeBytes(&pattern, 1);
        fl::vector<bool> waveform = mock.getWaveformWithFraming();
        // 0xEC = 0b11101100 LSB-first: 0-0-1-1-0-1-1-1
        FL_REQUIRE(waveform[0] == false);  // START
        FL_REQUIRE(waveform[1] == false);  // D0=0
        FL_REQUIRE(waveform[2] == false);  // D1=0
        FL_REQUIRE(waveform[3] == true);   // D2=1
        FL_REQUIRE(waveform[4] == true);   // D3=1
        FL_REQUIRE(waveform[5] == false);  // D4=0
        FL_REQUIRE(waveform[6] == true);   // D5=1
        FL_REQUIRE(waveform[7] == true);   // D6=1
        FL_REQUIRE(waveform[8] == true);   // D7=1
        FL_REQUIRE(waveform[9] == true);   // STOP
    }

    FL_SUBCASE("Pattern 0b11 -> 0x8C") {
        mock.resetCapturedData();
        uint8_t pattern = detail::encodeUart2Bits(0x03);
        FL_REQUIRE_EQ(pattern, 0x8C);
        mock.writeBytes(&pattern, 1);
        fl::vector<bool> waveform = mock.getWaveformWithFraming();
        // 0x8C = 0b10001100 LSB-first: 0-0-1-1-0-0-0-1
        FL_REQUIRE(waveform[0] == false);  // START
        FL_REQUIRE(waveform[1] == false);  // D0=0
        FL_REQUIRE(waveform[2] == false);  // D1=0
        FL_REQUIRE(waveform[3] == true);   // D2=1
        FL_REQUIRE(waveform[4] == true);   // D3=1
        FL_REQUIRE(waveform[5] == false);  // D4=0
        FL_REQUIRE(waveform[6] == false);  // D5=0
        FL_REQUIRE(waveform[7] == false);  // D6=0
        FL_REQUIRE(waveform[8] == true);   // D7=1
        FL_REQUIRE(waveform[9] == true);   // STOP
    }
}

FL_TEST_CASE("UartPeripheralMock - Start/stop bit validation") {
    UartPeripheralMock mock;
    UartPeripheralConfig config = defaultConfig();

    FL_SUBCASE("Valid 8N1 frames") {
        config.mStopBits = 1;
        FL_CHECK(mock.initialize(config));
        uint8_t data[] = {0x00, 0xFF, 0xAA, 0x55, 0x12, 0x34};
        FL_CHECK(mock.writeBytes(data, 6));
        FL_CHECK(mock.waitTxDone(1000));
        FL_CHECK(mock.verifyStartStopBits());
    }

    FL_SUBCASE("Valid 8N2 frames") {
        config.mStopBits = 2;
        FL_CHECK(mock.initialize(config));
        uint8_t data[] = {0x00, 0xFF, 0xAA, 0x55};
        FL_CHECK(mock.writeBytes(data, 4));
        FL_CHECK(mock.waitTxDone(1000));
        FL_CHECK(mock.verifyStartStopBits());
    }

    FL_SUBCASE("Verification fails with no data") {
        FL_CHECK(mock.initialize(config));
        FL_CHECK_FALSE(mock.verifyStartStopBits());
    }
}

FL_TEST_CASE("UART waveformToEdges produces correct edge structure") {
    UartPeripheralMock mock;
    FL_REQUIRE(mock.initialize(defaultConfig()));
    uint8_t data[] = {0xEF, 0xEF};  // LED "00","00" pattern
    mock.writeBytes(data, 2);

    fl::vector<bool> waveform = mock.getWaveformWithFraming();
    FL_REQUIRE(waveform.size() == 20);

    // 0xEF = 0b11101111, LSB-first: 1,1,1,1,0,1,1,1
    FL_CHECK_EQ(false, waveform[0]);  // START bit
    FL_CHECK_EQ(true,  waveform[1]);  // D0=1
    FL_CHECK_EQ(true,  waveform[2]);  // D1=1
    FL_CHECK_EQ(true,  waveform[9]); // STOP bit

    fl::vector<EdgeTime> edges = waveformToEdges(waveform, UART_BIT_NS);

    // Total time: 20 waveform bits + 2 idle bits = 22 * UART_BIT_NS
    uint32_t total_ns = 0;
    for (size_t i = 0; i < edges.size(); i++) {
        total_ns += edges[i].ns;
    }
    FL_CHECK_EQ(22u * UART_BIT_NS, total_ns);
}

FL_TEST_CASE("UartPeripheralMock - Edge cases (all zeros, all ones, alternating)") {
    UartPeripheralMock mock;
    FL_CHECK(mock.initialize(defaultConfig()));

    FL_SUBCASE("All zeros byte") {
        uint8_t data = 0x00;
        FL_CHECK(mock.writeBytes(&data, 1));
        FL_CHECK(mock.waitTxDone(1000));
        auto waveform = mock.getWaveformWithFraming();
        FL_CHECK(waveform[0] == false);
        for (int i = 1; i <= 8; ++i) FL_CHECK(waveform[i] == false);
        FL_CHECK(waveform[9] == true);
    }

    FL_SUBCASE("All ones byte") {
        uint8_t data = 0xFF;
        FL_CHECK(mock.writeBytes(&data, 1));
        FL_CHECK(mock.waitTxDone(1000));
        auto waveform = mock.getWaveformWithFraming();
        FL_CHECK(waveform[0] == false);
        for (int i = 1; i <= 8; ++i) FL_CHECK(waveform[i] == true);
        FL_CHECK(waveform[9] == true);
    }

    FL_SUBCASE("Alternating pattern (0xAA)") {
        uint8_t data = 0xAA;  // 0b10101010
        FL_CHECK(mock.writeBytes(&data, 1));
        FL_CHECK(mock.waitTxDone(1000));
        auto waveform = mock.getWaveformWithFraming();
        FL_CHECK(waveform[0] == false);
        FL_CHECK(waveform[1] == false);  // B0=0
        FL_CHECK(waveform[2] == true);   // B1=1
        FL_CHECK(waveform[3] == false);
        FL_CHECK(waveform[4] == true);
        FL_CHECK(waveform[5] == false);
        FL_CHECK(waveform[6] == true);
        FL_CHECK(waveform[7] == false);
        FL_CHECK(waveform[8] == true);
        FL_CHECK(waveform[9] == true);
    }
}

//=============================================================================
// Transmission Timing and Reset
//=============================================================================

FL_TEST_CASE("UartPeripheralMock - Virtual time and timing") {
    UartPeripheralMock mock;
    FL_CHECK(mock.initialize(defaultConfig()));
    mock.setVirtualTimeMode(true);

    FL_SUBCASE("Transmission lifecycle") {
        uint8_t data = 0xA5;
        FL_CHECK(mock.writeBytes(&data, 1));
        FL_CHECK(mock.isBusy());

        uint64_t tx_duration = mock.getTransmissionDuration();
        uint64_t reset_duration = mock.getResetDuration();
        FL_CHECK(tx_duration > 0);
        FL_CHECK(reset_duration >= 50);

        mock.pumpTime(tx_duration);
        FL_CHECK(mock.isBusy());  // Still in reset period

        mock.pumpTime(reset_duration);
        FL_CHECK_FALSE(mock.isBusy());

        auto captured = mock.getCapturedBytes();
        FL_REQUIRE(captured.size() == 1);
        FL_CHECK(captured[0] == 0xA5);
    }

    FL_SUBCASE("Partial time advancement") {
        uint8_t data = 0xA5;
        FL_CHECK(mock.writeBytes(&data, 1));
        uint64_t tx_duration = mock.getTransmissionDuration();

        mock.pumpTime(tx_duration / 2);
        FL_CHECK(mock.isBusy());

        mock.pumpTime(tx_duration - tx_duration / 2);
        FL_CHECK(mock.getRemainingTransmissionTime() == 0);
        FL_CHECK(mock.getRemainingResetTime() > 0);

        mock.pumpTime(mock.getRemainingResetTime());
        FL_CHECK_FALSE(mock.isBusy());
    }

    FL_SUBCASE("Multiple transmissions") {
        uint8_t d1 = 0xAA, d2 = 0x55;

        FL_CHECK(mock.writeBytes(&d1, 1));
        mock.pumpTime(mock.getTransmissionDuration() + mock.getResetDuration());
        FL_CHECK_FALSE(mock.isBusy());

        FL_CHECK(mock.writeBytes(&d2, 1));
        mock.pumpTime(mock.getTransmissionDuration() + mock.getResetDuration());
        FL_CHECK_FALSE(mock.isBusy());

        auto captured = mock.getCapturedBytes();
        FL_REQUIRE(captured.size() == 2);
        FL_CHECK(captured[0] == 0xAA);
        FL_CHECK(captured[1] == 0x55);
    }

    FL_SUBCASE("Force transmission complete") {
        mock.setTransmissionDelay(10000000);
        uint8_t data = 0xA5;
        FL_CHECK(mock.writeBytes(&data, 1));
        FL_CHECK(mock.isBusy());
        mock.forceTransmissionComplete();
        FL_CHECK_FALSE(mock.isBusy());
    }
}

FL_TEST_CASE("UartPeripheralMock - Reset timing") {
    UartPeripheralMock mock;
    FL_CHECK(mock.initialize(defaultConfig()));
    mock.setVirtualTimeMode(true);

    FL_SUBCASE("Reset period after transmission") {
        uint8_t data[] = {0xAA, 0x55, 0xFF};
        FL_CHECK(mock.writeBytes(data, sizeof(data)));
        mock.pumpTime(mock.getTransmissionDuration());
        FL_CHECK(mock.waitTxDone(1000));
        FL_CHECK(mock.isBusy());  // In reset period
    }

    FL_SUBCASE("Accepts new writes after reset expires") {
        uint8_t d1[] = {0xAA};
        FL_CHECK(mock.writeBytes(d1, 1));
        mock.pumpTime(mock.getTransmissionDuration());
        FL_CHECK(mock.waitTxDone(1000));
        FL_CHECK(mock.isBusy());

        mock.pumpTime(mock.getResetDuration());
        FL_CHECK_FALSE(mock.isBusy());

        uint8_t d2[] = {0x55};
        FL_CHECK(mock.writeBytes(d2, 1));
        FL_CHECK(mock.isBusy());
    }

    FL_SUBCASE("Multiple transmissions respect reset gaps") {
        for (int i = 0; i < 3; i++) {
            if (mock.isBusy()) {
                mock.pumpTime(mock.getRemainingResetTime());
            }
            FL_CHECK_FALSE(mock.isBusy());

            uint8_t data = static_cast<uint8_t>(0x11 * (i + 1));
            FL_CHECK(mock.writeBytes(&data, 1));
            mock.pumpTime(mock.getTransmissionDuration());
            FL_CHECK(mock.waitTxDone(1000));
            FL_CHECK(mock.isBusy());
        }

        auto captured = mock.getCapturedBytes();
        FL_REQUIRE(captured.size() == 3);
        FL_CHECK(captured[0] == 0x11);
        FL_CHECK(captured[1] == 0x22);
        FL_CHECK(captured[2] == 0x33);
    }

    FL_SUBCASE("Reset duration scales with transmission size") {
        // Small transmission (10 bytes)
        uint8_t small_data[10];
        for (int i = 0; i < 10; i++) small_data[i] = 0xAA;
        FL_CHECK(mock.writeBytes(small_data, sizeof(small_data)));
        mock.pumpTime(mock.getTransmissionDuration());
        FL_CHECK(mock.waitTxDone(1000));
        uint64_t small_reset = mock.getResetDuration();
        mock.pumpTime(small_reset);

        // Large transmission (1000 bytes)
        uint8_t large_data[1000];
        for (int i = 0; i < 1000; i++) large_data[i] = static_cast<uint8_t>(i);
        FL_CHECK(mock.writeBytes(large_data, sizeof(large_data)));
        mock.pumpTime(mock.getTransmissionDuration());
        FL_CHECK(mock.waitTxDone(1000));
        uint64_t large_reset = mock.getResetDuration();
        mock.pumpTime(large_reset);

        FL_CHECK(large_reset > small_reset);
        FL_CHECK(small_reset >= 50);
    }

    FL_SUBCASE("WS2812 reset requirement (>50us) is satisfied") {
        uint8_t data[] = {0xAA, 0x55};
        FL_CHECK(mock.writeBytes(data, sizeof(data)));
        mock.pumpTime(mock.getTransmissionDuration());
        FL_CHECK(mock.waitTxDone(1000));
        FL_CHECK(mock.getResetDuration() >= 50);
    }
}

//=============================================================================
// State Management
//=============================================================================

FL_TEST_CASE("UartPeripheralMock - State management") {
    UartPeripheralMock mock;
    FL_CHECK(mock.initialize(defaultConfig()));

    FL_SUBCASE("Reset captured data") {
        uint8_t data[] = {0x01, 0x02, 0x03};
        FL_CHECK(mock.writeBytes(data, 3));
        FL_CHECK(mock.waitTxDone(1000));
        FL_CHECK(mock.getCapturedByteCount() == 3);
        mock.resetCapturedData();
        FL_CHECK(mock.getCapturedByteCount() == 0);
    }

    FL_SUBCASE("Full reset") {
        uint8_t data[] = {0x01, 0x02, 0x03};
        FL_CHECK(mock.writeBytes(data, 3));
        FL_CHECK(mock.waitTxDone(1000));
        mock.reset();
        FL_CHECK_FALSE(mock.isInitialized());
        FL_CHECK_FALSE(mock.isBusy());
        FL_CHECK(mock.getCapturedByteCount() == 0);
    }

    FL_SUBCASE("Reset between tests") {
        uint8_t d1 = 0xAA;
        FL_CHECK(mock.writeBytes(&d1, 1));
        FL_CHECK(mock.waitTxDone(1000));

        mock.reset();
        FL_CHECK(mock.initialize(defaultConfig()));

        uint8_t d2 = 0x55;
        FL_CHECK(mock.writeBytes(&d2, 1));
        FL_CHECK(mock.waitTxDone(1000));
        auto captured = mock.getCapturedBytes();
        FL_REQUIRE(captured.size() == 1);
        FL_CHECK(captured[0] == 0x55);
    }
}

//=============================================================================
// ChannelEngineUART
//=============================================================================

FL_TEST_CASE("ChannelEngineUART - Lifecycle") {
    ChannelEngineUARTFixture fixture;

    FL_SUBCASE("Initial state is READY") {
        FL_CHECK(fixture.mDriver.poll() == IChannelDriver::DriverState::READY);
    }

    FL_SUBCASE("Engine name is UART") {
        FL_CHECK(fl::string(fixture.mDriver.getName()) == "UART");
    }

    FL_SUBCASE("Peripheral not initialized before first show") {
        FL_CHECK_FALSE(fixture.mMockPeripheral->isInitialized());
    }
}

FL_TEST_CASE("ChannelEngineUART - Single channel enqueue and show") {
    ChannelEngineUARTFixture fixture;

    FL_SUBCASE("Enqueue keeps state READY") {
        auto channel = fixture.createChannel(17, 10);
        fixture.mDriver.enqueue(channel);
        FL_CHECK(fixture.mDriver.poll() == IChannelDriver::DriverState::READY);
    }

    FL_SUBCASE("Show triggers initialization") {
        auto channel = fixture.createChannel(17, 10);
        fixture.mDriver.enqueue(channel);
        fixture.mDriver.show();
        FL_CHECK(fixture.mMockPeripheral->isInitialized());
    }

    FL_SUBCASE("Show transmits encoded data") {
        auto channel = fixture.createChannel(17, 10);
        fixture.mDriver.enqueue(channel);
        fixture.mDriver.show();
        fixture.mMockPeripheral->forceTransmissionComplete();
        FL_CHECK(fixture.pollUntilReady());
        auto captured = fixture.mMockPeripheral->getCapturedBytes();
        FL_CHECK(captured.size() == 120);  // 10 * 3 * 4
    }

    FL_SUBCASE("Encoding correctness - 0xE4, 0x00, 0xFF") {
        auto channel = fixture.createChannel(17, 1);
        auto& buffer = channel->getData();
        buffer[0] = 0xE4;
        buffer[1] = 0x00;
        buffer[2] = 0xFF;

        fixture.mDriver.enqueue(channel);
        fixture.mDriver.show();
        fixture.mMockPeripheral->forceTransmissionComplete();
        FL_CHECK(fixture.pollUntilReady());

        auto captured = fixture.mMockPeripheral->getCapturedBytes();
        FL_CHECK(captured.size() == 12);

        // 0xE4 = 0b11100100
        FL_CHECK(captured[0] == 0x8C);  // 11
        FL_CHECK(captured[1] == 0xEC);  // 10
        FL_CHECK(captured[2] == 0x8F);  // 01
        FL_CHECK(captured[3] == 0xEF);  // 00

        // 0x00 → all 0xEF
        for (int i = 4; i < 8; i++) FL_CHECK(captured[i] == 0xEF);

        // 0xFF → all 0x8C
        for (int i = 8; i < 12; i++) FL_CHECK(captured[i] == 0x8C);
    }
}

FL_TEST_CASE("ChannelEngineUART - State machine") {
    ChannelEngineUARTFixture fixture;

    FL_SUBCASE("READY -> DRAINING -> READY (virtual time)") {
        fixture.mMockPeripheral->setVirtualTimeMode(true);

        auto channel = fixture.createChannel(17, 10);
        fixture.mDriver.enqueue(channel);

        FL_CHECK(fixture.mDriver.poll() == IChannelDriver::DriverState::READY);
        fixture.mDriver.show();
        FL_CHECK(fixture.mDriver.poll() == IChannelDriver::DriverState::DRAINING);

        fixture.mMockPeripheral->pumpTime(
            fixture.mMockPeripheral->getTransmissionDuration() + 1000);
        FL_CHECK(fixture.mDriver.poll() == IChannelDriver::DriverState::READY);
    }

    FL_SUBCASE("Multiple show() calls") {
        auto ch1 = fixture.createChannel(17, 5);
        fixture.mDriver.enqueue(ch1);
        fixture.mDriver.show();
        fixture.mMockPeripheral->forceTransmissionComplete();
        FL_CHECK(fixture.pollUntilReady());
        FL_CHECK(fixture.mMockPeripheral->getCapturedBytes().size() == 60);

        fixture.mMockPeripheral->resetCapturedData();

        auto ch2 = fixture.createChannel(17, 10);
        fixture.mDriver.enqueue(ch2);
        fixture.mDriver.show();
        fixture.mMockPeripheral->forceTransmissionComplete();
        FL_CHECK(fixture.pollUntilReady());
        FL_CHECK(fixture.mMockPeripheral->getCapturedBytes().size() == 120);
    }
}

FL_TEST_CASE("ChannelEngineUART - Buffer sizing") {
    ChannelEngineUARTFixture fixture;

    FL_SUBCASE("10 LEDs -> 120 bytes") {
        auto ch = fixture.createChannel(17, 10);
        fixture.mDriver.enqueue(ch);
        fixture.mDriver.show();
        fixture.mMockPeripheral->forceTransmissionComplete();
        FL_CHECK(fixture.pollUntilReady());
        FL_CHECK(fixture.mMockPeripheral->getCapturedBytes().size() == 120);
    }

    FL_SUBCASE("50 LEDs -> 600 bytes") {
        auto ch = fixture.createChannel(17, 50);
        fixture.mDriver.enqueue(ch);
        fixture.mDriver.show();
        fixture.mMockPeripheral->forceTransmissionComplete();
        FL_CHECK(fixture.pollUntilReady());
        FL_CHECK(fixture.mMockPeripheral->getCapturedBytes().size() == 600);
    }

    FL_SUBCASE("500 LEDs -> 6000 bytes") {
        auto ch = fixture.createChannel(17, 500);
        fixture.mDriver.enqueue(ch);
        fixture.mDriver.show();
        fixture.mMockPeripheral->forceTransmissionComplete();
        FL_CHECK(fixture.pollUntilReady());
        FL_CHECK(fixture.mMockPeripheral->getCapturedBytes().size() == 6000);
    }
}

FL_TEST_CASE("ChannelEngineUART - Edge cases") {
    ChannelEngineUARTFixture fixture;

    FL_SUBCASE("Show with no enqueued channels") {
        fixture.mDriver.show();
        FL_CHECK(fixture.mDriver.poll() == IChannelDriver::DriverState::READY);
    }

    FL_SUBCASE("Empty channel (0 LEDs)") {
        ChipsetTimingConfig timing(WS2812_T0H, WS2812_T0L, WS2812_T1H, WS2812_T1L);
        fl::vector_psram<uint8_t> emptyData;
        auto data = fl::make_shared<ChannelData>(17, timing, fl::move(emptyData));
        fixture.mDriver.enqueue(data);
        fixture.mDriver.show();
        FL_CHECK(fixture.mDriver.poll() == IChannelDriver::DriverState::READY);
        FL_CHECK_FALSE(fixture.mMockPeripheral->isInitialized());
    }

    FL_SUBCASE("Null channel") {
        fixture.mDriver.enqueue(nullptr);
        fixture.mDriver.show();
        FL_CHECK(fixture.mDriver.poll() == IChannelDriver::DriverState::READY);
    }

    FL_SUBCASE("Multiple channels transmitted sequentially") {
        auto ch1 = fixture.createChannel(17, 10);
        auto ch2 = fixture.createChannel(18, 10);
        fixture.mDriver.enqueue(ch1);
        fixture.mDriver.enqueue(ch2);
        fixture.mDriver.show();
        FL_CHECK(fixture.mMockPeripheral->isInitialized());
        fixture.mMockPeripheral->forceTransmissionComplete();
        fixture.mDriver.poll();
        fixture.mMockPeripheral->forceTransmissionComplete();
        FL_CHECK(fixture.pollUntilReady());
    }

    FL_SUBCASE("Rapid show() calls") {
        for (int i = 0; i < 10; i++) {
            auto ch = fixture.createChannel(17, 10);
            fixture.mDriver.enqueue(ch);
            fixture.mDriver.show();
            fixture.mMockPeripheral->forceTransmissionComplete();
            FL_CHECK(fixture.pollUntilReady());
            fixture.mMockPeripheral->resetCapturedData();
        }
    }

    FL_SUBCASE("Very large LED count (2000 LEDs)") {
        auto ch = fixture.createChannel(17, 2000);
        fixture.mDriver.enqueue(ch);
        fixture.mDriver.show();
        fixture.mMockPeripheral->forceTransmissionComplete();
        FL_CHECK(fixture.pollUntilReady());
        FL_CHECK(fixture.mMockPeripheral->getCapturedBytes().size() == 24000);
    }
}

//=============================================================================
// ChannelEngineUART - canHandle with timing validation
//=============================================================================

FL_TEST_CASE("ChannelEngineUART - canHandle validates timing") {
    ChannelEngineUARTFixture fixture;

    FL_SUBCASE("Accepts WS2812 timing") {
        auto ch = fixture.createChannel(17, 10);
        FL_CHECK(fixture.mDriver.canHandle(ch));
    }

    FL_SUBCASE("Rejects null channel") {
        FL_CHECK_FALSE(fixture.mDriver.canHandle(nullptr));
    }

    FL_SUBCASE("Rejects infeasible timing (TM1829-1600kHz)") {
        // TM1829-1600k: T1=100, T2=300, T3=200, period=600ns → 8.3 Mbps
        ChipsetTimingConfig timing(100, 300, 200, 500);
        fl::vector_psram<uint8_t> data(30);
        auto ch = fl::make_shared<ChannelData>(17, timing, fl::move(data));
        FL_CHECK_FALSE(fixture.mDriver.canHandle(ch));
    }
}

} // FL_TEST_FILE
