"""Board-definition contracts for Raspberry Pi Pico 2 and Pico 2 W."""

import re
from pathlib import Path

from ci.boards import create_board
from ci.compiler.sketch_filter import parse_filter_from_sketch, should_skip_sketch


REPO_ROOT = Path(__file__).resolve().parents[2]
AUTORESEARCH_INO = REPO_ROOT / "examples" / "AutoResearch" / "AutoResearch.ino"
AUTORESEARCH_PLATFORM = (
    REPO_ROOT / "examples" / "AutoResearch" / "AutoResearchPlatform.h"
)
MUTEX_DISPATCH = REPO_ROOT / "src" / "platforms" / "mutex.h"
MUTEX_IMPL = REPO_ROOT / "src" / "platforms" / "arm" / "rp" / "mutex_rp.cpp.hpp"
SEMAPHORE_DISPATCH = REPO_ROOT / "src" / "platforms" / "semaphore.h"
SEMAPHORE_IMPL = REPO_ROOT / "src" / "platforms" / "arm" / "rp" / "semaphore_rp.cpp.hpp"
WIFI_API = REPO_ROOT / "src" / "fl" / "net" / "wifi.h"
RP_WIFI_IMPL = REPO_ROOT / "src" / "platforms" / "arm" / "rp" / "wifi_rp.cpp.hpp"
ESP_WIFI_IMPL = (
    REPO_ROOT / "src" / "platforms" / "esp" / "32" / "net" / "wifi_esp32.cpp.hpp"
)
RP_BUILD = REPO_ROOT / "src" / "platforms" / "arm" / "rp" / "_build.cpp.hpp"
RP_WATCHDOG_IMPL = (
    REPO_ROOT / "src" / "platforms" / "arm" / "rp" / "watchdog_rp.impl.hpp"
)
RP_BLE_IMPL = REPO_ROOT / "src" / "platforms" / "arm" / "rp" / "ble_rp.cpp.hpp"
AUTORESEARCH_NET = REPO_ROOT / "examples" / "AutoResearch" / "AutoResearchNet.cpp"
AUTORESEARCH_SKETCH = REPO_ROOT / "examples" / "AutoResearch" / "AutoResearch.ino"
AUTORESEARCH_TEST = REPO_ROOT / "examples" / "AutoResearch" / "AutoResearchTest.cpp"
AUTORESEARCH_SYSTEM_METHODS = (
    REPO_ROOT / "examples" / "AutoResearch" / "AutoResearchRemoteSystemMethods.cpp"
)
AUTORESEARCH_PARALLEL = (
    REPO_ROOT / "examples" / "AutoResearch" / "AutoResearchRemoteRunParallelTest.cpp"
)
AUTORESEARCH_RP_CONCURRENCY = (
    REPO_ROOT / "examples" / "AutoResearch" / "AutoResearchRpConcurrency.cpp"
)


def test_rp2350w_selects_the_pico_2_w_board_profile() -> None:
    board = create_board("rp2350w")

    assert board.board_name == "rp2350w"
    assert board.real_board_name == "rpipico2w"
    assert board.framework == "arduino"
    assert board.board_build_core == "earlephilhower"
    assert board.platform_packages is not None
    assert "arduino-pico/releases/download/5.7.0/rp2040-5.7.0.zip" in (
        board.platform_packages
    )
    assert board.defines is None

    ini = board.to_platformio_ini()
    assert "[env:rp2350w]" in ini
    assert "board = rpipico2w" in ini
    assert "board_build.core = earlephilhower" in ini
    assert "board_build.ipbtstack = ipv4btcble" in ini
    assert "lib_deps = BTstackLib,HTTPUpdate" in ini


def test_rp2350w_ble_transport_uses_btstack_with_singleton_state() -> None:
    source = RP_BLE_IMPL.read_text(encoding="utf-8")

    assert "#include <BTstackLib.h>" in source
    assert "fl::Singleton<RpBleRuntime>" in source
    assert "att_server_request_to_send_notification" in source
    assert "att_server_get_mtu" in source
    # Outbound responses queue in a bounded FIFO and drain in ATT-MTU
    # chunks; a single shared buffer truncated an in-flight response when
    # a second one was produced mid-transfer (FastLED#3955).
    assert "BleNotifyQueue notifications" in source
    assert "notifications.chunkSize" in source
    assert "notifications.advance" in source
    assert "notify failed: %u" in source


