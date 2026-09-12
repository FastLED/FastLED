"""OTA autoresearch helpers for FastLED hardware-in-the-loop testing.

Provides WiFi management and HTTP autoresearch flows for --ota mode.
Tests the fl::OTA web-based firmware update interface by verifying:
  - Authentication (Basic Auth: admin:<password>)
  - Unauthenticated access is rejected (401)
  - OTA upload endpoint exists and handles errors
"""

from __future__ import annotations

import asyncio
import base64
import hashlib
import time
from collections.abc import Callable
from pathlib import Path
from typing import TYPE_CHECKING, Any

import httpx
from colorama import Fore, Style

from ci.autoresearch.net import (
    _connect_peer_with_retry,
    _reclaim_stale_port_locks,
    create_wifi_manager,
    kPeerConnectAttempts,
)
from ci.rpc_client import RpcClient, RpcError, RpcTimeoutError
from ci.util.global_interrupt_handler import handle_keyboard_interrupt


if TYPE_CHECKING:
    from ci.util.serial_interface import SerialInterface


def _served_request_count(status: dict[str, Any]) -> int:
    """Read `servedRequests` from an untrusted device status payload.

    `int(status.get("servedRequests", 0))` raised ValueError/TypeError on a
    non-numeric or null value, and neither is caught by the handler around
    this flow — so a malformed device reply aborted the run with a traceback
    instead of the normal failure message (FastLED#3956).

    A value that is not a plain non-negative integer counts as zero served
    requests, which surfaces as the ordinary "did not serve" RuntimeError.
    """
    value = status.get("servedRequests", 0)
    # bool is an int subclass; True must not read as "1 request served".
    if isinstance(value, bool) or not isinstance(value, int):
        return 0
    return value if value >= 0 else 0


def ota_applied_ok(applied: dict[str, Any]) -> bool:
    """Whether `rpOtaUpdateStatus` says the RP actually applied the image.

    Both flags are required, and both must be exactly `True`:

    * ``attempted`` false means `pollOtaArtifactUpdate` never ran the fetch,
      so nothing was applied regardless of what the C6 served.
    * ``succeeded`` false means it ran and `HTTPUpdate` rejected the image --
      `ERROR[4]: Not Enough Space` is that case, and it is reached *after*
      the artifact has been served, so the C6 side looks healthy.

    Identity against `True` rather than truthiness on purpose: a device that
    answers `"succeeded": "false"` or `1` is malformed, and reading either as
    a pass is the failure mode this whole flow keeps producing. Anything that
    is not the boolean means "not proven", which fails.
    """
    return applied.get("attempted") is True and applied.get("succeeded") is True


async def _settle_link(
    client: "RpcClient",
    label: str,
    remaining_timeout: "Callable[[], float]",
) -> None:
    """Ping `client` until it answers, or raise naming the board that did not.

    Exists because the opening request of the peer-OTA run could time out
    against a device that was demonstrably healthy on both sides of the
    window. Without this the failure surfaced as a bare
    `No response with ID 1`, which names neither the board nor the call.

    `remaining_timeout` is the run's own budget helper. Settling must not
    invent a window of its own: three fixed 10 s pings plus their backoffs
    could outlast a shorter caller deadline, so each ping is clamped to
    whatever the run has left and raises once that is exhausted.
    """
    last: Exception | None = None
    for attempt in range(1, 4):
        try:
            await client.send("ping", {}, timeout=min(10.0, remaining_timeout()))
            if attempt > 1:
                print(f"  {label} link settled after {attempt} attempts")
            return
        except KeyboardInterrupt as ki:
            # Not strictly required -- KeyboardInterrupt derives from
            # BaseException, so the tuple below never catches it. Kept
            # explicit so a later widening of that tuple cannot silently
            # start swallowing Ctrl-C mid-retry. A bare `raise` would trip
            # KBI002: this repo requires the handler notify the main thread.
            handle_keyboard_interrupt(ki)
            raise
        except (RpcError, RpcTimeoutError) as exc:
            # `RuntimeError` used to be in this tuple, because a failed
            # *write* arrived as one: `PyserialMonitor.write` converts
            # `serial.SerialException` into `RuntimeError("Serial write
            # error: ...")` and `RpcClient.send` let it straight through.
            # Uncaught it defeated the point of this helper twice over -- no
            # retry, and a bare transport error naming neither the board nor
            # the call, which is exactly the failure #3956 was about.
            #
            # `send` normalizes it into `RpcError` now (#4191), which is the
            # right home for it, so the workaround is gone and this tuple is
            # back to the contract `send` documents.
            last = exc
            print(f"  {label} did not answer ping (attempt {attempt}/3): {exc}")
            # No backoff after the final attempt -- it would just delay the
            # error by 2 s.
            if attempt < 3:
                await asyncio.sleep(2.0)
    raise RpcTimeoutError(
        f"{label} did not answer ping after 3 attempts; last error: {last}"
    )


