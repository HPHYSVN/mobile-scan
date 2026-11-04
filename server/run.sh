#!/usr/bin/env bash
# Start the scan server on Linux/macOS.
set -euo pipefail
cd "$(dirname "$0")"
HOST="${SCAN_HOST:-0.0.0.0}"
PORT="${SCAN_PORT:-8000}"
exec .venv/bin/uvicorn app.main:app --host "$HOST" --port "$PORT"
