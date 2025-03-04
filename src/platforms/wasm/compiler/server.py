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
from contextlib import asynccontextmanager
from pathlib import Path
from tempfile import NamedTemporaryFile
from threading import Timer

import psutil  # type: ignore
from code_sync import (  # type: ignore
    sync_source_directory_if_volume_is_mapped,
    sync_src_to_target,
)
from compile_lock import COMPILE_LOCK  # type: ignore
from disklru import DiskLRUCache  # type: ignore
from fastapi import (  # type: ignore
    BackgroundTasks,
    Body,
    FastAPI,
    File,
    Header,
    HTTPException,
    UploadFile,
)
from fastapi.responses import FileResponse, RedirectResponse, Response  # type: ignore
from sketch_hasher import generate_hash_of_project_files  # type: ignore
from starlette.middleware.base import BaseHTTPMiddleware  # type: ignore
from starlette.requests import Request  # type: ignore

_EXAMPLES: list[str] = [
    "Chromancer",
    "LuminescentGrand",
    "wasm",
    "FxAnimartrix",
    "FxCylon",
    "FxDemoReel100",
    "FxFire2012",
    "FxEngine",
    "FxGfx2Video",
    "FxNoisePlusPalette",
    "FxNoiseRing",
    "FxSdCard",
    "FxWater",
]
_VOLUME_MAPPED_SRC = Path("/host/fastled/src")
_RSYNC_DEST = Path("/js/fastled/src")

_TEMP_DIR = Path("/tmp")

_TEST = False
_UPLOAD_LIMIT = 10 * 1024 * 1024
_MEMORY_LIMIT_MB = int(os.environ.get("MEMORY_LIMIT_MB", "0"))  # 0 means disabled
_MEMORY_CHECK_INTERVAL = 0.1  # Check every 100ms
_MEMORY_EXCEEDED_EXIT_CODE = 137  # Standard OOM kill code
# Protect the endpoints from random bots.
# Note that that the wasm_compiler.py greps for this string to get the URL of the server.
# Changing the name could break the compiler.
_AUTH_TOKEN = "oBOT5jbsO4ztgrpNsQwlmFLIKB"

_LIVE_GIT_UPDATES_INTERVAL = int(
    os.environ.get("LIVE_GIT_UPDATE_INTERVAL", 60 * 60 * 24)
)  # Update every 24 hours
_ALLOW_SHUTDOWN = os.environ.get("ALLOW_SHUTDOWN", "false").lower() in ["true", "1"]
_NO_SKETCH_CACHE = os.environ.get("NO_SKETCH_CACHE", "false").lower() in ["true", "1"]
_LIVE_GIT_FASTLED_DIR = Path("/git/fastled")

# TODO - cleanup
_NO_AUTO_UPDATE = (
    os.environ.get("NO_AUTO_UPDATE", "0") in ["1", "true"]
    or _VOLUME_MAPPED_SRC.exists()
)
# This feature is broken. To fix, issue a git update, THEN invoke the compiler command to re-warm the cache.
# otherwise you get worst case scenario on a new compile.
# _LIVE_GIT_UPDATES_ENABLED = (not _NO_AUTO_UPDATE) or (
#     os.environ.get("LIVE_GIT_UPDATES", "0") in ["1", "true"]
# )
_LIVE_GIT_UPDATES_ENABLED = False


if _NO_SKETCH_CACHE:
    print("Sketch caching disabled")

UPLOAD_DIR = Path("/uploads")
UPLOAD_DIR.mkdir(exist_ok=True)
COMPILE_COUNT = 0
COMPILE_FAILURES = 0
COMPILE_SUCCESSES = 0
START_TIME = time.time()

OUTPUT_DIR = Path("/output")
OUTPUT_DIR.mkdir(exist_ok=True)

# Initialize disk cache
SKETCH_CACHE_FILE = OUTPUT_DIR / "compile_cache.db"
SKETCH_CACHE_MAX_ENTRIES = 50
SKETCH_CACHE = DiskLRUCache(str(SKETCH_CACHE_FILE), SKETCH_CACHE_MAX_ENTRIES)


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


