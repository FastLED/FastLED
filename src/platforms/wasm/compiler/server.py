import json
import os
import shutil
import subprocess
import tempfile
import threading
import time
import warnings
import zipfile
import zlib
from pathlib import Path
from tempfile import NamedTemporaryFile
from threading import Timer
from typing import Callable

from disklru import DiskLRUCache  # type: ignore
from fastapi import Header  # type: ignore
from fastapi import BackgroundTasks, FastAPI, File, HTTPException, UploadFile
from fastapi.responses import FileResponse, RedirectResponse  # type: ignore
from sketch_hasher import generate_hash_of_project_files
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.requests import Request
from starlette.responses import Response

_VOLUME_MAPPED_SRC = Path("/host/fastled/src")
_RSYNC_DEST = Path("/js/fastled/src")

_TEST = False
_UPLOAD_LIMIT = 10 * 1024 * 1024
# Protect the endpoints from random bots.
# Note that that the wasm_compiler.py greps for this string to get the URL of the server.
# Changing the name could break the compiler.
_AUTH_TOKEN = "oBOT5jbsO4ztgrpNsQwlmFLIKB"

_LIVE_GIT_UPDATES_INTERVAL = int(
    os.environ.get("LIVE_GIT_UPDATE_INTERVAL", 60 * 60 * 24)
)  # Update every 24 hours
_ALLOW_SHUTDOWN = os.environ.get("ALLOW_SHUTDOWN", "false").lower() in ["true", "1"]
_NO_SKETCH_CACHE = os.environ.get("NO_SKETCH_CACHE", "false").lower() in ["true", "1"]
_LIVE_GIT_FASTLED_DIR = Path("/git/fastled2")

# TODO - cleanup
_NO_AUTO_UPDATE = (
    os.environ.get("NO_AUTO_UPDATE", "0") in ["1", "true"]
    or _VOLUME_MAPPED_SRC.exists()
)
_LIVE_GIT_UPDATES_ENABLED = (not _NO_AUTO_UPDATE) or (
    os.environ.get("LIVE_GIT_UPDATES", "0") in ["1", "true"]
)
_START_TIME = time.time()


if _NO_SKETCH_CACHE:
    print("Sketch caching disabled")

UPLOAD_DIR = Path("/uploads")
UPLOAD_DIR.mkdir(exist_ok=True)
COMPILE_LOCK = threading.Lock()

OUTPUT_DIR = Path("/output")
OUTPUT_DIR.mkdir(exist_ok=True)

# Initialize disk cache
SKETCH_CACHE_FILE = OUTPUT_DIR / "compile_cache.db"
SKETCH_CACHE_MAX_ENTRIES = 50
SKETCH_CACHE = DiskLRUCache(str(SKETCH_CACHE_FILE), SKETCH_CACHE_MAX_ENTRIES)


app = FastAPI()


class UploadSizeMiddleware(BaseHTTPMiddleware):
    def __init__(self, app, max_upload_size: int):
        super().__init__(app)
        self.max_upload_size = max_upload_size

    async def dispatch(self, request: Request, call_next):
        if request.method == "POST" and "/compile/wasm" in request.url.path:
            print(
                f"Upload request with content-length: {request.headers.get('content-length')}"
            )
            content_length = request.headers.get("content-length")
            if content_length:
                content_length = int(content_length)  # type: ignore
                if content_length > self.max_upload_size:  # type: ignore
                    return Response(
                        status_code=413,
                        content=f"File size exceeds {self.max_upload_size} byte limit, for large assets please put them in data/ directory to avoid uploading them to the server.",
                    )
        return await call_next(request)


app.add_middleware(UploadSizeMiddleware, max_upload_size=_UPLOAD_LIMIT)


