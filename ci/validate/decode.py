"""Decode validation for FastLED codec testing.

Validates media file decoding by reading the file, base64-encoding it,
constructing a JSON-RPC payload, and writing it to .build/decode_payload.json.
The C++ decode_file test reads from that hardcoded path by default.

When a serial port is provided (device mode), the payload is sent directly
to the attached device via JSON-RPC and the response (including first 16 pixels)
is displayed.

Supports local files and URLs (downloaded to a temp file automatically).
"""

from __future__ import annotations

import base64
import json
import os
import shutil
import subprocess
import tempfile
import urllib.request
from pathlib import Path
from typing import TYPE_CHECKING, Any

from colorama import Fore, Style


if TYPE_CHECKING:
    from ci.util.serial_interface import SerialInterface


# Well-known payload path that C++ test reads by default
PAYLOAD_PATH = ".build/decode_payload.json"


# Supported extensions and their codec types (for display only)
SUPPORTED_EXTENSIONS: dict[str, str] = {
    ".mp4": "H.264 (MP4)",
    ".mpeg": "MPEG-1",
    ".mpg": "MPEG-1",
    ".gif": "GIF",
    ".jpg": "JPEG",
    ".jpeg": "JPEG",
    ".mp3": "MP3",
    ".webp": "WebP",
}


def _is_url(path: str) -> bool:
    return path.startswith("http://") or path.startswith("https://")


def _get_extension(path: str) -> str:
    """Get lowercase file extension including the dot."""
    _, ext = os.path.splitext(path)
    return ext.lower()


def _download_to_temp(url: str) -> tuple[str, str]:
    """Download a URL to a temporary file, preserving the extension.

    Returns (temp_path, extension).
    """
    ext = _get_extension(url)
    # Strip any query params from extension
    if "?" in ext:
        ext = ext[: ext.index("?")]

    fd, temp_path = tempfile.mkstemp(suffix=ext, prefix="fastled_decode_")
    os.close(fd)

    print(f"  Downloading {url} ...")
    try:
        urllib.request.urlretrieve(url, temp_path)  # noqa: S310
    except KeyboardInterrupt:
        os.unlink(temp_path)
        from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly

        handle_keyboard_interrupt_properly()
    except Exception as e:
        os.unlink(temp_path)
        raise RuntimeError(f"Failed to download {url}: {e}") from e

    size = os.path.getsize(temp_path)
    print(f"  Downloaded {size:,} bytes to {temp_path}")
    return temp_path, ext


def _build_jsonrpc_payload(file_bytes: bytes, extension: str) -> str:
    """Build a JSON-RPC request string with base64-encoded file data.

    Args:
        file_bytes: Raw file contents.
        extension: File extension including the dot (e.g. ".mp4").

    Returns:
        JSON string of the JSON-RPC request.
    """
    b64_data = base64.b64encode(file_bytes).decode("ascii")
    payload = {
        "method": "decodeFile",
        "params": [b64_data, extension],
        "id": 1,
    }
    return json.dumps(payload)


def _resolve_file(decode_path: str) -> tuple[str, str, str | None]:
    """Resolve decode_path to (file_path, extension, temp_download_path_or_None)."""
    temp_download: str | None = None
    if _is_url(decode_path):
        file_path, ext = _download_to_temp(decode_path)
        temp_download = file_path
    else:
        file_path = str(Path(decode_path).resolve())
        ext = _get_extension(file_path)
    return file_path, ext, temp_download


def _print_pixel_table(pixels: list[list[int]]) -> None:
    """Print a table of pixel RGB values."""
    print(f"\n  {'Index':<8}{'R':<6}{'G':<6}{'B':<6}")
    print(f"  {'─' * 26}")
    for i, px in enumerate(pixels):
        r, g, b = px[0], px[1], px[2]
        print(f"  {i:<8}{r:<6}{g:<6}{b:<6}")


