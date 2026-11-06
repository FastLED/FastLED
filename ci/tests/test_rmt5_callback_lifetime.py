"""Regress #4993 using the real ESP RMT5 adapter against a minimal host SDK."""

import shutil
from pathlib import Path

import pytest
from running_process import RunningProcess


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "src/platforms/esp/32/drivers/rmt/rmt_5/rmt5_peripheral_esp.cpp.hpp"

# These declarations replace SDK dependencies, not the production implementation.
SDK = r"""
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <new>
#include <array>
#if TEST_IDF_MAJOR < 6
#define SOC_RMT_GROUPS 1
#define SOC_RMT_TX_CANDIDATES_PER_GROUP 4
#endif
#define FL_IS_ESP32 1
#define FASTLED_RMT5 1
#define FL_NO_EXCEPT noexcept
#define FL_IRAM
#define FL_HAS_WARN 0
#define FL_WARN(...) ((void)0)
#define FL_LOG_RMT(...) ((void)0)
#define FL_DBG(...) ((void)0)
#define FL_WARN_ONCE(...) ((void)0)
#define FL_EXTERN_C_BEGIN
#define FL_EXTERN_C_END
#define FL_MEMORY_BARRIER ((void)0)
#define FL_FALLTHROUGH [[fallthrough]]
#define ESP_IDF_VERSION_VAL(a,b,c) ((a)*10000+(b)*100+(c))
#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(TEST_IDF_MAJOR,1,0)
#define ESP_IDF_VERSION_6_OR_HIGHER (TEST_IDF_MAJOR >= 6)
using esp_err_t = int;
constexpr int ESP_OK=0, ESP_ERR_INVALID_ARG=1, ESP_ERR_TIMEOUT=2, ESP_ERR_INVALID_STATE=3;
constexpr int ESP_LOG_NONE=0, ESP_LOG_WARN=0, RMT_CLK_SRC_DEFAULT=0, MALLOC_CAP_DMA=0;
constexpr int ESP_CACHE_MSYNC_FLAG_DIR_C2M=1, ESP_CACHE_MSYNC_FLAG_UNALIGNED=2;
using TickType_t = unsigned;
constexpr TickType_t portMAX_DELAY=~0u;
#define pdMS_TO_TICKS(x) (x)
using gpio_num_t = int;
struct gpio_io_config_t { unsigned sig_out=0, oe=0, fun_sel=0; };
inline esp_err_t gpio_get_io_config(gpio_num_t, gpio_io_config_t*) { return ESP_ERR_INVALID_ARG; }
struct rmt_channel_t;
using rmt_channel_handle_t = rmt_channel_t*;
struct rmt_tx_done_event_data_t {};
using rmt_tx_done_callback_t = bool (*)(rmt_channel_handle_t,
    const rmt_tx_done_event_data_t*, void*);
struct rmt_tx_event_callbacks_t { rmt_tx_done_callback_t on_trans_done; };
struct rmt_channel_t { rmt_tx_event_callbacks_t callbacks{}; void* context=nullptr; };
struct rmt_tx_channel_config_t {
    int gpio_num, clk_src, intr_priority; unsigned resolution_hz;
    size_t mem_block_symbols, trans_queue_depth;
    struct { unsigned invert_out, with_dma, io_od_mode, io_loop_back; } flags;
};
struct rmt_transmit_config_t { int loop_count; struct { int eot_level; } flags; };
struct rmt_symbol_word_t { unsigned duration0, level0, duration1, level1; };
using rmt_encode_state_t = unsigned;
constexpr unsigned RMT_ENCODING_RESET=0, RMT_ENCODING_COMPLETE=1, RMT_ENCODING_MEM_FULL=2;
struct rmt_encoder_t {
    size_t (*encode)(rmt_encoder_t*, rmt_channel_handle_t, const void*, size_t, rmt_encode_state_t*);
    esp_err_t (*reset)(rmt_encoder_t*);
    esp_err_t (*del)(rmt_encoder_t*);
};
using rmt_encoder_handle_t = rmt_encoder_t*;
struct rmt_bytes_encoder_config_t {
    rmt_symbol_word_t bit0, bit1; struct { int msb_first; } flags;
};
struct rmt_copy_encoder_config_t {};
static bool fail_register=false, fail_delete=false;
static size_t live_allocations=0;
void* operator new(size_t size) {
    void* result=std::malloc(size);
    if (!result) std::abort();
    ++live_allocations;
    return result;
}
void operator delete(void* value) noexcept {
    if (value) { --live_allocations; std::free(value); }
}
void operator delete(void* value, size_t) noexcept { ::operator delete(value); }
inline esp_err_t rmt_new_tx_channel(const rmt_tx_channel_config_t*, rmt_channel_handle_t* out) {
    *out=new rmt_channel_t; return ESP_OK;
}
inline esp_err_t rmt_del_channel(rmt_channel_handle_t channel) {
    if (fail_delete) return ESP_ERR_INVALID_ARG;
    delete channel; return ESP_OK;
}
inline esp_err_t rmt_tx_register_event_callbacks(rmt_channel_handle_t channel,
    const rmt_tx_event_callbacks_t* callbacks, void* context) {
    if (fail_register) return ESP_ERR_INVALID_ARG;
    channel->callbacks=*callbacks; channel->context=context; return ESP_OK;
}
inline esp_err_t rmt_enable(rmt_channel_handle_t) { return ESP_OK; }
inline esp_err_t rmt_disable(rmt_channel_handle_t) { return ESP_OK; }
inline esp_err_t rmt_tx_switch_gpio(rmt_channel_handle_t, gpio_num_t, bool) { return ESP_OK; }
inline esp_err_t rmt_transmit(rmt_channel_handle_t, rmt_encoder_handle_t,
    const void*, size_t, const rmt_transmit_config_t*) { return ESP_OK; }
inline esp_err_t rmt_tx_wait_all_done(rmt_channel_handle_t, TickType_t) { return ESP_OK; }
inline esp_err_t rmt_new_bytes_encoder(const rmt_bytes_encoder_config_t*, rmt_encoder_handle_t*) {
    return ESP_ERR_INVALID_ARG;
}
inline esp_err_t rmt_new_copy_encoder(const rmt_copy_encoder_config_t*, rmt_encoder_handle_t*) {
    return ESP_ERR_INVALID_ARG;
}
inline esp_err_t rmt_del_encoder(rmt_encoder_handle_t) { return ESP_OK; }
inline esp_err_t esp_cache_msync(void*, size_t, int) { return ESP_OK; }
inline void esp_log_level_set(const char*, int) {}
inline void* heap_caps_aligned_alloc(size_t, size_t size, int) { return std::malloc(size); }
inline void heap_caps_free(void* ptr) { std::free(ptr); }
namespace fl {
using std::array;
using u8=uint8_t; using u32=uint32_t; using u64=uint64_t;
inline void delayMicroseconds(unsigned) {}
struct ChipsetTiming { u32 T1, T2, T3, RESET; };
template<class T> struct Singleton { static T& instance() { static T value; return value; } };
namespace detail {
using Rmt5TxDoneCallback=bool (*)(void*, const void*, void*);
struct Rmt5ChannelConfig {
    int gpio_num=1; u32 resolution_hz=10000000;
    size_t mem_block_symbols=64, trans_queue_depth=1;
    bool invert_out=false, with_dma=false; int intr_priority=0;
};
class Rmt5PeripheralESP {
public:
    static Rmt5PeripheralESP& instance() noexcept;
    virtual ~Rmt5PeripheralESP();
    static constexpr u32 kPinRoutingUnknown=~0u;
    virtual bool createTxChannel(const Rmt5ChannelConfig&, void**) noexcept=0;
    virtual bool deleteChannel(void*) noexcept=0;
    virtual bool enableChannel(void*) noexcept=0;
    virtual bool disableChannel(void*) noexcept=0;
    virtual bool transmit(void*, void*, const u8*, size_t) noexcept=0;
    virtual bool waitAllDone(void*, u32) noexcept=0;
    virtual void* createEncoder(const ChipsetTiming&, u32) noexcept=0;
    virtual void deleteEncoder(void*) noexcept=0;
    virtual bool resetEncoder(void*) noexcept=0;
    virtual bool registerTxCallback(void*, Rmt5TxDoneCallback, void*) noexcept=0;
    virtual void configureLogging() noexcept=0;
    virtual bool syncCache(void*, size_t) noexcept=0;
    virtual u8* allocateDmaBuffer(size_t) noexcept=0;
    virtual void freeDmaBuffer(u8*) noexcept=0;
    virtual u32 getPinRouting(int) noexcept=0;
    virtual bool rebindTxPin(void*, int) noexcept=0;
};
}
}
"""

