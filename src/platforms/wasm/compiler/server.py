from fastapi import FastAPI, File, UploadFile, HTTPException, Header  # type: ignore
import tempfile
from fastapi.responses import FileResponse, RedirectResponse  # type: ignore
import threading
import shutil
import zipfile
from pathlib import Path
import subprocess
import time

def cleanup_file(file_path: Path) -> None:
    """Clean up a file after it has been sent."""
    try:
        if file_path.exists():
            file_path.unlink()
    except Exception as e:
        print(f"Error cleaning up file {file_path}: {e}")

_TEST = False

_PURGE_TIME = 60 * 60 * 1  # 1 hour

_UPLOAD_LIMIT = 10 * 1024 * 1024
# Protect the endpoints from random bots.
# Note that that the wasm_compiler.py greps for this string to get the URL of the server.
# Changing the name could break the compiler.
_AUTH_TOKEN = "oBOT5jbsO4ztgrpNsQwlmFLIKB"

app = FastAPI()
upload_dir = Path("uploads")
upload_dir.mkdir(exist_ok=True)
compile_lock = threading.Lock()

output_dir = Path("output")
output_dir.mkdir(exist_ok=True)

@app.get("/", include_in_schema=False)
async def read_root() -> RedirectResponse:
    """Redirect to the /docs endpoint."""
    return RedirectResponse(url="/docs")

def compile_source(temp_src_dir: Path, file_path: Path) -> FileResponse | HTTPException:
    """Compile source code and return compiled artifacts as a zip file."""
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
        proc = subprocess.Popen(
            ["python", "run.py", "compile", f"--mapped-dir={temp_src_dir}"],
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

    output_zip_path = output_dir / f"fastled_output_{hash(str(file_path))}.zip"
    print(f"\nCreating output zip at: {output_zip_path}")
    
    with zipfile.ZipFile(output_zip_path, 'w', zipfile.ZIP_DEFLATED) as zip_out:
        print("\nAdding files to output zip:")
        for file_path in fastled_js_dir.rglob("*"):
            if file_path.is_file():
                arc_path = file_path.relative_to(fastled_js_dir)
                print(f"  Adding: {arc_path}")
                zip_out.write(file_path, arc_path)

    response = FileResponse(
        path=output_zip_path,
        media_type="application/zip",
        filename="fastled_output.zip"
    )
    
    # Schedule cleanup after response is sent
    threading.Timer(_PURGE_TIME, cleanup_file, args=[output_zip_path]).start()
    
    return response

@app.post("/compile/wasm")
def compile_wasm(
    file: UploadFile = File(...),
    authorization: str = Header(None)
) -> FileResponse:
    """Upload a file into a temporary directory."""
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
        
        print("\nContents of source directory:")
        for path in Path(temp_src_dir).rglob("*"):
            print(f"  {path}")

        out = compile_source(Path(temp_src_dir), file_path)
        if isinstance(out, HTTPException):
            print("Raising HTTPException")
            raise out
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

