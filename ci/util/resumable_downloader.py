"""Resumable HTTP downloader with chunked download support and automatic retry."""

import _thread
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Optional


class ResumableDownloader:
    """Downloader with resume capability for large files."""

    def __init__(self, chunk_size: int = 8192, max_retries: int = 5):
        self.chunk_size = chunk_size
        self.max_retries = max_retries

    def download(self, url: str, file_path: Path) -> None:
        """Download with resume capability.

        Args:
            url: URL to download
            file_path: Path where to save the file
        """
        import time
        import urllib.error
        import urllib.request

        # Get the total file size
        total_size = self._get_file_size(url)
        if total_size is None:
            print(f"WARNING: Could not determine file size for {url}")
            total_size = 0
        else:
            print(
                f"File size: {total_size:,} bytes ({total_size / (1024 * 1024):.1f} MB)"
            )

        # Check if partial file exists
        start_byte = 0
        if file_path.exists():
            start_byte = file_path.stat().st_size
            if start_byte == total_size:
                print(f"File already completely downloaded: {file_path}")
                return
            elif start_byte > 0:
                print(
                    f"Resuming download from byte {start_byte:,} ({start_byte / (1024 * 1024):.1f} MB)"
                )

        retry_count = 0
        while retry_count <= self.max_retries:
            try:
                self._download_range(url, file_path, start_byte, total_size)
                print(f"SUCCESS: Download completed successfully: {file_path}")
                return
            except (urllib.error.URLError, ConnectionError, OSError) as e:
                retry_count += 1
                current_size = file_path.stat().st_size if file_path.exists() else 0

                if retry_count <= self.max_retries:
                    wait_time = min(2**retry_count, 30)  # Exponential backoff, max 30s
                    print(
                        f"\nCONNECTION LOST: At {current_size:,} bytes. Retry {retry_count}/{self.max_retries} in {wait_time}s..."
                    )
                    time.sleep(wait_time)
                    start_byte = current_size
                else:
                    print(f"\nERROR: Download failed after {self.max_retries} retries")
                    raise
            except KeyboardInterrupt:
                print("\nWARNING: Download interrupted by user")
                _thread.interrupt_main()
                raise

    def _get_file_size(self, url: str) -> Optional[int]:
        """Get the total file size via HEAD request."""
        try:
            import urllib.request

            req = urllib.request.Request(url, method="HEAD")
            with urllib.request.urlopen(req, timeout=30) as response:
                content_length = response.headers.get("Content-Length")
                return int(content_length) if content_length else None
        except KeyboardInterrupt:
            _thread.interrupt_main()
            raise
        except Exception:
            return None

    def _download_range(
        self, url: str, file_path: Path, start_byte: int, total_size: int
    ) -> None:
        """Download from start_byte to end of file."""
        import urllib.request

        # Create range request
        headers: dict[str, str] = {}
        if start_byte > 0:
            headers["Range"] = f"bytes={start_byte}-"

        req = urllib.request.Request(url, headers=headers)

        # Open file in append mode if resuming, write mode if starting fresh
        mode = "ab" if start_byte > 0 else "wb"

        with urllib.request.urlopen(req, timeout=30) as response:
            with open(file_path, mode) as f:
                downloaded = start_byte

                while True:
                    chunk = response.read(self.chunk_size)
                    if not chunk:
                        break

                    f.write(chunk)
                    downloaded += len(chunk)

                    # Progress reporting
                    if total_size > 0:
                        progress = downloaded / total_size * 100
                        mb_downloaded = downloaded / (1024 * 1024)
                        mb_total = total_size / (1024 * 1024)
                        print(
                            f"\rProgress: {progress:.1f}% ({mb_downloaded:.1f}/{mb_total:.1f} MB)",
                            end="",
                            flush=True,
                        )
                    else:
                        mb_downloaded = downloaded / (1024 * 1024)
                        print(
                            f"\rDownloaded: {mb_downloaded:.1f} MB", end="", flush=True
                        )

                print()  # New line after progress
