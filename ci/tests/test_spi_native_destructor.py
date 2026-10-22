"""Exercise the real native SPI destructor with controlled SDK ownership errors."""

import shutil
from pathlib import Path

import pytest
from running_process import RunningProcess


SRC = Path(__file__).resolve().parents[2] / "src"


COMMON = r"""
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>
#define FL_IS_ESP32
#define FL_NO_EXCEPT noexcept
#define FL_LINK_WEAK __attribute__((weak))
#define SOC_SPI_PERIPH_NUM 3
#define SPI2_HOST 1
#define SPI3_HOST 2
#define SPI_DMA_CH_AUTO 3
#define SPICOMMON_BUSFLAG_MASTER 1
#define SPI_DEVICE_HALFDUPLEX 1
#define FASTLED_ESP32_HAS_SPI3 1
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 2
#define ESP_ERR_INVALID_ARG 3
#define ESP_ERR_TIMEOUT 4
#define portMAX_DELAY 0xffffffffu
#define pdMS_TO_TICKS(value) (value)
using esp_err_t = int;
using spi_host_device_t = int;
using spi_device_handle_t = void*;
using TickType_t = unsigned;
struct spi_transaction_t { unsigned length; const void* tx_buffer; };
struct spi_bus_config_t {
    int mosi_io_num, miso_io_num, sclk_io_num, quadwp_io_num, quadhd_io_num;
    unsigned max_transfer_sz, flags;
};
struct spi_device_interface_config_t {
    unsigned mode, clock_speed_hz; int spics_io_num; unsigned queue_size, flags;
};
extern spi_transaction_t* queued;
extern bool outstanding;
extern int resultError;
extern unsigned removed;
struct Payload {
    std::vector<unsigned char> bytes;
    explicit Payload(std::size_t count) : bytes(count, 0xa5) {}
    ~Payload() { if (outstanding) std::_Exit(92); }
};
namespace fl {
using u8 = std::uint8_t; using i8 = std::int8_t;
using u32 = std::uint32_t; using size = std::size_t;
using std::shared_ptr; using std::make_shared; using std::numeric_limits;
using std::memset;
template<class T> class span {
    T* pointer; std::size_t count;
public:
    span(T* p, std::size_t n) : pointer(p), count(n) {}
    T* data() const { return pointer; }
    std::size_t size() const { return count; }
};
enum class SPIError { NOT_INITIALIZED, BUSY, BUFFER_TOO_LARGE, ALLOCATION_FAILED };
enum class TransmitMode { ASYNC, SYNC };
class DMABuffer {
    std::shared_ptr<Payload> value;
public:
    DMABuffer() = default;
    explicit DMABuffer(std::size_t count) : value(std::make_shared<Payload>(count)) {}
    explicit DMABuffer(SPIError) {}
    void reset() { value.reset(); }
    bool ok() const { return bool(value); }
    std::size_t size() const { return value ? value->bytes.size() : 0; }
    span<u8> data() { return {value->bytes.data(), value->bytes.size()}; }
};
class SpiHw1 {
public:
    struct Config {
        u8 bus_num = 2; u32 clock_speed_hz = 1000000;
        i8 clock_pin = 18, data_pin = 4; u32 max_transfer_sz = 4096;
    };
    virtual ~SpiHw1() = default;
    virtual bool begin(const Config&) noexcept = 0;
    virtual void end() noexcept = 0;
    virtual DMABuffer acquireDMABuffer(std::size_t) noexcept = 0;
    virtual bool transmit(TransmitMode) noexcept = 0;
    virtual bool waitComplete(u32) noexcept = 0;
    virtual bool isBusy() const noexcept = 0;
    virtual bool isInitialized() const noexcept = 0;
    virtual int getBusId() const noexcept = 0;
    virtual const char* getName() const noexcept = 0;
};
}
inline int spi_bus_initialize(int, const spi_bus_config_t*, int) { return ESP_OK; }
inline int spi_bus_add_device(int, const spi_device_interface_config_t*, void** handle) {
    *handle = reinterpret_cast<void*>(1); return ESP_OK;
}
inline int spi_device_queue_trans(void*, spi_transaction_t* value, unsigned) {
    if (outstanding) std::_Exit(94);
    queued = value; outstanding = true; return ESP_OK;
}
inline int spi_device_get_trans_result(void*, spi_transaction_t** result, unsigned) {
    if (resultError) return resultError;
    *result = queued; outstanding = false; return ESP_OK;
}
inline int spi_bus_remove_device(void*) { ++removed; return ESP_OK; }
inline int spi_bus_free(int) { return ESP_OK; }
inline void fatalCheck(int error) {
    if (!error) return;
    if (!outstanding || !queued || queued->length != 32 || removed ||
        static_cast<const unsigned char*>(queued->tx_buffer)[0] != 0xa5) std::_Exit(93);
    std::_Exit(73);
}
#define __builtin_abort() fatalCheck(ESP_FAIL)
"""


