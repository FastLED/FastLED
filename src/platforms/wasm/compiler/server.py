import hashlib
import os
import shutil
import subprocess
import tempfile
import threading
import time
import warnings
import zipfile
import zlib
from dataclasses import dataclass
from pathlib import Path
from tempfile import NamedTemporaryFile, TemporaryDirectory
from threading import Timer
from typing import List

from fastapi import (BackgroundTasks, FastAPI, File, Header,  # type: ignore
                     HTTPException, UploadFile)
from fastapi.responses import FileResponse, RedirectResponse  # type: ignore


@dataclass
class CacheEntry:
    hash: str
    data: bytes
    last_access: float

_TEST = False
_UPLOAD_LIMIT = 10 * 1024 * 1024
# Protect the endpoints from random bots.
# Note that that the wasm_compiler.py greps for this string to get the URL of the server.
# Changing the name could break the compiler.
_AUTH_TOKEN = "oBOT5jbsO4ztgrpNsQwlmFLIKB"

_SOURCE_EXTENSIONS = ['.cpp', '.hpp', '.h', '.ino']

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

@dataclass
class ProjectFiles:
    """A class to represent the project files."""
    src_files: list[Path]
    other_files: list[Path]


def collect_files(directory: Path) -> ProjectFiles:
    """Collect files from a directory and separate them into source and other files.

    Args:
        directory (Path): The directory to scan for files.

    Returns:
        ProjectFiles: Object containing lists of source and other files.
    """
    print(f"Collecting files from {directory}")
    src_files: list[Path] = []
    other_files: list[Path] = []

    def is_source_file(filename: str) -> bool:
        return any(filename.endswith(ext) for ext in _SOURCE_EXTENSIONS)
    
    for root, _, filenames in os.walk(str(directory)):
        for filename in filenames:
            print(f"Checking file: {filename}")
            file_path = Path(os.path.join(root, filename))

            if is_source_file(filename):
                src_files.append(file_path)
            else:
                other_files.append(file_path)
    
    return ProjectFiles(src_files=src_files, other_files=other_files)

def concatenate_files(file_list: List[Path], output_file: Path) -> None:
    """Concatenate files into a single output file.

    Args:
        file_list (List[str]): List of file paths to concatenate.
        output_file (str): Path to the output file.
    """
    with open(str(output_file), 'w', encoding='utf-8') as outfile:
        for file_path in file_list:
            outfile.write(f"// File: {file_path}\n")
            with open(file_path, 'r', encoding='utf-8') as infile:
                outfile.write(infile.read())
                outfile.write("\n\n")

# return a hash
def preprocess_with_gcc(input_file: Path, output_file: Path) -> None:
    """Preprocess a file with GCC, leaving #include directives intact.

    Args:
        input_file (str): Path to the input file.
        output_file (str): Path to the preprocessed output file.
    """
    # Convert paths to absolute paths
    # input_file = os.path.abspath(str(input_file))
    input_file = input_file.absolute()
    output_file = output_file.absolute()
    temp_input = str(input_file) + ".tmp"
    
    try:
        # Create modified version of input that comments out includes
        with open(str(input_file), 'r') as fin, open(str(temp_input), 'w') as fout:
            for line in fin:
                if line.strip().startswith('#include'):
                    fout.write(f"// PRESERVED: {line}")
                else:
                    fout.write(line)

        # Run GCC preprocessor with explicit output path in order to remove
        # comments. This is necessary to ensure that the hash
        # of the preprocessed file is consistent without respect to formatting
        # and whitespace.
        gcc_command: list[str] = [
            "gcc",
            "-E",  # Preprocess only
            "-P",  # No line markers
            "-fdirectives-only",
            "-fpreprocessed",  # Handle preprocessed input
            "-x", "c++",  # Explicitly treat input as C++ source
            "-o", str(output_file),  # Explicit output file
            temp_input
        ]
        
        result = subprocess.run(gcc_command, 
                              check=True,
                              capture_output=True,
                              text=True)
        
        if not os.path.exists(output_file):
            raise FileNotFoundError(f"GCC failed to create output file. stderr: {result.stderr}")

        # Restore include lines
        with open(output_file, 'r') as f:
            content = f.read()
        
        content = content.replace('// PRESERVED: #include', '#include')
        out_lines: list[str] = []
        # now preform minification to further strip out horizontal whitespace and // File: comments.
        for line in content.split("\n"):
            # Skip file marker comments and empty lines
            line = line.strip()
            if not line:  # skip empty line
                continue
            if line.startswith("// File:"):  # these change because of the temp file, so need to be removed.
                continue
            # Collapse multiple spaces into single space and strip whitespace
            line = ' '.join(line.split())
            out_lines.append(line)
        # Join with new lines
        content = '\n'.join(out_lines)
        with open(output_file, 'w') as f:
            f.write(content)
            
        print(f"Preprocessed file saved to {output_file}")
        
    except subprocess.CalledProcessError as e:
        print(f"GCC preprocessing failed: {e.stderr}")
        raise
    except Exception as e:
        print(f"Preprocessing error: {str(e)}")
        raise
    finally:
        # Clean up temporary file
        try:
            if os.path.exists(temp_input):
                os.remove(temp_input)
        except:  # noqa: E722
            warnings.warn(f"Failed to remove temporary file: {temp_input}")
            pass



