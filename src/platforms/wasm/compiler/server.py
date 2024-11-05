from fastapi import FastAPI, File, UploadFile
import aiohttp
import tempfile
from fastapi.responses import FileResponse, RedirectResponse
import shutil
from pathlib import Path

app = FastAPI()
upload_dir = Path("uploads")
upload_dir.mkdir(exist_ok=True)

@app.get("/", include_in_schema=False)
async def read_root() -> RedirectResponse:
    """Redirect to the /docs endpoint."""
    return RedirectResponse(url="/docs")

@app.get("/upload/")
async def upload_file(url: str) -> dict:
    """Download a zip file asynchronously into a temporary directory."""
    try:
        async with aiohttp.ClientSession() as session:
            async with session.get(url) as response:
                if response.status == 200:
                    temp_dir = tempfile.TemporaryDirectory()
                    zip_path = Path(temp_dir.name) / "downloaded.zip"
                    with open(zip_path, "wb") as f:
                        f.write(await response.read())
                    return {"message": "File downloaded successfully", "path": str(zip_path)}
                return {"error": "Failed to download file"}
    except Exception as e:
        return {"error": str(e)}
        

@app.get("/download/{filename}")
async def download_file(filename: str) -> FileResponse | dict:
    file_path = upload_dir / filename
    if file_path.exists():
        return FileResponse(file_path)
    return {"error": "File not found"}
