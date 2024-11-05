from fastapi import FastAPI, File, UploadFile, HTTPException  # type: ignore
import tempfile
from fastapi.responses import FileResponse, RedirectResponse  # type: ignore
import threading
import shutil
import warnings
from pathlib import Path

_UPLOAD_LIMIT = 10 * 1024 * 1024

# Protect the endpoints from random bots.
_AUTH_TOKEN = "oBOT5jbsO4ztgrpNsQwlmFLIKB"

app = FastAPI()
upload_dir = Path("uploads")
upload_dir.mkdir(exist_ok=True)
compile_lock = threading.Lock()

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
            shutil.copyfileobj(file.file, f)
        
        with compile_lock:
            print("Compile would happen here")
        
        return FileResponse(path=str(file_path), filename=file.filename)
    except Exception as e:
        return {"error": str(e)}
    finally:
        try:
            temp_dir.cleanup()
        except Exception as e:
            warnings.warn(f"Error cleaning up temporary directory: {e}")