def test_rp2350w_ble_disconnect_clears_pending_notification_state() -> None:
    """A dropped link must not leave the send-registration flags latched.

    Otherwise a request that registered just before the drop blocks every
    send on the next connection (FastLED#3955).
    """
    source = RP_BLE_IMPL.read_text(encoding="utf-8")

    disconnect = source[
        source.index("static void onDisconnected(") : source.index("TransportState* createTransport(")
    ]
    assert "notifications.clear()" in disconnect
    assert "notification_scheduled = false" in disconnect
    assert "notification_callback_pending = false" in disconnect

    destroy = source[
        source.index("void destroyTransport(") : source.index("StatusInfo queryStatus(")
    ]
    assert "notification_callback_pending = false" in destroy


def test_rp2350_and_rp2350w_keep_distinct_board_profiles() -> None:
    pico_2 = create_board("rp2350")
    pico_2_w = create_board("rp2350w")

    assert pico_2.real_board_name == "rpipico2"
    assert pico_2_w.real_board_name == "rpipico2w"
    assert pico_2.get_real_board_name() != pico_2_w.get_real_board_name()


def test_autoresearch_filter_admits_both_rp2350_profiles() -> None:
    sketch_filter = parse_filter_from_sketch(AUTORESEARCH_INO)

    assert sketch_filter is not None
    for environment in ("rp2350", "rp2350w"):
        skip, reason = should_skip_sketch(create_board(environment), sketch_filter)
        assert skip is False, reason


def test_rp_autoresearch_does_not_block_rpc_on_usb_dtr() -> None:
    source = AUTORESEARCH_INO.read_text(encoding="utf-8")
    wait_policy = source[
        source.index("#if defined(FL_IS_ESP_32S3)") : source.index(
            "const fl::RxBackend RX_BACKEND"
        )
    ]

    assert "defined(FL_IS_RP)" in wait_policy


def test_rp_bootloader_reboot_is_not_counted_as_a_watchdog_crash() -> None:
    source = RP_WATCHDOG_IMPL.read_text(encoding="utf-8")

    assert "#include <pico/version.h>" in source
    assert "PICO_SDK_VERSION_MINOR >= 3" in source
    assert "FL_WATCHDOG_HAS_RP_ENABLE_MARKER" in source
    assert "FL_WATCHDOG_HAS_RP_DISABLE_API" in source
    assert "if (watchdog_enable_caused_reboot()) return ResetCause::WATCHDOG;" in source
    assert "if (watchdog_caused_reboot()) return ResetCause::WATCHDOG;" in source
    assert "hw_clear_bits(&watchdog_hw->ctrl, WATCHDOG_CTRL_ENABLE_BITS);" in source
    assert "if (watchdog_hw->reason) return ResetCause::SOFTWARE;" in source

    begin_body = source[
        source.index("void Watchdog::begin") : source.index("void Watchdog::feed")
    ]
    assert begin_body.index("(void)lastResetCause();") < begin_body.index(
        "watchdog_enable(timeout_ms, true);"
    )


def test_rp_concurrency_probe_uses_a_separate_core1_stack() -> None:
    source = AUTORESEARCH_RP_CONCURRENCY.read_text(encoding="utf-8")

    assert "bool core1_separate_stack = true;" in source


def test_autoresearch_identity_prioritizes_pico_2_w() -> None:
    source = AUTORESEARCH_PLATFORM.read_text(encoding="utf-8")
    pico_2_w_branch = source.index("defined(ARDUINO_RASPBERRY_PI_PICO_2W)")
    generic_rp2350_branch = source.index("defined(FL_IS_RP2350)")

    assert pico_2_w_branch < generic_rp2350_branch
    assert 'return "Raspberry Pi Pico 2 W (RP2350)";' in source
    assert 'return "RP2350";' in source


def test_autoresearch_uses_uart_timing_for_both_rp_uart_engines() -> None:
    source = AUTORESEARCH_TEST.read_text(encoding="utf-8")

    assert 'fl::strcmp(driver_name, "UART0") == 0' in source
    assert 'fl::strcmp(driver_name, "UART1") == 0' in source