async def run_device_decode_validation(
    decode_path: str,
    port: str,
    timeout: float = 120.0,
    serial_interface: "SerialInterface | None" = None,
) -> int:
    """Send media file to device via JSON-RPC and decode on-device.

    Args:
        decode_path: Local file path or URL to a media file.
        port: Serial port for device communication.
        timeout: RPC timeout in seconds.
        serial_interface: Optional serial interface override.

    Returns:
        Exit code (0 = success, 1 = failure).
    """
    from ci.rpc_client import RpcClient, RpcTimeoutError
    from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly

    print()
    print("=" * 60)
    print("DEVICE DECODE VALIDATION MODE")
    print("=" * 60)
    print()

    temp_download: str | None = None
    client: RpcClient | None = None

    try:
        file_path, ext, temp_download = _resolve_file(decode_path)

        if ext not in SUPPORTED_EXTENSIONS:
            supported = ", ".join(sorted(SUPPORTED_EXTENSIONS.keys()))
            print(
                f"{Fore.RED}Error: Unsupported file extension '{ext}'{Style.RESET_ALL}"
            )
            print(f"  Supported: {supported}")
            return 1

        if not os.path.isfile(file_path):
            print(f"{Fore.RED}Error: File not found: {file_path}{Style.RESET_ALL}")
            return 1

        codec_name = SUPPORTED_EXTENSIONS[ext]
        file_size = os.path.getsize(file_path)
        print(f"  File:   {file_path}")
        print(f"  Size:   {file_size:,} bytes")
        print(f"  Codec:  {codec_name}")
        print(f"  Port:   {port}")
        print()

        with open(file_path, "rb") as f:
            file_bytes = f.read()

        b64_data = base64.b64encode(file_bytes).decode("ascii")
        print(f"  Base64 size: {len(b64_data):,} bytes")
        print()

        # Connect to device
        print("  Connecting to device...")
        client = RpcClient(
            port,
            timeout=timeout,
            serial_interface=serial_interface,
            verbose=True,
        )
        await client.connect(boot_wait=15.0, drain_boot=True)

        # Ping to verify connection
        ping_response = await client.send("ping", retries=3)
        print(f"  Ping OK: {ping_response.data}")
        print()

        # Send decodeFile RPC
        print(f"  Sending {codec_name} data ({file_size:,} bytes) to device...")
        response = await client.send(
            "decodeFile",
            args=[b64_data, ext],
            timeout=timeout,
        )

        print()
        data: dict[str, Any] = response.data
        success = data.get("success", False)

        if not success:
            error_msg = data.get("error", "Unknown error")
            print(f"{Fore.RED}DEVICE DECODE FAILED: {error_msg}{Style.RESET_ALL}")
            return 1

        # Display results
        width = data.get("frame_width", data.get("width", "?"))
        height = data.get("frame_height", data.get("height", "?"))
        print(f"  Decoded frame: {width}x{height}")

        pixels = data.get("pixels", [])
        if pixels:
            print(f"  First {len(pixels)} pixels:")
            _print_pixel_table(pixels)

        print()
        print(
            f"{Fore.GREEN}DEVICE DECODE VALIDATION PASSED ({codec_name}){Style.RESET_ALL}"
        )
        return 0

    except RpcTimeoutError as e:
        print(f"{Fore.RED}RPC Timeout: {e}{Style.RESET_ALL}")
        return 1
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        return 1  # unreachable but keeps type checker happy
    except RuntimeError as e:
        print(f"{Fore.RED}Error: {e}{Style.RESET_ALL}")
        return 1
    finally:
        if client:
            await client.close()
        if temp_download and os.path.exists(temp_download):
            os.unlink(temp_download)


async def run_decode_validation(decode_path: str) -> int:
    """Run decode validation on a local file or URL (host-only mode).

    Args:
        decode_path: Local file path or URL to a media file.

    Returns:
        Exit code (0 = success, 1 = failure).
    """
    print()
    print("=" * 60)
    print("DECODE VALIDATION MODE")
    print("=" * 60)
    print()

    temp_download: str | None = None

    try:
        # Resolve the file
        file_path, ext, temp_download = _resolve_file(decode_path)

        # Validate extension
        if ext not in SUPPORTED_EXTENSIONS:
            supported = ", ".join(sorted(SUPPORTED_EXTENSIONS.keys()))
            print(
                f"{Fore.RED}Error: Unsupported file extension '{ext}'{Style.RESET_ALL}"
            )
            print(f"  Supported: {supported}")
            return 1

        # Validate file exists
        if not os.path.isfile(file_path):
            print(f"{Fore.RED}Error: File not found: {file_path}{Style.RESET_ALL}")
            return 1

        codec_name = SUPPORTED_EXTENSIONS[ext]
        file_size = os.path.getsize(file_path)
        print(f"  File:  {file_path}")
        print(f"  Size:  {file_size:,} bytes")
        print(f"  Codec: {codec_name}")
        print()

        # Read file and base64-encode into JSON-RPC payload
        with open(file_path, "rb") as f:
            file_bytes = f.read()

        payload_json = _build_jsonrpc_payload(file_bytes, ext)

        # Write payload to well-known path (C++ test reads this by default)
        project_root = Path(__file__).resolve().parent.parent
        payload_file = project_root / PAYLOAD_PATH
        payload_file.parent.mkdir(parents=True, exist_ok=True)
        payload_file.write_text(payload_json, encoding="utf-8")

        b64_size = len(base64.b64encode(file_bytes))
        print(f"  Base64 payload: {b64_size:,} bytes")
        print(f"  Payload file:   {payload_file}")
        print()

        # Run the C++ decode test
        print(f"  Running decode_file test for {codec_name}...")
        print()

        uv_exe = shutil.which("uv")
        if uv_exe is None:
            print(f"{Fore.RED}Error: 'uv' not found in PATH{Style.RESET_ALL}")
            return 1

        result = subprocess.run(
            [uv_exe, "run", "test.py", "decode_file", "--cpp"],
            cwd=str(project_root),
        )

        print()
        if result.returncode == 0:
            print(
                f"{Fore.GREEN}DECODE VALIDATION PASSED ({codec_name}){Style.RESET_ALL}"
            )
        else:
            print(f"{Fore.RED}DECODE VALIDATION FAILED ({codec_name}){Style.RESET_ALL}")

        return result.returncode

    except RuntimeError as e:
        print(f"{Fore.RED}Error: {e}{Style.RESET_ALL}")
        return 1

    finally:
        # Clean up downloaded temp file (payload stays in .build/)
        if temp_download and os.path.exists(temp_download):
            os.unlink(temp_download)