HARNESS = r"""
#define REQUIRE(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "line %d: %s (live allocations: %zu)\n", \
        __LINE__, #condition, live_allocations); return 1; } } while (false)
struct CallbackState { int calls=0; void* channel=nullptr; const void* event=nullptr; };
bool onDone(void* channel, const void* event, void* context) {
    auto& state=*static_cast<CallbackState*>(context);
    ++state.calls; state.channel=channel; state.event=event; return true;
}
bool fire(void* handle, const rmt_tx_done_event_data_t& event) {
    auto* channel=static_cast<rmt_channel_handle_t>(handle);
    return channel->callbacks.on_trans_done(channel, &event, channel->context);
}
int main(int argc, char**) {
    fl::detail::Rmt5PeripheralESPImpl peripheral;
    fl::detail::Rmt5ChannelConfig config;
    CallbackState first, second;
    rmt_tx_done_event_data_t event;
    const size_t initial=live_allocations;
    void* handle=nullptr;
    REQUIRE(peripheral.createTxChannel(config, &handle));
    REQUIRE(peripheral.registerTxCallback(handle, onDone, &first));
    REQUIRE(fire(handle, event));
    REQUIRE(first.calls==1 && first.channel==handle && first.event==&event);
    if (argc==2) {
        // Failed SDK registration must keep the previously installed context alive.
        fail_register=true;
        REQUIRE(!peripheral.registerTxCallback(handle, onDone, &second));
        fail_register=false;
        REQUIRE(fire(handle, event));
        REQUIRE(first.calls==2 && second.calls==0);
    } else if (argc==3) {
        const size_t registered=live_allocations;
        for (int i=0; i<100; ++i) {
            REQUIRE(peripheral.registerTxCallback(handle, onDone, &second));
            REQUIRE(fire(handle, event));
        }
        REQUIRE(first.calls==1 && second.calls==100);
        REQUIRE(live_allocations==registered);
    } else if (argc==4) {
        const size_t registered=live_allocations;
        fail_delete=true;
        REQUIRE(!peripheral.deleteChannel(handle));
        REQUIRE(live_allocations==registered);
        REQUIRE(fire(handle, event));
        REQUIRE(first.calls==2);
        fail_delete=false;
    }
    if (argc==5) {
        void* others[3]{};
        CallbackState states[3];
        for (int i=0; i<3; ++i) {
            REQUIRE(peripheral.createTxChannel(config, &others[i]));
            REQUIRE(peripheral.registerTxCallback(others[i], onDone, &states[i]));
        }
        // Replacement must work even when every hardware callback slot is in use.
        REQUIRE(peripheral.registerTxCallback(handle, onDone, &second));
        REQUIRE(fire(handle, event));
        REQUIRE(second.calls==1);
        // Release channels out of order while the remaining callbacks stay live.
        REQUIRE(peripheral.deleteChannel(others[0]));
        REQUIRE(fire(others[1], event));
        REQUIRE(fire(others[2], event));
        REQUIRE(peripheral.deleteChannel(handle));
        REQUIRE(fire(others[1], event));
        REQUIRE(fire(others[2], event));
        REQUIRE(peripheral.deleteChannel(others[1]));
        REQUIRE(fire(others[2], event));
        REQUIRE(peripheral.deleteChannel(others[2]));
        REQUIRE(states[1].calls==2 && states[2].calls==3);
        REQUIRE(peripheral.createTxChannel(config, &handle));
        REQUIRE(peripheral.registerTxCallback(handle, onDone, &first));
    }
    REQUIRE(peripheral.deleteChannel(handle));
    REQUIRE(live_allocations==initial);
    for (int i=0; i<1000; ++i) {
        config.gpio_num=1+(i%2);
        REQUIRE(peripheral.createTxChannel(config, &handle));
        REQUIRE(peripheral.registerTxCallback(handle, onDone, &first));
        REQUIRE(fire(handle, event));
        REQUIRE(peripheral.deleteChannel(handle));
        REQUIRE(live_allocations==initial);
    }
    return 0;
}
"""


