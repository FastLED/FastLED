import os
import shutil
import subprocess
import tempfile
import threading
import time
import zipfile
import zlib
from pathlib import Path
from threading import Timer

import os
import subprocess
from typing import List
import hashlib

# import tempdir
from tempfile import TemporaryDirectory

from fastapi import (BackgroundTasks, FastAPI, File, Header,  # type: ignore
                     HTTPException, UploadFile)
from fastapi.responses import FileResponse, RedirectResponse, StreamingResponse  # type: ignore

_TEST = False
_UPLOAD_LIMIT = 10 * 1024 * 1024
# Protect the endpoints from random bots.
# Note that that the wasm_compiler.py greps for this string to get the URL of the server.
# Changing the name could break the compiler.
_AUTH_TOKEN = "oBOT5jbsO4ztgrpNsQwlmFLIKB"

_GIT_UPDATE_INTERVAL = 600  # Fetch the git repository every 10 mins.
_GIT_REPO_PATH = "/js/fastled"  # Path to the git repository


def update_git_repo():
    """Update git repository by fetching and resetting to origin/main."""
    try:
        print("\nAttempting to update git repository...")
        with compile_lock:
            # Change to repo directory
            os.chdir(_GIT_REPO_PATH)
            # Fetch and reset to origin/main
            subprocess.run(["git", "fetch", "origin"], check=True)
            subprocess.run(["git", "reset", "--hard", "origin/master"], check=True)
            print("Git repository updated successfully")
    except Exception as e:
        print(f"Error updating git repository: {e}")
    finally:
        # Schedule next update
        Timer(_GIT_UPDATE_INTERVAL, update_git_repo).start()

app = FastAPI()
Timer(_GIT_UPDATE_INTERVAL, update_git_repo).start()  # Start the periodic git update
upload_dir = Path("uploads")
upload_dir.mkdir(exist_ok=True)
compile_lock = threading.Lock()

output_dir = Path("/output")
output_dir.mkdir(exist_ok=True)


def collect_files(directory: str, extensions: List[str]) -> List[Path]:
    """Collect files with specific extensions from a directory.

    Args:
        directory (str): The directory to scan for files.
        extensions (List[str]): The list of file extensions to include.

    Returns:
        List[str]: A list of file paths matching the extensions.
    """
    files: list[Path] = []
    for root, _, filenames in os.walk(directory):
        for filename in filenames:
            if any(filename.endswith(ext) for ext in extensions):
                files.append(Path(os.path.join(root, filename)))
    return files

def concatenate_files(file_list: List[Path], output_file: str) -> None:
    """Concatenate files into a single output file.

    Args:
        file_list (List[str]): List of file paths to concatenate.
        output_file (str): Path to the output file.
    """
    with open(output_file, 'w', encoding='utf-8') as outfile:
        for file_path in file_list:
            outfile.write(f"// File: {file_path}\n")
            with open(file_path, 'r', encoding='utf-8') as infile:
                outfile.write(infile.read())
                outfile.write("\n\n")

# return a hash
def preprocess_with_gcc(input_file: str, output_file: str) -> None:
    """Preprocess a file with GCC, leaving #include directives intact.

    Args:
        input_file (str): Path to the input file.
        output_file (str): Path to the preprocessed output file.
    """
    gcc_command = [
        "gcc", "-E", "-fdirectives-only", input_file, "-o", output_file
    ]
    try:
        subprocess.run(gcc_command, check=True)
        print(f"Preprocessed file saved to {output_file}")
    except subprocess.CalledProcessError as e:
        print(f"GCC preprocessing failed: {e}")



def hash_string(s: str) -> str:
    # sha 256

    return hashlib.sha256(s.encode()).hexdigest()


def generate_hash_of_src_files(root_dir: Path) -> str:
    """Generate a hash of all files in a directory.

    Args:
        root_dir (Path): The root directory to hash.

    Returns:
        str: The hash of all src files in the directory like cpp and hpp, h.
    """
    extensions = ['.cpp', '.hpp', '.h']
    files: list[Path] = collect_files(root_dir, extensions)
    try:
        with TemporaryDirectory() as temp_dir:
            temp_file = Path(temp_dir) / "concatenated_output.cpp"
            preprocessed_file = Path(temp_dir) / "preprocessed_output.cpp"
            concatenate_files(files, temp_file)
            preprocess_with_gcc(temp_file, preprocessed_file)
            contents = preprocessed_file.read_text()
            # strip the last line in it:
            contents = contents.split("\n")[:-1]
            out_lines: list[str] = []
            for line in contents:
                if "concatenated_output.cpp" not in line:
                    out_lines.append(line)
            contents = "\n".join(out_lines)
            # print("contents: ", contents)
            return hash_string(contents)
    except Exception as e:
        import traceback
        stack_trae = traceback.format_exc()
        print(stack_trae)
        print(f"Error generating hash of src files: {e}")
        return "error"