# Raw bytes per writeOtaArtifact call.
#
# Base64 expands this by 4/3, and the device's RPC receive path stops
# answering above ~512 characters of request payload: measured on an
# ESP32-C6, 428 characters answer in 0.33 s and 512 never answer at all.
# The request is delivered in full -- the daemon transmits all 735 bytes and
# receives nothing back -- so the drop is device-side, and there is no error
# response to key off. See FastLED#3956.
#
# That makes 512 raw bytes (684 characters) dead today. 256 raw bytes (344
# characters) does transfer correctly, verified end to end, but per-RPC
# latency is ~318 ms and size-independent, so a 1,034,444-byte image would
# need 4,041 round trips -- about 21 minutes against a 12 minute run budget.
# Halving the chunk trades a fast failure for a slow one.
#
# So this stays at 512 until the device-side limit is raised. Once it is,
# this is the lever that matters: 4 KB chunks move the same image in ~1.3
# minutes.
kOtaChunkBytes = 512

# The largest request payload measured to be *answered*, in base64
# characters. On an ESP32-C6, 428 characters answer in 0.33 s while 512 time
# out with nothing stored and the next RPC answering instantly.
#
# 428 and not 512: the true ceiling is somewhere between the two and was
# never bisected, so the only defensible constant is the largest size known
# to work. Taking 512 as the limit would call a 384-byte chunk deliverable,
# and 384 bytes is exactly 512 characters -- the size measured to time out.
#
# Written down rather than left in prose so the preflight below can check
# against it. FastLED#3956 is the device-side fix.
kOtaLargestAnsweredRequestCharacters = 428