def test_autoresearch_frame_sizes_the_default_rp_pio_capture_buffer() -> None:
    source = AUTORESEARCH_TEST.read_text(encoding="utf-8")

    assert "rx_channel->backend() == fl::RxBackend::PLATFORM_DEFAULT" in source


def test_autoresearch_reports_typed_device_info_for_both_rp_uart_engines() -> None:
    source = AUTORESEARCH_SYSTEM_METHODS.read_text(encoding="utf-8")

    assert 'name == "UART0"' in source
    assert "deviceJson<fl::Bus::UART, 0>()" in source
    assert 'name == "UART1"' in source
    assert "deviceJson<fl::Bus::UART, 1>()" in source


def test_autoresearch_parallel_mode_preserves_rp_uart_instance_affinity() -> None:
    source = AUTORESEARCH_PARALLEL.read_text(encoding="utf-8")

    assert 'n == "UART0"' in source
    assert "opts.mBusWhich = 0" in source
    assert 'n == "UART1"' in source
    assert "opts.mBusWhich = 1" in source


def test_rp_platform_dispatches_real_mutex_and_semaphore_backends() -> None:
    for source_path in (
        MUTEX_DISPATCH,
        MUTEX_IMPL,
        SEMAPHORE_DISPATCH,
        SEMAPHORE_IMPL,
    ):
        source = source_path.read_text(encoding="utf-8")
        assert not re.search(r"\bFL_IS_RP2040\b", source), (
            f"{source_path} still guards on FL_IS_RP2040"
        )
        assert re.search(r"\bFL_IS_RP\b", source), (
            f"{source_path} does not guard on FL_IS_RP"
        )


def test_rp2350w_selects_the_cyw43_wifi_backend() -> None:
    """Pico 2 W must not silently fall through to the no-WiFi stubs."""
    api_source = WIFI_API.read_text(encoding="utf-8")
    implementation = RP_WIFI_IMPL.read_text(encoding="utf-8")
    build_source = RP_BUILD.read_text(encoding="utf-8")

    assert "FL_IS_RP2350" in api_source
    assert "PICO_CYW43_SUPPORTED" in api_source
    assert "FL_HAS_INCLUDE(<WiFi.h>)" in api_source
    assert "WiFi.beginNoBlock" in implementation
    assert "fl::Singleton<WifiState>::instance()" in implementation
    assert "#if defined(PICO_CYW43_SUPPORTED)" in AUTORESEARCH_NET.read_text(
        encoding="utf-8"
    )
    assert '"platforms/arm/rp/wifi_rp.cpp.hpp"' in build_source


def test_wifi_backends_are_platform_scoped_singletons() -> None:
    """The unity build must not compile ESP implementation code for RP targets."""
    implementation = ESP_WIFI_IMPL.read_text(encoding="utf-8")

    assert "#if FL_WIFI_AVAILABLE && defined(FL_IS_ESP32)" in implementation
    assert "fl::Singleton<WifiState>::instance()" in implementation
    assert "WifiState g_state" not in implementation


def test_rp2350w_autoresearch_exposes_the_cyw43_http_peer_surface() -> None:
    """The C6 peer can drive RP2350W HTTP through the existing RPC plane."""
    net_source = AUTORESEARCH_NET.read_text(encoding="utf-8")
    sketch_source = AUTORESEARCH_SKETCH.read_text(encoding="utf-8")

    assert "defined(FL_IS_RP2350)" in net_source
    assert "WiFiClient" in net_source
    assert "WiFiServer" in net_source
    assert "fl::Singleton<RpPeerState>::instance()" in net_source
    assert "void pollNetServer()" in net_source
    assert "pollNetServer();" in sketch_source
    assert "const uint32_t deadline_ms = millis() + 2000;" in net_source
    assert "state.server->begin();" in net_source
    assert "if (*state.server)" in net_source
    assert "FastLED.watchdog().feed();" in net_source
    assert "delay(10);" in net_source
    assert "} while (static_cast<int32_t>(millis() - deadline_ms) < 0);" in net_source
    assert 'response.set("error", "TCP server failed to listen")' in net_source