def compile_source(temp_src_dir: Path, file_path: Path, background_tasks: BackgroundTasks, build_mode: str, profile: bool) -> FileResponse | HTTPException:
    """Compile source code and return compiled artifacts as a zip file."""
    temp_zip_dir = None
    try:
        # Find the first directory in temp_src_dir
        src_dir = next(Path(temp_src_dir).iterdir())
        print(f"\nFound source directory: {src_dir}")
    except StopIteration:
        return HTTPException(
            status_code=500,
            detail=f"No files found in extracted directory: {temp_src_dir}"
        )
    
    print(f"Files are ready, waiting for compile lock...")
    compile_lock_start = time.time()
    with compile_lock:
        compile_lock_end = time.time()
        print("\nRunning compiler...")
        cmd = ["python", "run.py", "compile", f"--mapped-dir={temp_src_dir}", f"--{build_mode}"]
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
        stdout = proc.communicate()[0]
        return_code = proc.returncode
        if return_code != 0:
            return HTTPException(
                status_code=400,
                detail=f"Compilation failed with return code {return_code}:\n{stdout}"
            )
    compile_time = time.time() - compile_lock_end
    compile_lock_time = compile_lock_end - compile_lock_start
        
    print(f"\nCompiler output:\nstdout:\n{stdout}")
    print(f"Compile lock time: {compile_lock_time:.2f}s")
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
            detail=f"Compilation artifacts not found at {fastled_js_dir}"
        )

    # Replace separate stdout/stderr files with single out.txt
    out_txt = fastled_js_dir / "out.txt"
    perf_txt = fastled_js_dir / "perf.txt"
    print(f"\nSaving combined output to: {out_txt}")
    out_txt.write_text(stdout)
    perf_txt.write_text(f"Compile lock time: {compile_lock_time:.2f}s\nCompile time: {compile_time:.2f}s")

    output_dir.mkdir(exist_ok=True)  # Ensure output directory exists
    output_zip_path = output_dir / f"fastled_output_{hash(str(file_path))}.zip"
    print(f"\nCreating output zip at: {output_zip_path}")
    start_zip = time.time()
    try:
        with zipfile.ZipFile(output_zip_path, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as zip_out:
            print("\nAdding files to output zip:")
            for file_path in fastled_js_dir.rglob("*"):
                if file_path.is_file():
                    arc_path = file_path.relative_to(fastled_js_dir)
                    print(f"  Adding: {arc_path}")
                    zip_out.write(file_path, arc_path)
    except zipfile.BadZipFile as e:
        print(f"Error creating zip file: {e}")
        return HTTPException(
            status_code=500,
            detail=f"Failed to create zip file: {e}"
        )
    except zlib.error as e:
        print(f"Compression error: {e}")
        return HTTPException(
            status_code=500,
            detail=f"Zip compression failed - zlib error: {e}"
        )
    except Exception as e:
        print(f"Unexpected error creating zip: {e}")
        return HTTPException(
            status_code=500,
            detail=f"Failed to create zip file: {e}"
        )
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
        background=background_tasks
    )

from dataclasses import dataclass

@dataclass
class CacheEntry:
    hash: str
    data: bytes
    last_access: float

CACHE_LOCK = threading.Lock()
CACHE: dict[str, CacheEntry] = {}
CACHE_MAX_SIZE = 1 * 1024 * 1024
CACHE_MAX_ENTRIES = 50

def try_get_cached_zip(hash: str) -> bytes | None:
    with CACHE_LOCK:
        entry = CACHE.get(hash)
        if entry is None:
            return None
        entry.last_access = time.time()
        return entry.data

