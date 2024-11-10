from fastapi import FastAPI, File, UploadFile, HTTPException  # type: ignore
import tempfile
from fastapi.responses import FileResponse, RedirectResponse  # type: ignore
import threading
import shutil
import zipfile
import warnings
from pathlib import Path
import subprocess

_UPLOAD_LIMIT = 10 * 1024 * 1024
MAPPED_DIR = Path("/mapped")
OUTPUT_PATH = MAPPED_DIR / "out.js"

# Protect the endpoints from random bots.
# Note that that the wasm_compiler.py greps for this string to get the URL of the server.
# Changing the name could break the compiler.
_AUTH_TOKEN = "oBOT5jbsO4ztgrpNsQwlmFLIKB"

app = FastAPI()
upload_dir = Path("uploads")
upload_dir.mkdir(exist_ok=True)
compile_lock = threading.Lock()

# Make sure the /mapped directory exists to match the expectations of the compile.py script.
# When that script is used, a volume will be passed in. However when the server script is used
# there will not be a volume mount passed in.
if not MAPPED_DIR.exists():
    MAPPED_DIR.mkdir(exist_ok=True)

@app.get("/", include_in_schema=False)
async def read_root() -> RedirectResponse:
    """Redirect to the /docs endpoint."""
    return RedirectResponse(url="/docs")

@app.post("/compile/")
def upload_file(auth_token: str = "", file: UploadFile = File(...)) -> FileResponse:
    """Upload a file into a temporary directory."""

    if auth_token != _AUTH_TOKEN:
        raise HTTPException(status_code=401, detail="Unauthorized")

    if not file.filename.endswith('.zip'):
        return {"error": "Only .zip files are allowed."}

    if file.size > _UPLOAD_LIMIT:
        return {"error": f"File size exceeds {_UPLOAD_LIMIT} byte limit."}

    temp_dir = tempfile.TemporaryDirectory()
    try:
        file_path = Path(temp_dir.name) / file.filename
        with open(file_path, "wb") as f:
            print(f"Saving file to {file_path}")
            shutil.copyfileobj(file.file, f)

        # Acquire the compile lock and decompress the file
        with compile_lock:
            with zipfile.ZipFile(file_path, 'r') as zip_ref:
                print(f"Extracting {file_path} to {MAPPED_DIR}")
                zip_ref.extractall(MAPPED_DIR)
            # now print out the contents of the /mapped directory
            print("Contents of /mapped:")
            for path in MAPPED_DIR.rglob("*"):
                print(path)

            # Just run the compiler and return the results
            cp: subprocess.CompletedProcess = subprocess.run(
                ["python", "run.py", "compile"], 
                cwd="/js",
                capture_output=True,
                text=True
            )
            
            print("Compiler stdout:", cp.stdout)
            print("Compiler stderr:", cp.stderr)
            
            return {
                "status": "completed",
                "return_code": cp.returncode,
                "stdout": cp.stdout,
                "stderr": cp.stderr
            }

    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))
    finally:
        try:
            # Clean up temporary files after sending the response
            for item in MAPPED_DIR.iterdir():
                if item.is_file():
                    item.unlink()
                elif item.is_dir():
                    shutil.rmtree(item)
            temp_dir.cleanup()
        except Exception as e:
            warnings.warn(f"Error cleaning up temporary directory: {e}")