@pytest.fixture(scope="module", params=[5, 6], ids=["idf5", "idf6"])
def callback_binary(
    tmp_path_factory: pytest.TempPathFactory, request: pytest.FixtureRequest
) -> Path:
    compiler = shutil.which("clang++") or shutil.which("g++")
    if compiler is None:
        pytest.skip("host C++ compiler unavailable")
    directory = tmp_path_factory.mktemp("rmt5-callbacks")
    (directory / "sdk.h").write_text(SDK, encoding="utf-8")
    # Every production include is satisfied by the fake SDK except newly added
    # FastLED headers, which resolve through the real source include directory.
    headers = [
        "platforms/is_platform.h",
        "platforms/esp/32/feature_flags/enabled.h",
        "platforms/esp/esp_version.h",
        "platforms/esp/32/drivers/rmt/rmt_5/rmt5_peripheral_esp.h",
        "platforms/memory_barrier.h",
        "fl/chipsets/led_timing.h",
        "fl/system/delay.h",
        "fl/log/log.h",
        "fl/stl/bit_cast.h",
        "fl/stl/compiler_control.h",
        "fl/stl/noexcept.h",
        "fl/stl/array.h",
        "driver/gpio.h",
        "driver/rmt_tx.h",
        "esp_cache.h",
        "esp_heap_caps.h",
        "esp_log.h",
        "freertos/FreeRTOS.h",
        "esp_rom_gpio.h",
        "soc/gpio_sig_map.h",
        "soc/soc_caps.h",
        "platforms/esp/esp_cache_compat.h",
    ]
    for name in headers:
        path = directory / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "sdk.h"\n', encoding="utf-8")
    hal = directory / "hal/rmt_ll.h"
    hal.parent.mkdir(parents=True, exist_ok=True)
    hal.write_text(
        "#define RMT_LL_INST_NUM 1\n#define RMT_LL_TX_CANDIDATES_PER_INST 4\n",
        encoding="utf-8",
    )
    source = directory / "main.cpp"
    source.write_text(f'#include "{SOURCE}"\n' + HARNESS, encoding="utf-8")
    binary = directory / "callbacks.exe"
    result = RunningProcess.run(
        [
            compiler,
            "-std=c++17",
            "-O0",
            f"-DTEST_IDF_MAJOR={request.param}",
            "-I",
            str(directory),
            "-I",
            str(ROOT / "src"),
            str(source),
            "-o",
            str(binary),
        ],
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        check=False,
        timeout=60,
    )
    assert result.returncode == 0, result.stdout
    return binary


@pytest.mark.parametrize(
    "scenario",
    ["recreate", "register-failure", "replace", "delete-failure", "multiple-channels"],
)
def test_callback_context_lifetime(callback_binary: Path, scenario: str) -> None:
    count = [
        "recreate",
        "register-failure",
        "replace",
        "delete-failure",
        "multiple-channels",
    ].index(scenario)
    result = RunningProcess.run(
        [str(callback_binary), *(["scenario"] * count)],
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        check=False,
        timeout=30,
    )
    assert result.returncode == 0, f"{scenario}: {result.stdout}"
