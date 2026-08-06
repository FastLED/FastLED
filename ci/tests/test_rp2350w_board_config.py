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
RP_BLE_IMPL = REPO_ROOT / "src" / "platforms" / "arm" / "rp" / "ble_rp.cpp.hpp"
AUTORESEARCH_NET = REPO_ROOT / "examples" / "AutoResearch" / "AutoResearchNet.cpp"
AUTORESEARCH_SKETCH = REPO_ROOT / "examples" / "AutoResearch" / "AutoResearch.ino"


def test_rp2350w_selects_the_pico_2_w_board_profile() -> None:
    board = create_board("rp2350w")

    assert board.board_name == "rp2350w"
    assert board.real_board_name == "rpipico2w"
    assert board.framework == "arduino"
    assert board.board_build_core == "earlephilhower"
    assert board.platform_packages is not None
    assert "arduino-pico/releases/download/4.5.3/rp2040-4.5.3.zip" in (
        board.platform_packages
    )

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
    assert "att_server_notify" in source


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


def test_autoresearch_identity_prioritizes_pico_2_w() -> None:
    source = AUTORESEARCH_PLATFORM.read_text(encoding="utf-8")
    pico_2_w_branch = source.index("defined(ARDUINO_RASPBERRY_PI_PICO_2W)")
    generic_rp2350_branch = source.index("defined(FL_IS_RP2350)")

    assert pico_2_w_branch < generic_rp2350_branch
    assert 'return "Raspberry Pi Pico 2 W (RP2350)";' in source
    assert 'return "RP2350";' in source


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
