from fastapi import FastAPI, File, UploadFile
from fastapi.responses import FileResponse, RedirectResponse
import shutil
from pathlib import Path

app = FastAPI()
upload_dir = Path("uploads")
upload_dir.mkdir(exist_ok=True)

@app.get("/", include_in_schema=False)
async def read_root():
    """Redirect to the /docs endpoint."""
    return RedirectResponse(url="/docs")

@app.post("/upload/")
async def upload_file(file: UploadFile = File(...)):
    file_path = upload_dir / file.filename
    with file_path.open("wb") as buffer:
        shutil.copyfileobj(file.file, buffer)
    return {"filename": file.filename}

@app.get("/download/{filename}")
async def download_file(filename: str):
    file_path = upload_dir / filename
    if file_path.exists():
        return FileResponse(file_path)
    return {"error": "File not found"}