@asynccontextmanager  # type: ignore
async def lifespan(app: FastAPI):  # type: ignore
    print("Starting FastLED wasm compiler server...")
    try:
        print(f"Settings: {json.dumps(get_settings(), indent=2)}")
    except Exception as e:
        print(f"Error getting settings: {e}")

    if _MEMORY_LIMIT_MB > 0:
        print(f"Starting memory watchdog (limit: {_MEMORY_LIMIT_MB}MB)")
        memory_watchdog()

    sync_source_directory_if_volume_is_mapped()
    if _LIVE_GIT_UPDATES_ENABLED:
        Timer(
            _LIVE_GIT_UPDATES_INTERVAL, sync_live_git_to_target
        ).start()  # Start the periodic git update
    else:
        print("Auto updates disabled")
    yield  # end startup
    return  # end shutdown


app = FastAPI(lifespan=lifespan)

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
                    str(_LIVE_GIT_FASTLED_DIR),
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
    sync_src_to_target(
        _LIVE_GIT_FASTLED_DIR / "examples",
        _RSYNC_DEST.parent / "examples",
        callback=on_files_changed,
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
    epoch = time.time()

    def _print(msg) -> None:
        diff = time.time() - epoch
        print(f" = SERVER {diff:.2f}s = {msg}")

    _print("Starting compile_source")
    global COMPILE_COUNT
    global COMPILE_FAILURES
    global COMPILE_SUCCESSES
    COMPILE_COUNT += 1
    temp_zip_dir = None
    try:
        # Find the first directory in temp_src_dir
        src_dir = next(Path(temp_src_dir).iterdir())
        _print(f"\nFound source directory: {src_dir}")
    except StopIteration:
        return HTTPException(
            status_code=500,
            detail=f"No files found in extracted directory: {temp_src_dir}",
        )

    _print("Files are ready, waiting for compile lock...")
    COMPILE_LOCK_start = time.time()

    with COMPILE_LOCK:
        COMPILE_LOCK_end = time.time()

        # is_debug = build_mode.lower() == "debug"

        _print("\nRunning compiler...")
        cmd = [
            "python",
            "run.py",
            "compile",
            f"--mapped-dir={temp_src_dir}",
        ]
        # if is_debug:
        #    cmd += ["--no-platformio"]  # fast path that doesn't need a lock.
        cmd.append(f"--{build_mode.lower()}")
        if profile:
            cmd.append("--profile")
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
        _print("Compiler finished.")
        stdout = "".join(stdout_lines)
        proc.stdout.close()
        return_code = proc.wait()
        if return_code != 0:
            COMPILE_FAILURES += 1
            print(f"Compilation failed with return code {return_code}:\n{stdout}")
            return HTTPException(
                status_code=400,
                detail=f"Compilation failed with return code {return_code}:\n{stdout}",
            )
        COMPILE_SUCCESSES += 1
    compile_time = time.time() - COMPILE_LOCK_end
    COMPILE_LOCK_time = COMPILE_LOCK_end - COMPILE_LOCK_start

    print(f"\nCompiler output:\nstdout:\n{stdout}")
    print(f"Compile lock time: {COMPILE_LOCK_time:.2f}s")
    print(f"Compile time: {compile_time:.2f}s")

    # Find the fastled_js directory
    fastled_js_dir = src_dir / "fastled_js"
    print(f"\nLooking for fastled_js directory at: {fastled_js_dir}")

    _print("Looking for fastled_js directory...")
    if not fastled_js_dir.exists():
        print(f"Directory contents of {src_dir}:")
        for path in src_dir.rglob("*"):
            print(f"  {path}")
        return HTTPException(
            status_code=500,
            detail=f"Compilation artifacts not found at {fastled_js_dir}",
        )
    _print("Found fastled_js directory, zipping...")

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
    _print(f"\nCreating output zip at: {output_zip_path}")

    start_zip = time.time()
    try:
        with zipfile.ZipFile(
            output_zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=1
        ) as zip_out:
            _print("\nAdding files to output zip:")
            for file_path in fastled_js_dir.rglob("*"):
                if file_path.is_file():
                    arc_path = file_path.relative_to(fastled_js_dir)
                    _print(f"  Adding: {arc_path}")
                    zip_out.write(file_path, arc_path)
    except zipfile.BadZipFile as e:
        _print(f"Error creating zip file: {e}")
        return HTTPException(status_code=500, detail=f"Failed to create zip file: {e}")
    except zlib.error as e:
        _print(f"Compression error: {e}")
        return HTTPException(
            status_code=500, detail=f"Zip compression failed - zlib error: {e}"
        )
    except Exception as e:
        _print(f"Unexpected error creating zip: {e}")
        return HTTPException(status_code=500, detail=f"Failed to create zip file: {e}")
    zip_time = time.time() - start_zip
    print(f"Zip file created in {zip_time:.2f}s")

    def cleanup_files(output_zip_path=output_zip_path, temp_zip_dir=temp_zip_dir):
        if output_zip_path.exists():
            output_zip_path.unlink()
        if temp_zip_dir:
            shutil.rmtree(temp_zip_dir, ignore_errors=True)
        if temp_src_dir:
            shutil.rmtree(temp_src_dir, ignore_errors=True)

    background_tasks.add_task(cleanup_files)
    _print(f"\nReturning output zip: {output_zip_path}")
    return FileResponse(
        path=output_zip_path,
        media_type="application/zip",
        filename="fastled_output.zip",
        background=background_tasks,
    )


def memory_watchdog() -> None:
    """Monitor memory usage and kill process if it exceeds limit."""
    if _MEMORY_LIMIT_MB <= 0:
        return

    def check_memory() -> None:
        while True:
            process = psutil.Process(os.getpid())
            memory_mb = process.memory_info().rss / 1024 / 1024
            if memory_mb > _MEMORY_LIMIT_MB:
                print(
                    f"Memory limit exceeded! Using {memory_mb:.1f}MB > {_MEMORY_LIMIT_MB}MB limit"
                )
                os._exit(_MEMORY_EXCEEDED_EXIT_CODE)
            time.sleep(_MEMORY_CHECK_INTERVAL)

    watchdog_thread = threading.Thread(target=check_memory, daemon=True)
    watchdog_thread.start()


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


def startup() -> None:
    print("Starting FastLED wasm compiler server...")
    try:
        print(f"Settings: {json.dumps(get_settings(), indent=2)}")
    except Exception as e:
        print(f"Error getting settings: {e}")

    if _MEMORY_LIMIT_MB > 0:
        print(f"Starting memory watchdog (limit: {_MEMORY_LIMIT_MB}MB)")
        memory_watchdog()

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

    print("Endpoint accessed: / (root redirect to docs)")
    return RedirectResponse(url="/docs")


@app.get("/healthz")
async def healthz() -> dict:
    """Health check endpoint."""
    print("Endpoint accessed: /healthz")
    return {"status": "ok"}


if _ALLOW_SHUTDOWN:

    @app.get("/shutdown")
    async def shutdown() -> dict:
        """Shutdown the server."""
        print("Endpoint accessed: /shutdown")
        print("Shutting down server...")
        SKETCH_CACHE.close()
        os._exit(0)
        return {"status": "ok"}


@app.get("/settings")
async def settings() -> dict:
    """Get the current settings."""
    print("Endpoint accessed: /settings")
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
    print("Endpoint accessed: /compile/wasm/inuse")
    return {"in_use": COMPILE_LOCK.locked()}


def zip_example_to_file(example: str, dst_zip_file: Path) -> None:
    examples_dir = Path(f"/js/fastled/examples/{example}")
    if not examples_dir.exists():
        raise HTTPException(
            status_code=404, detail=f"Example {example} not found at {examples_dir}"
        )

    try:
        print(f"Creating zip file at: {dst_zip_file}")
        with zipfile.ZipFile(str(dst_zip_file), "w", zipfile.ZIP_DEFLATED) as zip_out:
            for file_path in examples_dir.rglob("*"):
                if file_path.is_file():
                    if "fastled_js" in file_path.parts:
                        continue
                    arc_path = file_path.relative_to(Path("/js/fastled/examples"))
                    zip_out.write(file_path, arc_path)
        print(f"Zip file created at: {dst_zip_file}")
    except Exception as e:
        warnings.warn(f"Error: {e}")
        raise


def make_random_path_string(digits: int) -> str:
    """Generate a random number."""
    import random
    import string

    return "".join(random.choices(string.ascii_lowercase + string.digits, k=digits))


@app.get("/project/init")
def project_init(background_tasks: BackgroundTasks) -> FileResponse:
    """Archive /js/fastled/examples/wasm into a zip file and return it."""
    print("Endpoint accessed: /project/init")
    # tmp_zip_file = NamedTemporaryFile(delete=False)
    # tmp_zip_path = Path(tmp_zip_file.name)

    tmp_zip_path = _TEMP_DIR / f"wasm-{make_random_path_string(16)}.zip"
    zip_example_to_file("wasm", tmp_zip_path)

    # assert tmp_zip_path.exists()
    if not tmp_zip_path.exists():
        warnings.warn("Failed to create zip file for wasm example.")
        raise HTTPException(
            status_code=500, detail="Failed to create zip file for wasm example."
        )

    def cleanup() -> None:
        try:
            os.unlink(tmp_zip_path)
        except Exception as e:
            warnings.warn(f"Error cleaning up: {e}")

    background_tasks.add_task(cleanup)
    return FileResponse(
        path=tmp_zip_path,
        media_type="application/zip",
        filename="fastled_example.zip",
        background=background_tasks,
    )


@app.post("/project/init")
def project_init_example(
    background_tasks: BackgroundTasks, example: str = Body(...)
) -> FileResponse:
    """Archive /js/fastled/examples/{example} into a zip file and return it."""
    print(f"Endpoint accessed: /project/init/example with example: {example}")
    if ".." in example:
        raise HTTPException(status_code=400, detail="Invalid example name.")
    name = Path("example").name
    tmp_file_path = _TEMP_DIR / f"{name}-{make_random_path_string(16)}.zip"
    zip_example_to_file(example, Path(tmp_file_path))

    if not tmp_file_path.exists():
        warnings.warn(f"Failed to create zip file for {example} example.")
        raise HTTPException(
            status_code=500, detail=f"Failed to create zip file for {example} example."
        )

    def cleanup() -> None:
        try:
            os.unlink(tmp_file_path)
        except Exception as e:
            warnings.warn(f"Error cleaning up: {e}")
            raise

    background_tasks.add_task(cleanup)
    return FileResponse(
        path=tmp_file_path,
        media_type="application/zip",
        filename="fastled_example.zip",
        background=background_tasks,
    )


@app.get("/info")
def info_examples() -> dict:
    """Get a list of examples."""
    print("Endpoint accessed: /info")
    uptime = time.time() - START_TIME
    uptime_fmtd = time.strftime("%H:%M:%S", time.gmtime(uptime))
    try:
        build_timestamp = (
            Path("/image_timestamp.txt").read_text(encoding="utf-8").strip()
        )
    except Exception as e:
        import warnings

        warnings.warn(f"Error reading build timestamp: {e}")
        build_timestamp = "unknown"

    # ARG FASTLED_VERSION=3.9.11
    # ENV FASTLED_VERSION=${FASTLED_VERSION}

    fastled_version = os.environ.get("FASTLED_VERSION", "unknown")
    out = {
        "examples": _EXAMPLES,
        "compile_count": COMPILE_COUNT,
        "compile_failures": COMPILE_FAILURES,
        "compile_successes": COMPILE_SUCCESSES,
        "uptime": uptime_fmtd,
        "build_timestamp": build_timestamp,
        "fastled_version": fastled_version,
    }
    return out


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
    print(f"Endpoint accessed: /compile/wasm with file: {file.filename}")
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
            # Extract everything first
            zip_ref.extractall(temp_src_dir)

            # Then find and remove any platformio.ini files
            platform_files = list(Path(temp_src_dir).rglob("*platformio.ini"))
            if platform_files:
                warnings.warn(f"Removing platformio.ini files: {platform_files}")
                for p in platform_files:
                    p.unlink()

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