def update_live_git_repo() -> None:
    if not _LIVE_GIT_UPDATES_ENABLED:
        return
    try:
        if not _LIVE_GIT_FASTLED_DIR.exists():
            subprocess.run(
                [
                    "git",
                    "clone",
                    "https://github.com/fastled/fastled.git",
                    "/git/fastled2",
                    "--depth=1",
                ],
                check=True,
            )
            print("Cloned live FastLED repository")
        else:
            print("Updating live FastLED repository")
            subprocess.run(
                ["git", "fetch", "origin"],
                check=True,
                capture_output=True,
                cwd=_LIVE_GIT_FASTLED_DIR,
            )
            subprocess.run(
                ["git", "reset", "--hard", "origin/master"],
                check=True,
                capture_output=True,
                cwd=_LIVE_GIT_FASTLED_DIR,
            )
            print("Live FastLED repository updated successfully")
    except subprocess.CalledProcessError as e:
        warnings.warn(
            f"Error updating live FastLED repository: {e.stdout}\n\n{e.stderr}"
        )


def try_get_cached_zip(hash: str) -> bytes | None:
    if _NO_SKETCH_CACHE:
        print("Sketch caching disabled, skipping cache get")
        return None
    return SKETCH_CACHE.get_bytes(hash)


def cache_put(hash: str, data: bytes) -> None:
    if _NO_SKETCH_CACHE:
        print("Sketch caching disabled, skipping cache put")
        return
    SKETCH_CACHE.put_bytes(hash, data)


def sync_src_to_target(
    src: Path, dst: Path, callback: Callable[[], None] | None = None
) -> bool:
    """Sync the volume mapped source directory to the FastLED source directory."""
    suppress_print = (
        _START_TIME + 30 > time.time()
    )  # Don't print during initial volume map.
    if not src.exists():
        # Volume is not mapped in so we don't rsync it.
        print(f"Skipping rsync, as fastled src at {src} doesn't exist")
        return False
    try:
        print("\nSyncing source directories...")
        with COMPILE_LOCK:
            # Use rsync to copy files, preserving timestamps and deleting removed files
            cp: subprocess.CompletedProcess = subprocess.run(
                ["rsync", "-av", "--info=NAME", "--delete", f"{src}/", f"{dst}/"],
                check=True,
                text=True,
                capture_output=True,
            )
            if cp.returncode == 0:
                changed = False
                changed_lines: list[str] = []
                lines = cp.stdout.split("\n")
                for line in lines:
                    suffix = line.strip().split(".")[-1]
                    if suffix in ["cpp", "h", "hpp", "ino", "py", "js", "html", "css"]:
                        if not suppress_print:
                            print(f"Changed file: {line}")
                        changed = True
                        changed_lines.append(line)
                if changed:
                    if not suppress_print:
                        print(f"FastLED code had updates: {changed_lines}")
                    if callback:
                        callback()
                    return True
                print("Source directory synced successfully with no changes")
                return False
            else:
                print(f"Error syncing directories: {cp.stdout}\n\n{cp.stderr}")
                return False

    except subprocess.CalledProcessError as e:
        print(f"Error syncing directories: {e.stdout}\n\n{e.stderr}")
    except Exception as e:
        print(f"Error syncing directories: {e}")
    return False


def sync_source_directory_if_volume_is_mapped(
    callback: Callable[[], None] | None = None
) -> bool:
    """Sync the volume mapped source directory to the FastLED source directory."""
    if not _VOLUME_MAPPED_SRC.exists():
        # Volume is not mapped in so we don't rsync it.
        print("Skipping rsync, as fastled src volume not mapped")
        return False
    return sync_src_to_target(_VOLUME_MAPPED_SRC, _RSYNC_DEST, callback=callback)


def sync_live_git_to_target() -> None:
    if not _LIVE_GIT_UPDATES_ENABLED:
        return
    update_live_git_repo()  # no lock

    def on_files_changed() -> None:
        print("FastLED source changed from github repo, clearing disk cache.")
        SKETCH_CACHE.clear()

    sync_src_to_target(
        _LIVE_GIT_FASTLED_DIR / "src", _RSYNC_DEST, callback=on_files_changed
    )
    # Basically a setTimeout() in JS.
    Timer(
        _LIVE_GIT_UPDATES_INTERVAL, sync_live_git_to_target
    ).start()  # Start the periodic git update


