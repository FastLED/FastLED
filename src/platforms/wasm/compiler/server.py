from fastapi import FastAPI, File, UploadFile
import tempfile
from fastapi.responses import FileResponse, RedirectResponse
import threading
import shutil
from pathlib import Path

_UPLOAD_LIMIT = 10 * 1024 * 1024

app = FastAPI()
upload_dir = Path("uploads")
upload_dir.mkdir(exist_ok=True)
compile_lock = threading.Lock()

@app.get("/", include_in_schema=False)
async def read_root() -> RedirectResponse:
    """Redirect to the /docs endpoint."""
    return RedirectResponse(url="/docs")

@app.post("/upload/")
def upload_file(file: UploadFile = File(...)) -> dict:
    """Upload a file into a temporary directory."""

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
        
        return {"message": "File uploaded successfully", "path": str(file_path)}
    except Exception as e:
        return {"error": str(e)}
    finally:
        temp_dir.cleanup(ignore_errors=True)