def ota_chunk_is_deliverable(chunk_bytes: int, character_limit: int) -> bool:
    """Whether a full chunk survives base64 expansion into one request.

    Base64 is four characters per three bytes, rounded up to a whole group.
    """

    encoded_characters = ((chunk_bytes + 2) // 3) * 4
    return encoded_characters <= character_limit


def ota_largest_request_bytes(artifact_bytes: int, chunk_bytes: int) -> int:
    """The largest slice a transfer of this artifact will actually send.

    The chunk size only when the artifact is at least that big: a shorter
    artifact sends one shorter request, and judging it against the chunk
    would refuse a transfer that would have worked.
    """

    if artifact_bytes < chunk_bytes:
        return artifact_bytes
    return chunk_bytes


def ota_transfer_is_deliverable(
    artifact_bytes: int, chunk_bytes: int, character_limit: int
) -> bool:
    """Whether every request in this transfer fits the device's limit.

    The preflight's whole decision, in one place a test can call. Keeping it
    inline meant a case could only check the pieces and would pass with the
    preflight wired to the wrong one -- which is exactly what happened on the
    first attempt at covering this.
    """

    return ota_chunk_is_deliverable(
        ota_largest_request_bytes(artifact_bytes, chunk_bytes), character_limit
    )


async def run_ota_peer_autoresearch(
    upload_port: str,
    peer_upload_port: str,
    serial_iface: "SerialInterface | None",
    firmware_path: Path | None,
    timeout: float = 360.0,
) -> int:
    """Update RP2350W from an ESP32-C6 fixture without host WiFi access.

    Both boards are deployed by fbuild before this function runs. The host uses
    the normal fbuild-backed RPC transport only to stage the RP image onto the
    C6. The RP then fetches that verified artifact over their private WiFi link.
    """
    from ci.util.serial_interface import create_serial_interface

    print("\nOTA PEER AUTORESEARCH: RP2350W <- ESP32-C6")
    print("  Host WiFi is not used or changed by this mode.")
    if firmware_path is None or not firmware_path.is_file():
        print(
            f"  {Fore.RED}RP2350W firmware is missing: {firmware_path}{Style.RESET_ALL}"
        )
        return 1

    artifact = firmware_path.read_bytes()
    sha256 = hashlib.sha256(artifact).hexdigest()
    deadline = time.monotonic() + timeout
    primary: RpcClient | None = None
    peer: RpcClient | None = None

    def rpc_timeout() -> float:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise RpcTimeoutError("--net-peer --ota deadline expired")
        return min(20.0, remaining)

    async def rpc_data(
        client: RpcClient,
        method: str,
        params: str | list[Any] | dict[str, Any] | None,
    ) -> dict[str, Any]:
        # Name the board and the call. `RpcClient.send` reports only
        # `No response with ID 1 within 20.0s`, which identifies neither which
        # of the two links went silent nor what was asked of it. This flow
        # alternates between two boards, so that message is not a small
        # inconvenience: it sent me after the wrong board repeatedly, and the
        # existing `_settle_link` / `_connect_peer_with_retry` helpers both
        # label their failures precisely because of it. See FastLED#3899.
        which = "RP2350W" if client is primary else "ESP32-C6"
        port = upload_port if client is primary else peer_upload_port
        try:
            response = await client.send(
                method,
                {} if params is None else params,
                timeout=rpc_timeout(),
            )
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except (RpcError, RpcTimeoutError) as exc:
            raise RpcTimeoutError(
                f"{which} ({port}) did not answer {method!r}: {exc}"
            ) from exc
        if not isinstance(response.data, dict):
            raise RuntimeError(
                f"{which} ({port}) returned a non-object response to {method!r}"
            )
        return response.data

    try:
        primary = RpcClient(
            upload_port, timeout=rpc_timeout(), serial_interface=serial_iface
        )
        peer = RpcClient(
            peer_upload_port,
            timeout=rpc_timeout(),
            serial_interface=create_serial_interface(peer_upload_port),
        )
        # The companion's deploy has exited by now and can leave its port
        # locked in the daemon; reclaim before either client connects.
        # See FastLED/fbuild#1429.
        _reclaim_stale_port_locks([upload_port, peer_upload_port])
        try:
            await primary.connect(boot_wait=3.0, drain_boot=True)
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception as exc:  # noqa: BLE001
            raise RpcTimeoutError(
                f"RP2350W ({upload_port}) failed to attach: {exc}"
            ) from exc
        # The peer flashes last, so its USB-CDC is the one most likely to be
        # mid-re-enumeration here. Four of the captured `No response with ID 1`
        # failures came from this path. See FastLED#3899.
        await _connect_peer_with_retry(
            peer, "ESP32-C6", peer_upload_port, rpc_timeout, kPeerConnectAttempts
        )

        # Settle both links before the first real call. The peer flash runs
        # for ~60-90 s after the primary's, and the first request on the
        # primary was timing out even though the device answered `status`
        # immediately when queried standalone a moment later, and had just
        # served a schema fetch (FastLED#3956). One dropped opening request
        # should not sink the whole run, so ping with a bounded retry and
        # report which link is at fault when it genuinely is unreachable.
        await _settle_link(primary, "RP2350W", rpc_timeout)
        await _settle_link(peer, "ESP32-C6", rpc_timeout)

        primary_status = await rpc_data(primary, "status", {})
        peer_status = await rpc_data(peer, "status", {})
        if "rp2350" not in str(primary_status.get("platform", "")).lower():
            raise RuntimeError(f"Primary board is not RP2350W: {primary_status}")
        if "esp32-c6" not in str(peer_status.get("platform", "")).lower():
            raise RuntimeError(f"Companion board is not ESP32-C6: {peer_status}")

        begin = await rpc_data(
            peer, "beginOtaArtifact", {"size": len(artifact), "sha256": sha256}
        )
        if not begin.get("success"):
            raise RuntimeError(f"C6 refused OTA artifact: {begin}")
        # Fail here rather than at the first write, and say why.
        #
        # `kOtaChunkBytes` is 512, which base64-expands to 684 characters and
        # is over the measured device limit -- so the first full write is not
        # answered, and what that looks like from outside is a hang partway
        # into an OTA rather than a refusal. An earlier revision of this
        # comment described 256 as being what the loop sent, which the
        # constant has not said for some time; a reader had two numbers to
        # choose from and no way to tell which was live.
        #
        # 256 does transfer, verified end to end, but at ~318 ms per RPC and
        # size-independent it needs 4,041 round trips for a 1,034,444-byte
        # image -- about 21 minutes against a 12 minute run budget. So both
        # candidate values fail today, one quickly and one slowly, and the
        # real fix is device-side. FastLED#3956.
        # The largest slice this transfer will actually send, which is the
        # chunk size only when the artifact is at least that big. An artifact
        # shorter than one chunk sends one shorter request, and rejecting it
        # against `kOtaChunkBytes` would refuse a transfer that would have
        # worked -- a 321-byte artifact is one 428-character request, inside
        # the measured limit.
        largest_slice = ota_largest_request_bytes(len(artifact), kOtaChunkBytes)
        if not ota_transfer_is_deliverable(
            len(artifact), kOtaChunkBytes, kOtaLargestAnsweredRequestCharacters
        ):
            raise RuntimeError(
                f"OTA artifact transfer cannot run: the largest request this "
                f"{len(artifact)}-byte artifact sends is {largest_slice} bytes, "
                f"which base64-expands past the measured "
                f"{kOtaLargestAnsweredRequestCharacters}-character request limit, so "
                f"writeOtaArtifact will not be answered. This is FastLED#3956 "
                f"and is device-side; lowering the chunk to 256 transfers "
                f"correctly but needs ~21 minutes against a 12 minute budget."
            )
        for offset in range(0, len(artifact), kOtaChunkBytes):
            encoded = base64.b64encode(
                artifact[offset : offset + kOtaChunkBytes]
            ).decode("ascii")
            written = await rpc_data(peer, "writeOtaArtifact", encoded)
            if not written.get("success"):
                raise RuntimeError(
                    f"C6 artifact write failed at byte {offset}: {written}"
                )
        finished = await rpc_data(peer, "finishOtaArtifact", {})
        if not finished.get("success") or finished.get("sha256") != sha256:
            raise RuntimeError(f"C6 artifact verification failed: {finished}")

        c6_server = await rpc_data(peer, "startNetServer", {})
        if not c6_server.get("success"):
            raise RuntimeError(f"C6 AP start failed: {c6_server}")
        artifact_server = await rpc_data(peer, "startOtaArtifactServer", {})
        host = artifact_server.get("ip")
        port = artifact_server.get("port")
        # `isinstance(port, int)` alone admits 0, negatives and values past
        # 65535, which the device would then narrow and silently target a
        # different endpoint (FastLED#3956). bool is an int subclass, so
        # exclude it explicitly.
        if (
            not artifact_server.get("success")
            or not isinstance(host, str)
            or not isinstance(port, int)
            or isinstance(port, bool)
            or not 1 <= port <= 65535
        ):
            raise RuntimeError(f"C6 artifact server failed: {artifact_server}")

        joined = await rpc_data(
            primary,
            "wifiConnect",
            {"ssid": c6_server.get("ssid"), "password": c6_server.get("password")},
        )
        if not joined.get("success"):
            raise RuntimeError(f"RP2350W WiFi join failed: {joined}")
        for _ in range(20):
            wifi = await rpc_data(primary, "wifiStatus", {})
            if wifi.get("connected"):
                break
            await asyncio.sleep(min(0.5, rpc_timeout()))
        else:
            raise RpcTimeoutError("RP2350W did not join the C6 AP")

        accepted = await rpc_data(
            primary, "applyOtaArtifact", {"host": host, "port": port}
        )
        if not accepted.get("success"):
            raise RuntimeError(f"RP2350W rejected OTA artifact: {accepted}")
        await primary.close()
        primary = None
        await asyncio.sleep(min(8.0, rpc_timeout()))

        primary = RpcClient(
            upload_port,
            timeout=rpc_timeout(),
            serial_interface=create_serial_interface(upload_port),
        )
        await primary.connect(boot_wait=8.0, drain_boot=True)
        await rpc_data(primary, "ping", {})
        served = await rpc_data(peer, "otaArtifactStatus", {})
        if not served.get("success") or _served_request_count(served) < 1:
            # The RP fetches with the RPC link closed, so its own reason for
            # failing never reaches the host live. Ask for it now that the
            # link is back: without this the C6's `servedRequests: 0` is the
            # whole report, and a fetch that failed looks exactly like one
            # that never ran. See FastLED#3956.
            try:
                rp_update = await rpc_data(primary, "rpOtaUpdateStatus", {})
            except KeyboardInterrupt as ki:
                # Ahead of the tuple below on purpose. KeyboardInterrupt
                # derives from BaseException so the tuple cannot catch it
                # today, but this file already keeps the handler explicit
                # everywhere else so a later widening cannot silently start
                # swallowing Ctrl-C -- and this probe runs inside the failure
                # path, where an interrupt is most likely.
                handle_keyboard_interrupt(ki)
                raise
            except (RpcTimeoutError, RuntimeError, OSError) as probe_error:
                rp_update = {"probeFailed": str(probe_error)}
            raise RuntimeError(
                f"C6 did not serve the RP2350W artifact: {served}; "
                f"RP2350W update state: {rp_update}"
            )

        # The C6 having served the image proves a download happened, not that
        # the RP accepted it. `HTTPUpdate` can fetch the whole artifact and
        # still reject it -- `ERROR[4]: Not Enough Space` is exactly that
        # shape -- so passing on `servedRequests >= 1` alone would score the
        # run on the transfer and call an update proven that never applied.
        # #3832 asks this criterion to prove the expected build after reboot;
        # ask the board. See FastLED#3956.
        applied = await rpc_data(primary, "rpOtaUpdateStatus", {})
        if not ota_applied_ok(applied):
            raise RuntimeError(
                f"RP2350W did not apply the artifact it fetched: {applied}; "
                f"C6 side: {served}"
            )

        print(f"{Fore.GREEN}OTA PEER AUTORESEARCH PASSED{Style.RESET_ALL}")
        return 0
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        return 130
    except (RpcTimeoutError, RuntimeError, OSError) as error:
        print(f"{Fore.RED}OTA peer autoresearch failed: {error}{Style.RESET_ALL}")
        return 1
    finally:
        for client, stop_method in ((primary, "stopNet"), (peer, "stopNet")):
            if client is None:
                continue
            try:
                await client.send(stop_method, {}, timeout=2.0)
            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
            except Exception:
                pass
            await client.close()


async def run_ota_autoresearch(
    upload_port: str,
    serial_iface: "SerialInterface | None",
    timeout: float = 60.0,
    firmware_path: Path | None = None,
) -> int:
    """Run OTA autoresearch (--ota).

    1. Send startOta RPC to ESP32 (starts WiFi AP + OTA HTTP server)
    2. Connect host to ESP32's WiFi AP
    3. Run HTTP tests against OTA endpoints with auth checks
    4. (Optional) Flash firmware via OTA upload and verify reboot
    5. Send stopOta RPC to cleanup
    6. Restore host WiFi

    Args:
        upload_port: Serial port for RPC communication
        serial_iface: Pre-created serial interface
        timeout: RPC timeout in seconds
        firmware_path: Path to firmware.bin for OTA flash test (None = skip)

    Returns:
        Exit code (0 = success, 1 = failure)
    """
    wifi = create_wifi_manager()

    print()
    print("=" * 60)
    print("OTA AUTORESEARCH MODE")
    print("=" * 60)
    print()

    # Save current WiFi SSID for restore
    original_ssid = wifi.get_current_ssid()
    if original_ssid:
        print(f"  Current WiFi: '{original_ssid}' (will restore after test)")
    else:
        print("  No current WiFi connection detected")

    client: RpcClient | None = None

    try:
        # Connect to device via RPC
        print(f"\n  Connecting to device on {upload_port}...")
        client = RpcClient(upload_port, timeout=timeout, serial_interface=serial_iface)
        # This path has no budget helper of its own, so give it one rather
        # than letting the retry waits run unbounded.
        ota_deadline = time.monotonic() + timeout

        def ota_remaining() -> float:
            left = ota_deadline - time.monotonic()
            if left <= 0:
                raise RpcTimeoutError("--ota deadline expired")
            return min(20.0, left)

        await _connect_peer_with_retry(
            client, "device", upload_port, ota_remaining, kPeerConnectAttempts
        )
        print(f"  {Fore.GREEN}Connected to device{Style.RESET_ALL}")

        # Step 1: Start OTA on device
        print("\n--- Step 1: Start WiFi AP + OTA Server on ESP32 ---")
        response = await client.send("startOta", {}, timeout=30.0)
        ota_info = response.data

        if not isinstance(ota_info, dict) or not ota_info.get("success"):
            error = (
                ota_info.get("error", "Unknown error")
                if isinstance(ota_info, dict)
                else str(ota_info)
            )
            print(f"  {Fore.RED}Failed to start OTA: {error}{Style.RESET_ALL}")
            return 1

        ssid = ota_info.get("ssid", "")
        password = ota_info.get("password", "")
        ip = ota_info.get("ip", "192.168.4.1")
        port = ota_info.get("port", 80)
        ota_password = ota_info.get("ota_password", "")
        hostname = ota_info.get("hostname", "")
        print(
            f"  {Fore.GREEN}OTA started: SSID={ssid}, IP={ip}:{port}, hostname={hostname}{Style.RESET_ALL}"
        )

        # Step 2: Connect host to ESP32 WiFi AP
        print("\n--- Step 2: Connect Host to ESP32 WiFi AP ---")
        if not wifi.connect(ssid, password):
            print(f"  {Fore.RED}Failed to connect host to ESP32 AP{Style.RESET_ALL}")
            return 1

        # Step 3: Run HTTP tests
        print(f"\n--- Step 3: Validate OTA HTTP Endpoints on {ip}:{port} ---")
        base_url = f"http://{ip}:{port}"
        tests_passed = 0
        tests_failed = 0

        # Build Basic Auth header
        auth_value = base64.b64encode(f"admin:{ota_password}".encode()).decode()
        auth_headers = {"Authorization": f"Basic {auth_value}"}
        bad_auth_value = base64.b64encode(b"admin:wrongpassword").decode()
        bad_auth_headers = {"Authorization": f"Basic {bad_auth_value}"}

        # Test 1: GET / with valid auth -> 200 + HTML
        print("\n  Test 1: GET / (valid auth)")
        try:
            r = httpx.get(
                f"{base_url}/",
                headers=auth_headers,
                timeout=10.0,
                follow_redirects=True,
            )
            if r.status_code == 200:
                print(
                    f"    {Fore.GREEN}PASS{Style.RESET_ALL} - status={r.status_code}, content_length={len(r.content)}"
                )
                tests_passed += 1
            else:
                print(
                    f"    {Fore.RED}FAIL{Style.RESET_ALL} - status={r.status_code} (expected 200)"
                )
                tests_failed += 1
        except httpx.HTTPError as e:
            print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - {e}")
            tests_failed += 1

        # Test 2: GET / without auth -> 401
        print("\n  Test 2: GET / (no auth)")
        try:
            r = httpx.get(f"{base_url}/", timeout=10.0, follow_redirects=True)
            if r.status_code == 401:
                print(
                    f"    {Fore.GREEN}PASS{Style.RESET_ALL} - status={r.status_code} (correctly rejected)"
                )
                tests_passed += 1
            else:
                print(
                    f"    {Fore.RED}FAIL{Style.RESET_ALL} - status={r.status_code} (expected 401)"
                )
                tests_failed += 1
        except httpx.HTTPError as e:
            print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - {e}")
            tests_failed += 1

        # Test 3: GET / with wrong password -> 401
        print("\n  Test 3: GET / (wrong password)")
        try:
            r = httpx.get(
                f"{base_url}/",
                headers=bad_auth_headers,
                timeout=10.0,
                follow_redirects=True,
            )
            if r.status_code == 401:
                print(
                    f"    {Fore.GREEN}PASS{Style.RESET_ALL} - status={r.status_code} (correctly rejected)"
                )
                tests_passed += 1
            else:
                print(
                    f"    {Fore.RED}FAIL{Style.RESET_ALL} - status={r.status_code} (expected 401)"
                )
                tests_failed += 1
        except httpx.HTTPError as e:
            print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - {e}")
            tests_failed += 1

        # Test 4: POST /update with auth, no body -> error response (endpoint exists)
        print("\n  Test 4: POST /update (auth, no body)")
        try:
            r = httpx.post(f"{base_url}/update", headers=auth_headers, timeout=10.0)
            # We expect an error response (400 or 500) because no firmware data was sent,
            # but the key check is that the endpoint exists (not 404)
            if r.status_code != 404:
                print(
                    f"    {Fore.GREEN}PASS{Style.RESET_ALL} - status={r.status_code} (endpoint exists)"
                )
                tests_passed += 1
            else:
                print(
                    f"    {Fore.RED}FAIL{Style.RESET_ALL} - status=404 (endpoint not found)"
                )
                tests_failed += 1
        except httpx.HTTPError as e:
            print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - {e}")
            tests_failed += 1

        # Test 5: POST /update with auth, invalid firmware (4 zero bytes) -> error
        print("\n  Test 5: POST /update (auth, invalid firmware data)")
        try:
            r = httpx.post(
                f"{base_url}/update",
                headers=auth_headers,
                content=b"\x00\x00\x00\x00",
                timeout=10.0,
            )
            # We expect an error response (bad firmware header), but not 404
            if r.status_code != 404:
                print(
                    f"    {Fore.GREEN}PASS{Style.RESET_ALL} - status={r.status_code} (handled invalid firmware)"
                )
                tests_passed += 1
            else:
                print(
                    f"    {Fore.RED}FAIL{Style.RESET_ALL} - status=404 (endpoint not found)"
                )
                tests_failed += 1
        except httpx.HTTPError as e:
            print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - {e}")
            tests_failed += 1

        # Test 6: OTA Firmware Upload + Reboot Verification
        if firmware_path is None:
            print(
                "\n  Test 6: OTA firmware flash (SKIPPED - no firmware path provided)"
            )
        elif not firmware_path.exists():
            print(
                f"\n  Test 6: OTA firmware flash (SKIPPED - {firmware_path} not found)"
            )
        else:
            print("\n  Test 6: OTA firmware flash + reboot verification")
            try:
                firmware_data = firmware_path.read_bytes()
                firmware_size = len(firmware_data)
                print(
                    f"    Uploading firmware ({firmware_size} bytes) via POST /update..."
                )

                # Upload firmware with longer timeout (flash can take a while)
                r = httpx.post(
                    f"{base_url}/update",
                    headers=auth_headers,
                    content=firmware_data,
                    timeout=60.0,
                )

                if r.status_code != 200:
                    print(
                        f"    {Fore.RED}FAIL{Style.RESET_ALL} - upload returned status={r.status_code}, body={r.text!r}"
                    )
                    tests_failed += 1
                else:
                    print(
                        f"    Upload succeeded (status={r.status_code}, body={r.text!r})"
                    )

                    # Device will reboot after ~1s. Close the current RPC client
                    # since serial connection will drop.
                    await client.close()
                    client = None

                    # Restore WiFi back to original network - the ESP32 AP is gone
                    # after reboot, so we need our normal network for nothing but
                    # we need to drop the AP connection to avoid network issues.
                    wifi.restore(original_ssid)

                    # Wait for serial port to reappear after reboot. Use
                    # OS-level port enumeration (never a raw pyserial open
                    # of the device — see the PYS001 ban).
                    print("    Waiting for device to reboot...")
                    from ci.util.port_utils import port_exists

                    port_ready = False
                    max_wait = 20.0
                    start_time = time.time()

                    while time.time() - start_time < max_wait:
                        if port_exists(upload_port):
                            port_ready = True
                            elapsed = time.time() - start_time
                            print(f"    Serial port available after {elapsed:.1f}s")
                            break
                        await asyncio.sleep(0.5)

                    if not port_ready:
                        print(
                            f"    {Fore.RED}FAIL{Style.RESET_ALL} - serial port did not reappear after {max_wait}s"
                        )
                        tests_failed += 1
                    else:
                        # Reconnect via RPC and verify device is alive
                        print("    Reconnecting via RPC...")
                        client = RpcClient(
                            upload_port,
                            timeout=timeout,
                            serial_interface=serial_iface,
                        )
                        await client.connect(boot_wait=3.0, drain_boot=True)

                        status_resp = await client.send("status", {}, timeout=10.0)
                        status_data = status_resp.data

                        if isinstance(status_data, dict) and status_data.get("ready"):
                            print(
                                f"    {Fore.GREEN}PASS{Style.RESET_ALL} - device rebooted and responding (status={status_data})"
                            )
                            tests_passed += 1
                        else:
                            print(
                                f"    {Fore.RED}FAIL{Style.RESET_ALL} - unexpected status response: {status_data}"
                            )
                            tests_failed += 1

            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
                raise
            except httpx.HTTPError as e:
                print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - HTTP error: {e}")
                tests_failed += 1
            except RpcTimeoutError:
                print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - RPC timeout after reboot")
                tests_failed += 1
            except Exception as e:
                print(f"    {Fore.RED}FAIL{Style.RESET_ALL} - {e}")
                tests_failed += 1

        # Summary
        total = tests_passed + tests_failed
        print()
        print("=" * 60)
        if tests_failed == 0:
            print(
                f"{Fore.GREEN}OTA AUTORESEARCH PASSED ({tests_passed}/{total} tests){Style.RESET_ALL}"
            )
            return 0
        else:
            print(
                f"{Fore.RED}OTA AUTORESEARCH FAILED ({tests_passed}/{total} passed, {tests_failed} failed){Style.RESET_ALL}"
            )
            return 1

    except KeyboardInterrupt as ki:
        print("\n\n  Interrupted by user")
        handle_keyboard_interrupt(ki)
        return 130
    except RpcTimeoutError:
        print(f"\n  {Fore.RED}Timeout waiting for OTA response{Style.RESET_ALL}")
        return 1
    except Exception as e:
        print(f"\n  {Fore.RED}OTA autoresearch error: {e}{Style.RESET_ALL}")
        return 1
    finally:
        # Cleanup: stop OTA on device (only if client is still connected
        # and we haven't already rebooted via OTA flash)
        if client:
            try:
                await client.send("stopOta", {}, timeout=10.0)
            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
            except Exception:
                pass
            await client.close()
        # Restore original WiFi
        wifi.restore(original_ssid)