def compile_source(
    temp_src_dir: Path,
    file_path: Path,
    background_tasks: BackgroundTasks,
    build_mode: str,
    profile: bool,
    hash_value: str | None = None,
) -> FileResponse | HTTPException:
    """Compile source code and return compiled artifacts as a zip file."""
    temp_zip_dir = None
    try:
        # Find the first directory in temp_src_dir
        src_dir = next(Path(temp_src_dir).iterdir())
        print(f"\nFound source directory: {src_dir}")
    except StopIteration:
        return HTTPException(
            status_code=500,
            detail=f"No files found in extracted directory: {temp_src_dir}",
        )

    print("Files are ready, waiting for compile lock...")
    COMPILE_LOCK_start = time.time()
    with COMPILE_LOCK:
        COMPILE_LOCK_end = time.time()

        print("\nRunning compiler...")
        cmd = [
            "python",
            "run.py",
            "compile",
            f"--mapped-dir={temp_src_dir}",
        ]
        cmd.append(f"--{build_mode.lower()}")
        if profile:
            cmd.append("--profile")
        # cp = subprocess.run(cmd, cwd="/js", capture_output=True, text=True)
        # cp = subprocess.run(cmd, cwd="/js", stdout=subprocess.STDOUT, stderr=subprocess.STDOUT, text=True)
        proc = subprocess.Popen(
            cmd,
            cwd="/js",
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        assert proc.stdout is not None
        stdout_lines: list[str] = []

        for line in iter(proc.stdout.readline, ""):
            print(line, end="")
            stdout_lines.append(line)
        stdout = "".join(stdout_lines)
        proc.stdout.close()
        return_code = proc.wait()
        if return_code != 0:
            print(f"Compilation failed with return code {return_code}:\n{stdout}")
            return HTTPException(
                status_code=400,
                detail=f"Compilation failed with return code {return_code}:\n{stdout}",
            )
    compile_time = time.time() - COMPILE_LOCK_end
    COMPILE_LOCK_time = COMPILE_LOCK_end - COMPILE_LOCK_start

    print(f"\nCompiler output:\nstdout:\n{stdout}")
    print(f"Compile lock time: {COMPILE_LOCK_time:.2f}s")
    print(f"Compile time: {compile_time:.2f}s")

    # Find the fastled_js directory
    fastled_js_dir = src_dir / "fastled_js"
    print(f"\nLooking for fastled_js directory at: {fastled_js_dir}")

    if not fastled_js_dir.exists():
        print(f"Directory contents of {src_dir}:")
        for path in src_dir.rglob("*"):
            print(f"  {path}")
        return HTTPException(
            status_code=500,
            detail=f"Compilation artifacts not found at {fastled_js_dir}",
        )

    # Replace separate stdout/stderr files with single out.txt
    out_txt = fastled_js_dir / "out.txt"
    perf_txt = fastled_js_dir / "perf.txt"
    hash_txt = fastled_js_dir / "hash.txt"
    print(f"\nSaving combined output to: {out_txt}")
    out_txt.write_text(stdout)
    perf_txt.write_text(
        f"Compile lock time: {COMPILE_LOCK_time:.2f}s\nCompile time: {compile_time:.2f}s"
    )
    if hash_value is not None:
        hash_txt.write_text(hash_value)

    OUTPUT_DIR.mkdir(exist_ok=True)  # Ensure output directory exists
    output_zip_path = OUTPUT_DIR / f"fastled_output_{hash(str(file_path))}.zip"
    print(f"\nCreating output zip at: {output_zip_path}")
    start_zip = time.time()
    try:
        with zipfile.ZipFile(
            output_zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9
        ) as zip_out:
            print("\nAdding files to output zip:")
            for file_path in fastled_js_dir.rglob("*"):
                if file_path.is_file():
                    arc_path = file_path.relative_to(fastled_js_dir)
                    print(f"  Adding: {arc_path}")
                    zip_out.write(file_path, arc_path)
    except zipfile.BadZipFile as e:
        print(f"Error creating zip file: {e}")
        return HTTPException(status_code=500, detail=f"Failed to create zip file: {e}")
    except zlib.error as e:
        print(f"Compression error: {e}")
        return HTTPException(
            status_code=500, detail=f"Zip compression failed - zlib error: {e}"
        )
    except Exception as e:
        print(f"Unexpected error creating zip: {e}")
        return HTTPException(status_code=500, detail=f"Failed to create zip file: {e}")
    zip_time = time.time() - start_zip
    print(f"Zip file created in {zip_time:.2f}s")

    def cleanup_files():
        if output_zip_path.exists():
            output_zip_path.unlink()
        if temp_zip_dir:
            shutil.rmtree(temp_zip_dir, ignore_errors=True)
        if temp_src_dir:
            shutil.rmtree(temp_src_dir, ignore_errors=True)

    background_tasks.add_task(cleanup_files)

    return FileResponse(
        path=output_zip_path,
        media_type="application/zip",
        filename="fastled_output.zip",
        background=background_tasks,
    )


def get_settings() -> dict:
    settings = {
        "ALLOW_SHUTDOWN": _ALLOW_SHUTDOWN,
        "NO_AUTO_UPDATE": os.environ.get("NO_AUTO_UPDATE", "0"),
        "NO_SKETCH_CACHE": _NO_SKETCH_CACHE,
        "LIVE_GIT_UPDATES_ENABLED": _LIVE_GIT_UPDATES_ENABLED,
        "LIVE_GIT_UPDATES_INTERVAL": _LIVE_GIT_UPDATES_INTERVAL,
        "UPLOAD_LIMIT": _UPLOAD_LIMIT,
        "VOLUME_MAPPED_SRC": str(_VOLUME_MAPPED_SRC),
        "VOLUME_MAPPED_SRC_EXISTS": _VOLUME_MAPPED_SRC.exists(),
    }
    return settings


# on startup
@app.on_event("startup")
def startup_event():
    """Run on startup."""
    print("Starting FastLED wasm compiler server...")
    try:
        print(f"Settings: {json.dumps(get_settings(), indent=2)}")
    except Exception as e:
        print(f"Error getting settings: {e}")
    sync_source_directory_if_volume_is_mapped()
    if _LIVE_GIT_UPDATES_ENABLED:
        Timer(
            _LIVE_GIT_UPDATES_INTERVAL, sync_live_git_to_target
        ).start()  # Start the periodic git update
    else:
        print("Auto updates disabled")


@app.get("/", include_in_schema=False)
async def read_root() -> RedirectResponse:
    """Redirect to the /docs endpoint."""
    return RedirectResponse(url="/docs")


@app.get("/healthz")
async def healthz() -> dict:
    """Health check endpoint."""
    return {"status": "ok"}


if _ALLOW_SHUTDOWN:

    @app.get("/shutdown")
    async def shutdown() -> dict:
        """Shutdown the server."""
        print("Shutting down server...")
        SKETCH_CACHE.close()
        os._exit(0)
        return {"status": "ok"}


@app.get("/settings")
async def settings() -> dict:
    """Get the current settings."""
    settings = {
        "ALLOW_SHUTDOWN": _ALLOW_SHUTDOWN,
        "NO_AUTO_UPDATE": os.environ.get("NO_AUTO_UPDATE", "0"),
        "NO_SKETCH_CACHE": _NO_SKETCH_CACHE,
        "LIVE_GIT_UPDATES_ENABLED": _LIVE_GIT_UPDATES_ENABLED,
        "LIVE_GIT_UPDATES_INTERVAL": _LIVE_GIT_UPDATES_INTERVAL,
        "UPLOAD_LIMIT": _UPLOAD_LIMIT,
        "VOLUME_MAPPED_SRC": str(_VOLUME_MAPPED_SRC),
        "VOLUME_MAPPED_SRC_EXISTS": _VOLUME_MAPPED_SRC.exists(),
    }
    return settings


@app.get("/compile/wasm/inuse")
async def compiler_in_use() -> dict:
    """Check if the compiler is in use."""
    return {"in_use": COMPILE_LOCK.locked()}


# THIS MUST NOT BE ASYNC!!!!
@app.post("/compile/wasm")
def compile_wasm(
    file: UploadFile = File(...),
    authorization: str = Header(None),
    build: str = Header(None),
    profile: str = Header(None),
    background_tasks: BackgroundTasks = BackgroundTasks(),
) -> FileResponse:
    """Upload a file into a temporary directory."""
    if build is not None:
        build = build.lower()

    if build not in ["quick", "release", "debug", None]:
        raise HTTPException(
            status_code=400,
            detail="Invalid build mode. Must be one of 'quick', 'release', or 'debug' or omitted",
        )
    do_profile: bool = False
    if profile is not None:
        do_profile = profile.lower() == "true" or profile.lower() == "1"
    print(f"Build mode is {build}")
    build = build or "quick"
    print(f"Starting upload process for file: {file.filename}")

    if not _TEST and authorization != _AUTH_TOKEN:
        raise HTTPException(status_code=401, detail="Unauthorized")

    if file is None:
        raise HTTPException(status_code=400, detail="No file uploaded.")

    if file.filename is None:
        raise HTTPException(status_code=400, detail="No filename provided.")

    if not file.filename.endswith(".zip"):
        raise HTTPException(
            status_code=400, detail="Uploaded file must be a zip archive."
        )

    temp_zip_dir = None
    temp_src_dir = None

    try:
        # Create temporary directories - one for zip, one for source
        temp_zip_dir = tempfile.mkdtemp()
        temp_src_dir = tempfile.mkdtemp()
        print(
            f"Created temporary directories:\nzip_dir: {temp_zip_dir}\nsrc_dir: {temp_src_dir}"
        )

        file_path = Path(temp_zip_dir) / file.filename
        print(f"Saving uploaded file to: {file_path}")

        # Simple file save since size is already checked by middleware
        with open(file_path, "wb") as f:
            shutil.copyfileobj(file.file, f)

        print("extracting zip file...")
        hash_value: str | None = None
        with zipfile.ZipFile(file_path, "r") as zip_ref:
            zip_ref.extractall(temp_src_dir)
            try:
                hash_value = generate_hash_of_project_files(Path(temp_src_dir))
            except Exception as e:
                warnings.warn(
                    f"Error generating hash: {e}, fast cache access is disabled for this build."
                )

        def on_files_changed() -> None:
            print("Source files changed, clearing cache")
            SKETCH_CACHE.clear()

        sync_source_directory_if_volume_is_mapped(callback=on_files_changed)

        entry: bytes | None = None
        if hash_value is not None:
            print(f"Hash of source files: {hash_value}")
            entry = try_get_cached_zip(hash_value)
        if entry is not None:
            print("Returning cached zip file")
            # Create a temporary file for the cached data
            tmp_file = NamedTemporaryFile(delete=False)
            tmp_file.write(entry)
            tmp_file.close()

            def cleanup_temp():
                try:
                    os.unlink(tmp_file.name)
                except:  # noqa: E722
                    pass

            background_tasks.add_task(cleanup_temp)

            return FileResponse(
                path=tmp_file.name,
                media_type="application/zip",
                filename="fastled_output.zip",
                background=background_tasks,
            )

        print("\nContents of source directory:")
        for path in Path(temp_src_dir).rglob("*"):
            print(f"  {path}")
        out = compile_source(
            Path(temp_src_dir),
            file_path,
            background_tasks,
            build,
            do_profile,
            hash_value,
        )
        if isinstance(out, HTTPException):
            print("Raising HTTPException")
            txt = out.detail
            json_str = json.dumps(txt)
            warnings.warn(f"Error compiling source: {json_str}")
            raise out
        # Cache the compiled zip file
        out_path = Path(out.path)
        data = out_path.read_bytes()
        if hash_value is not None:
            cache_put(hash_value, data)
        return out
    except HTTPException as e:
        import traceback

        stacktrace = traceback.format_exc()
        print(f"HTTPException in upload process: {str(e)}\n{stacktrace}")
        raise e

    except Exception as e:
        import traceback

        stack_trace = traceback.format_exc()
        print(f"Error in upload process: {stack_trace}")
        raise HTTPException(
            status_code=500,
            detail=f"Upload process failed: {str(e)}\nTrace: {e.__traceback__}",
        )
    finally:
        # Clean up in case of error
        if temp_zip_dir:
            shutil.rmtree(temp_zip_dir, ignore_errors=True)
        if temp_src_dir:
            shutil.rmtree(temp_src_dir, ignore_errors=True)