@pytest.mark.parametrize(
    "mode", ["normal", "retry_end", "destructor_error", "repeat_transmit"]
)
@pytest.mark.parametrize("release", [False, True])
def test_native_spi_preserves_queued_ownership(
    tmp_path: Path, mode: str, release: bool
) -> None:
    compiler = shutil.which("c++") or shutil.which("clang++")
    if compiler is None:
        pytest.skip("C++ compiler required for native SPI lifecycle probe")
    (tmp_path / "common.h").write_text(COMMON)
    for name in (
        "platforms/esp/is_esp.h",
        "platforms/shared/spi_hw_1.h",
        "platforms/shared/spi_manager.h",
        "platforms/esp/32/drivers/spi/spi_hw_base.h",
        "fl/stl/cstring.h",
        "fl/stl/limits.h",
        "fl/stl/noexcept.h",
        "driver/spi_master.h",
        "esp_heap_caps.h",
        "esp_err.h",
    ):
        header = tmp_path / name
        header.parent.mkdir(parents=True, exist_ok=True)
        header.write_text('#include "common.h"\n')
    fixture = tmp_path / "probe.cpp"
    fixture.write_text(
        '#include "common.h"\n'
        "spi_transaction_t* queued = nullptr; bool outstanding = false;\n"
        "int resultError = 0; unsigned removed = 0;\n"
        '#include "platforms/esp/32/drivers/spi/spi_hw_1_esp32.cpp.hpp"\n'
        "int main() {\n"
        " { fl::SPISingleESP32 controller(2); fl::SpiHw1::Config config;\n"
        "   if (!controller.begin(config)) return 1;\n"
        "   auto buffer = controller.acquireDMABuffer(4);\n"
        "   if (!buffer.ok() || !controller.transmit()) return 2;\n"
        "   buffer.reset();\n"
        + (
            "   resultError = ESP_ERR_INVALID_ARG;\n"
            if mode in ("retry_end", "destructor_error")
            else ""
        )
        + (
            "   if (controller.transmit()) return 5;\n"
            if mode == "repeat_transmit"
            else ""
        )
        + (
            "   controller.end();\n"
            "   if (!outstanding || removed || !controller.isInitialized()) return 3;\n"
            "   resultError = ESP_OK;\n"
            if mode == "retry_end"
            else ""
        )
        + " }\n return !outstanding && removed == 1 ? 0 : 4;\n}\n"
    )
    executable = tmp_path / "probe"
    build = RunningProcess.run(
        [
            compiler,
            "-std=c++11",
            *(["-DNDEBUG"] if release else []),
            f"-I{tmp_path}",
            f"-I{SRC}",
            str(fixture),
            "-o",
            str(executable),
        ],
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        timeout=60,
    )
    assert build.returncode == 0, build.stdout
    result = RunningProcess.run(
        [str(executable)],
        capture_output=True,
        encoding="utf-8",
        errors="replace",
        timeout=30,
    )
    assert result.returncode == (73 if mode == "destructor_error" else 0), result.stdout
