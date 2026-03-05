"""Decode validation for FastLED codec testing.

Validates media file decoding by reading the file, base64-encoding it,
constructing a JSON-RPC payload, and writing it to .build/decode_payload.json.
The C++ decode_file test reads from that hardcoded path by default.

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

from colorama import Fore, Style


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


async def run_decode_validation(decode_path: str) -> int:
    """Run decode validation on a local file or URL.

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
        if _is_url(decode_path):
            file_path, ext = _download_to_temp(decode_path)
            temp_download = file_path
        else:
            file_path = str(Path(decode_path).resolve())
            ext = _get_extension(file_path)

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