def hash_string(s: str) -> str:
    # sha 256

    return hashlib.sha256(s.encode()).hexdigest()


def generate_hash_of_src_files(src_files: list[Path]) -> str:
    """Generate a hash of all source files in a directory.

    Args:
        root_dir (Path): The root directory to hash.

    Returns:
        str: The hash of all src files in the directory.
    """
    try:
        with TemporaryDirectory() as temp_dir:
            temp_file = Path(temp_dir) / "concatenated_output.cpp"
            preprocessed_file = Path(temp_dir) / "preprocessed_output.cpp"
            concatenate_files(src_files, Path(temp_file))
            preprocess_with_gcc(temp_file, preprocessed_file)
            contents = preprocessed_file.read_text()

            # strip the last line in it:
            parts = contents.split("\n")
            out_lines: list[str] = []
            for line in parts:
                if "concatenated_output.cpp" not in line:
                    out_lines.append(line)

            contents = "\n".join(out_lines)
            return hash_string(contents)
    except Exception:
        import traceback
        stack_trae = traceback.format_exc()
        print(stack_trae)
        raise


def generate_hash_of_project_files(root_dir: Path) -> str:
    """Generate a hash of all files in a directory.

    Args:
        root_dir (Path): The root directory to hash.

    Returns:
        str: The hash of all files in the directory.
    """
    """Generate a hash of all source files in a directory.

    Args:
        root_dir (Path): The root directory to hash.

    Returns:
        str: The hash of all src files in the directory.
    """

    project_files = collect_files(root_dir)
    src_file_hash = generate_hash_of_src_files(project_files.src_files)
    other_files = project_files.other_files
    # for all other files, don't pre-process them, just hash them
    hash_object = hashlib.sha256()
    for file in other_files:
        hash_object.update(file.read_bytes())
    other_files_hash = hash_object.hexdigest()
    return hash_string(src_file_hash + other_files_hash)



def compile_source(temp_src_dir: Path, file_path: Path, background_tasks: BackgroundTasks, build_mode: str, profile: bool, hash_value: str | None = None) -> FileResponse | HTTPException:
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
    
    print("Files are ready, waiting for compile lock...")
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
    hash_txt = fastled_js_dir / "hash.txt"
    print(f"\nSaving combined output to: {out_txt}")
    out_txt.write_text(stdout)
    perf_txt.write_text(f"Compile lock time: {compile_lock_time:.2f}s\nCompile time: {compile_time:.2f}s")
    if hash_value is not None:
        hash_txt.write_text(hash_value)

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
    
    if file.size is None:
        raise HTTPException(status_code=400, detail="No file size provided.")

    if not file.filename.endswith('.zip'):
        raise HTTPException(status_code=400, detail="Uploaded file must be a zip archive.")

    if file.size > _UPLOAD_LIMIT:
        raise HTTPException(
            status_code=400,
            detail=f"File size exceeds {_UPLOAD_LIMIT} byte limit."
        )

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
        hash_value: str | None = None
        with zipfile.ZipFile(file_path, 'r') as zip_ref:
            zip_ref.extractall(temp_src_dir)
            try:
                hash_value = generate_hash_of_project_files(Path(temp_src_dir))
            except Exception as e:
                warnings.warn(f"Error generating hash: {e}, fast cache access is disabled for this build.")

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
                background=background_tasks
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
            hash_value)
        if isinstance(out, HTTPException):
            print("Raising HTTPException")
            raise out
        # Cache the compiled zip file
        out_path = Path(out.path)
        data = out_path.read_bytes()
        if hash_value is not None:
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

