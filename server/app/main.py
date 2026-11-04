"""Scan server: receives photo sessions from the Android app, runs
photogrammetry (see reconstruction.py) in a background thread, and serves
the resulting GLB back.

Local-development server by design: no accounts, data under ./data.
If SCAN_API_KEY is set, every request must send header X-Api-Key with it.
Do not expose this server to the internet without a real auth layer.
"""

from __future__ import annotations

import json
import os
import threading
import time
import uuid
from pathlib import Path

from fastapi import FastAPI, File, Header, HTTPException, UploadFile
from fastapi.responses import FileResponse, JSONResponse

from .reconstruction import reconstruct

DATA_DIR = Path(os.environ.get("SCAN_DATA_DIR", Path(__file__).parent.parent / "data"))
SESSIONS_DIR = DATA_DIR / "sessions"
SESSIONS_DIR.mkdir(parents=True, exist_ok=True)
API_KEY = os.environ.get("SCAN_API_KEY", "")

app = FastAPI(title="Mobile3D Scan Server", version="0.1.0")
_jobs_lock = threading.Lock()


def require_key(x_api_key: str | None) -> None:
    if API_KEY and x_api_key != API_KEY:
        raise HTTPException(401, "missing or wrong X-Api-Key")


def session_dir(session_id: str) -> Path:
    path = (SESSIONS_DIR / session_id).resolve()
    if path.parent != SESSIONS_DIR.resolve():
        raise HTTPException(400, "bad session id")
    return path


def load_status(path: Path) -> dict:
    status_file = path / "status.json"
    if status_file.exists():
        try:
            return json.loads(status_file.read_text())
        except json.JSONDecodeError:
            pass
    return {"state": "unknown"}


def save_status(path: Path, status: dict) -> None:
    tmp = path / "status.json.tmp"
    tmp.write_text(json.dumps(status, indent=2))
    tmp.replace(path / "status.json")


def image_count(path: Path) -> int:
    images = path / "images"
    if not images.exists():
        return 0
    return sum(1 for p in images.iterdir() if p.suffix.lower() in {".jpg", ".jpeg", ".png"})


@app.get("/health")
def health():
    return {"ok": True, "sessions": len(list(SESSIONS_DIR.iterdir()))}


@app.post("/sessions")
def create_session(x_api_key: str | None = Header(default=None)):
    require_key(x_api_key)
    session_id = uuid.uuid4().hex[:16]
    path = session_dir(session_id)
    (path / "images").mkdir(parents=True, exist_ok=True)
    status = {"state": "collecting", "images": 0, "created": time.time()}
    save_status(path, status)
    return {"session_id": session_id, **status}


@app.get("/sessions/{session_id}")
def get_session(session_id: str, x_api_key: str | None = Header(default=None)):
    require_key(x_api_key)
    path = session_dir(session_id)
    if not path.exists():
        raise HTTPException(404, "session not found")
    status = load_status(path)
    status["images"] = image_count(path)
    status["has_result"] = (path / "result.glb").exists()
    return status


@app.post("/sessions/{session_id}/images")
async def upload_image(
    session_id: str,
    file: UploadFile = File(...),
    x_api_key: str | None = Header(default=None),
):
    """Idempotent per filename: re-uploading the same name resumes cleanly."""
    require_key(x_api_key)
    path = session_dir(session_id)
    if not path.exists():
        raise HTTPException(404, "session not found")
    name = Path(file.filename or "image.jpg").name
    if Path(name).suffix.lower() not in {".jpg", ".jpeg", ".png"}:
        raise HTTPException(400, "only .jpg/.png images")
    images = path / "images"
    images.mkdir(exist_ok=True)
    dest = images / name
    content = await file.read()
    if dest.exists() and dest.stat().st_size == len(content):
        return {"stored": name, "skipped": True, "images": image_count(path)}
    tmp = dest.with_suffix(dest.suffix + ".part")
    tmp.write_bytes(content)
    tmp.replace(dest)
    return {"stored": name, "skipped": False, "images": image_count(path)}


def _run_job(session_id: str) -> None:
    path = session_dir(session_id)
    status = load_status(path)
    status.update({"state": "running", "stage": "prepare", "progress": 0.0, "error": None})
    save_status(path, status)

    def progress(stage: str, frac: float) -> None:
        current = load_status(path)
        current.update({"state": "running", "stage": stage, "progress": round(frac, 3)})
        save_status(path, current)

    try:
        stats = reconstruct(path, progress)
        status = load_status(path)
        status.update({"state": "done", "stage": "done", "progress": 1.0, "result": stats})
        save_status(path, status)
    except Exception as exc:  # surfaced to the client, never hidden
        status = load_status(path)
        status.update({"state": "error", "stage": "error", "error": str(exc)})
        save_status(path, status)


@app.get("/sessions/{session_id}/files")
def list_files(session_id: str, x_api_key: str | None = Header(default=None)):
    """Lists stored images with sizes so clients can resume uploads."""
    require_key(x_api_key)
    path = session_dir(session_id)
    if not path.exists():
        raise HTTPException(404, "session not found")
    images = path / "images"
    files = []
    if images.exists():
        for item in sorted(images.iterdir()):
            if item.suffix.lower() in {".jpg", ".jpeg", ".png"}:
                files.append({"name": item.name, "bytes": item.stat().st_size})
    return {"files": files}


@app.post("/sessions/{session_id}/reconstruct")
def start_reconstruction(session_id: str, x_api_key: str | None = Header(default=None)):
    require_key(x_api_key)
    path = session_dir(session_id)
    if not path.exists():
        raise HTTPException(404, "session not found")
    with _jobs_lock:
        status = load_status(path)
        if status.get("state") == "running":
            return {"started": False, "reason": "already running", **status}
        thread = threading.Thread(target=_run_job, args=(session_id,), daemon=True)
        thread.start()
    return {"started": True, "session_id": session_id}


@app.get("/sessions/{session_id}/result")
def download_result(session_id: str, x_api_key: str | None = Header(default=None)):
    require_key(x_api_key)
    path = session_dir(session_id)
    result = path / "result.glb"
    if not result.exists():
        status = load_status(path)
        raise HTTPException(409, f"no result yet (state={status.get('state')})")
    return FileResponse(result, media_type="model/gltf-binary", filename="scan_result.glb")


@app.delete("/sessions/{session_id}")
def delete_session(session_id: str, x_api_key: str | None = Header(default=None)):
    require_key(x_api_key)
    path = session_dir(session_id)
    if not path.exists():
        raise HTTPException(404, "session not found")
    import shutil

    shutil.rmtree(path)
    return JSONResponse({"deleted": session_id})