def cache_zip(hash: str, data: bytes) -> None:
    if len(data) > CACHE_MAX_SIZE:
        print("Data too large to cache")
        return None
    with CACHE_LOCK:
        if len(CACHE) >= CACHE_MAX_ENTRIES:
            # Remove the oldest entry
            oldest_key = min(CACHE, key=lambda k: CACHE[k].last_access)
            del CACHE[oldest_key]
        CACHE[hash] = CacheEntry(hash=hash, data=data, last_access=time.time())
        



@app.get("/", include_in_schema=False)
async def read_root() -> RedirectResponse:
    """Redirect to the /docs endpoint."""
    return RedirectResponse(url="/docs")


@app.get("/healthz")
async def healthz() -> dict:
    """Health check endpoint."""
    return {"status": "ok"}


@app.post("/compile/wasm")
async def compile_wasm(
    file: UploadFile = File(...),
    authorization: str = Header(None),
    build: str = Header(None),
    profile: str = Header(None),
    background_tasks: BackgroundTasks = BackgroundTasks()
) -> FileResponse:
    """Upload a file into a temporary directory."""
    if build is not None:
        build = build.lower()

    if build not in ["quick", "release", "debug", None]:
         raise HTTPException(
             status_code=400,
             detail="Invalid build mode. Must be one of 'quick', 'release', or 'debug' or omitted"
         )
    if profile is not None:
        profile = profile.lower() == "true" or profile.lower() == "1"
    else:
        profile = False
    print(f"Build mode is {build}")
    build = build or "quick"
    print(f"Starting upload process for file: {file.filename}")

    if not _TEST and authorization != _AUTH_TOKEN:
        raise HTTPException(status_code=401, detail="Unauthorized")

    if not file.filename.endswith('.zip'):
        return {"error": "Only .zip files are allowed."}

    if file.size > _UPLOAD_LIMIT:
        return {"error": f"File size exceeds {_UPLOAD_LIMIT} byte limit."}

    temp_zip_dir = None
    temp_src_dir = None
    
    try:
        # Create temporary directories - one for zip, one for source
        temp_zip_dir = tempfile.mkdtemp()
        temp_src_dir = tempfile.mkdtemp()
        print(f"Created temporary directories:\nzip_dir: {temp_zip_dir}\nsrc_dir: {temp_src_dir}")
        
        file_path = Path(temp_zip_dir) / file.filename
        print(f"Saving uploaded file to: {file_path}")
        
        with open(file_path, "wb") as f:
            shutil.copyfileobj(file.file, f)

        print("extracting zip file...")
        with zipfile.ZipFile(file_path, 'r') as zip_ref:
            zip_ref.extractall(temp_src_dir)
            hash_value = generate_hash_of_src_files(Path(temp_src_dir))

        from io import BytesIO
        from tempfile import NamedTemporaryFile

        print(f"Hash of source files: {hash_value}")
        entry: bytes | None = try_get_cached_zip(hash_value)
        if entry is not None:
            print("Returning cached zip file")
            # Create a temporary file for the cached data
            tmp_file = NamedTemporaryFile(delete=False)
            tmp_file.write(entry)
            tmp_file.close()
            
            def cleanup_temp():
                try:
                    os.unlink(tmp_file.name)
                except:
                    pass
                    
            background_tasks.add_task(cleanup_temp)
            
            return FileResponse(
                path=tmp_file.name,
                media_type="application/zip",
                filename="fastled_output.zip",
                background=background_tasks
            )
        
        print("\nContents of source directory:")
        for path in Path(temp_src_dir).rglob("*"):
            print(f"  {path}")
        out = compile_source(Path(temp_src_dir), file_path, background_tasks, build, profile)
        if isinstance(out, HTTPException):
            print("Raising HTTPException")
            raise out
        # Cache the compiled zip file
        # get bytes from the file object
        path = out.path
        with open(path, "rb") as f:
            data = f.read()
        cache_zip(hash_value, data)
        return out
    except HTTPException as e:
        print(f"HTTPException in upload process: {str(e)}")
        raise e

    except Exception as e:
        print(f"Error in upload process: {str(e)}")
        # Clean up in case of error
        if temp_zip_dir:
            shutil.rmtree(temp_zip_dir, ignore_errors=True)
        if temp_src_dir:
            shutil.rmtree(temp_src_dir, ignore_errors=True)
        raise HTTPException(
            status_code=500,
            detail=f"Upload process failed: {str(e)}\nTrace: {e.__traceback__}"
        )

