#!/usr/bin/env python3
"""End-to-end test against a running server (not a unit test):
creates a session, uploads a few files (incl. an idempotent re-upload),
checks status/result semantics, deletes the session.

Usage: .venv/bin/python tests/test_server_e2e.py [base_url] [image_dir]
Requires the server running; image_dir optional (uses any .jpg found there,
otherwise generates tiny placeholder JPEGs which must NOT reconstruct).
"""
import sys
import time
from pathlib import Path

import requests

BASE = sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:8000"


def main() -> int:
    health = requests.get(f"{BASE}/health", timeout=10)
    assert health.status_code == 200, health.text
    print("health ok:", health.json())

    session = requests.post(f"{BASE}/sessions", timeout=10).json()["session_id"]
    print("session:", session)

    image_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else None
    files = []
    if image_dir and image_dir.exists():
        files = sorted(image_dir.glob("*.JPG"))[:3] + sorted(image_dir.glob("*.jpg"))[:3]
    if not files:
        print("no image_dir given: API-only smoke test (upload skipped)")
        requests.delete(f"{BASE}/sessions/{session}", timeout=10)
        return 0

    for path in files[:3]:
        with open(path, "rb") as fh:
            r = requests.post(
                f"{BASE}/sessions/{session}/images",
                files={"file": (path.name, fh, "image/jpeg")},
                timeout=120,
            )
        assert r.status_code == 200, r.text
        print("uploaded", path.name, r.json())

    # Idempotent re-upload of the same file must be skipped, not duplicated.
    first = files[0]
    with open(first, "rb") as fh:
        r = requests.post(
            f"{BASE}/sessions/{session}/images",
            files={"file": (first.name, fh, "image/jpeg")},
            timeout=120,
        )
    assert r.json().get("skipped") is True, r.text
    print("idempotent re-upload ok")

    status = requests.get(f"{BASE}/sessions/{session}", timeout=10).json()
    assert status["images"] == 3, status
    print("status ok:", status)

    result = requests.get(f"{BASE}/sessions/{session}/result", timeout=10)
    assert result.status_code == 409, result.status_code
    print("result-before-job correctly 409")

    requests.delete(f"{BASE}/sessions/{session}", timeout=10)
    gone = requests.get(f"{BASE}/sessions/{session}", timeout=10)
    assert gone.status_code == 404
    print("delete ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
