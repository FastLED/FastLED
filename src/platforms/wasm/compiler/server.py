from fastapi import FastAPI, File, UploadFile
import tempfile
from fastapi.responses import FileResponse, RedirectResponse
import asyncio
import shutil
from pathlib import Path

_UPLOAD_LIMIT = 10 * 1024 * 1024

app = FastAPI()
upload_dir = Path("uploads")
upload_dir.mkdir(exist_ok=True)
upload_lock = asyncio.Lock()

@app.get("/", include_in_schema=False)
async def read_root() -> RedirectResponse:
    """Redirect to the /docs endpoint."""
    return RedirectResponse(url="/docs")

@app.post("/upload/")
async def upload_file(file: UploadFile = File(...)) -> dict:
    """Upload a file asynchronously into a temporary directory."""

    if file.size > _UPLOAD_LIMIT:
        return {"error": "File size exceeds 10 MB limit"}

    async with upload_lock:
        try:
            temp_dir = tempfile.TemporaryDirectory()
            file_path = Path(temp_dir.name) / file.filename
            with open(file_path, "wb") as f:
                shutil.copyfileobj(file.file, f)
            return {"message": "File uploaded successfully", "path": str(file_path)}
        except Exception as e:
            return {"error": str(e)}

