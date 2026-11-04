# Mobile3D Scan Server

Máy chủ tái tạo 3D từ ảnh cho app Android: nhận phiên ảnh qua HTTP, chạy
photogrammetry thật (COLMAP qua `pycolmap` → Open3D Poisson → GLB qua
`trimesh`), trả file `result.glb` về điện thoại.

## Pipeline và giới hạn thật
Ảnh → hạ độ phân giải (tối đa 1600px) → SIFT + matching + SfM (COLMAP) →
point cloud thưa → ước lượng normal hướng về camera → Poisson depth 8 →
cắt 5% bề mặt kém tin cậy nhất → decimation tối đa ~120k mặt → GLB.

Giới hạn cần biết (không che giấu): không có bước dense MVS nên bề mặt là
xấp xỉ thô từ point cloud thưa — hợp với vật thể nhiều texture, chụp nhiều
góc chồng lấp lớn. Ít hơn 8 ảnh đăng ký được sẽ báo lỗi rõ ràng. Muốn chất
lượng cao hơn cần bổ sung dense MVS (OpenMVS/COLMAP CUDA) — ngoài phạm vi
bản này.

## Chạy trên Linux (máy phát triển hiện tại)
```bash
cd server
~/.local/bin/python3.12 -m venv .venv   # cần Python 3.12
.venv/bin/pip install -r requirements.txt
./run.sh                                 # http://0.0.0.0:8000
curl http://127.0.0.1:8000/health
```

## Chạy trên Windows
Yêu cầu Python 3.12 (64-bit) từ python.org, có trong PATH.
```powershell
cd server
powershell -ExecutionPolicy Bypass -File setup_windows.ps1
.\run_windows.bat
```
`pycolmap` và `open3d` đều có wheel Windows chính thức. Bản Windows **chưa
được kiểm chứng trên máy Windows thật** (môi trường phát triển hiện tại là
Linux) — nếu cài lỗi, gửi lại log pip để xử lý.

## API
| Method | Path | Mô tả |
|---|---|---|
| GET | `/health` | kiểm tra sống |
| POST | `/sessions` | tạo phiên → `{session_id}` |
| POST | `/sessions/{id}/images` | upload 1 ảnh (multipart, idempotent theo tên file — hỗ trợ tải tiếp) |
| GET | `/sessions/{id}` | trạng thái: collecting/running/done/error + stage/progress |
| POST | `/sessions/{id}/reconstruct` | bắt đầu job nền |
| GET | `/sessions/{id}/result` | tải `result.glb` (409 nếu chưa xong) |
| DELETE | `/sessions/{id}` | xoá phiên |

Tuỳ chọn bảo vệ tối thiểu cho LAN tin cậy: đặt biến môi trường
`SCAN_API_KEY`, client phải gửi header `X-Api-Key`. Đây không phải auth
production — không phơi server ra internet khi chưa có lớp bảo mật thật.

Dữ liệu nằm trong `server/data/sessions/<id>/` (ghi đè bằng `SCAN_DATA_DIR`).
Cổng/host ghi đè bằng `SCAN_PORT`/`SCAN_HOST`.

## Trong app Android
Màn Scan: chụp ảnh vào `source/` → nhập địa chỉ server (emulator dùng
`http://10.0.2.2:8000`, máy thật dùng IP LAN của PC) → “Tạo mô hình 3D” →
app tải ảnh lên (bỏ qua ảnh đã có), chờ job, tải GLB về `meshes/scan_result.glb`
và tự thêm vào cảnh.
