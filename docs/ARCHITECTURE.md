# Architecture Proposal (Discovery v0.1)


> Tên dự án: chưa quyết định. "Mobile3D" trong tài liệu này là tên trung tính tạm thời, không phải branding. Thư viện native: `libmobile3d.so`, package: `com.mobile3d.*`.
Status: `Proposed` — chưa triển khai, chờ xác nhận. Nguyên tắc: tối giản cho 1 dev + AI, không abstraction sớm.

## 1. Tổng quan 3 lớp
```
Jetpack Compose UI (Kotlin)  — màn hình, navigation, permission, project list
        │  JNI (ít, thô, handle-based)
Native engine libmobile3d.so (C++17) — mesh, scene, renderer GLES3, geometry ops
        │
Files: project dir (manifest.json + meshes/*.bin + textures/) qua SAF/filesDir
```
Cloud (FastAPI) chỉ xuất hiện từ M3, là tiến trình tách biệt, app chạy 100% offline khi không có cloud.

## 2. Module đề xuất (chỉ tạo khi tới milestone tương ứng)
| Module | Tới lúc | Trách nhiệm |
|---|---|---|
| `:app` (Kotlin/Compose) | M0 | Activity, `ViewportView` (SurfaceView), navigation, DI thủ công (không Hilt ở M0–M1) |
| `cpp/core` | M0 | Mesh (SoA: position/normal/uv/index), Scene (vector<Node>, không cây con trỏ phức tạp), math tự viết mỏng hoặc glm (MIT) |
| `cpp/render` | M0 | Renderer GLES 3.0, shader PBR-lite, incremental buffer update (`glBufferSubData` theo dirty range) |
| `cpp/io` | M0 | GLB/OBJ import/export |
| `cpp/geom` | M1/M2 | transform, raycast (BVH tự viết khi sculpt cần), decimate wrapper |
| `cpp/bridge` | M0 | JNI: `nativeCreate/Destroy/LoadMesh/Render/SetTransform/ApplyBrush...` |

Không tách Gradle multi-module native ở M0: một `libmobile3d.so` duy nhất, chia thư mục trong CMake. Tách module chỉ khi thời gian build đau thật (>60s incremental — đo ở M0).

## 3. JNI contract
- Opaque handle `jlong` trỏ `Engine*`. Kotlin không giữ mesh data.
- Dữ liệu lớn đi qua đường file (đường dẫn) hoặc `DirectByteBuffer` một lần lúc import; cấm copy mảng vertex qua JNI mỗi frame.
- Mọi call nặng (import, decimate, sculpt stroke commit) chạy trên background executor Kotlin → native worker thread; render thread chỉ đọc snapshot/dirty flags.
- Lỗi native trả về mã + chuỗi qua callback, cấm exception xuyên JNI.

## 4. Threading
Main (UI) / Render (GL context, sở hữu GPU resource) / Worker (1–2 thread: IO + geometry). Mesh CPU là nguồn sự thật; GPU buffer là cache đồng bộ theo dirty range. Sculpt brush áp lên CPU mesh trên worker, đánh dấu range bẩn, render thread upload phần bẩn — đây là rủi ro hiệu năng số 1, phải chứng minh ở M0 bằng spike (xem VALIDATION_PLAN D5–D7).

## 5. Project format `.forma` (thư mục zip hoặc folder)
```
project.m3d/  manifest.json (schemaVersion, appVersion, createdWith)
  scene.json    (nodes, transforms, material refs)
  meshes/<id>.bin  (binary SoA + header version)
  textures/<id>.png|ktx2
  source/       (ảnh capture gốc, optional)
  checkpoints/  (undo snapshot nén, giới hạn số lượng)
```
Lý do không dùng GLB làm project format: GLB không lưu undo/source/history/material editing state. GLB chỉ là định dạng *export*. Migration: `schemaVersion` + hàm migrate theo từng version, test bằng fixture file cũ.

## 6. Rendering: GLES 3.0 trước, Vulkan sau (nếu bao giờ)
GLES 3.0 có mặt trên ~mọi thiết bị Android 10+ mục tiêu; ARCore certified devices đều ≥ GLES 3.0 (đa số 3.2 — nguồn: developers.google.com/ar/devices, truy cập 2026-11-01). Vulkan không cho lợi ích đo được với renderer PBR-lite 1–2M tris, nhưng tốn gấp 2–3 lần code boilerplate và driver variance Adreno/Mali lớn. Thiết kế `render/` gói GL call sau interface `Renderer` nội bộ C++ — chỉ là ranh giới thư mục, không xây abstract factory đa backend ở M0.

## 7. Backend (M3+, tối giản)
FastAPI + Postgres (hoặc SQLite nếu 1 instance) + S3-compatible storage + 1 worker queue (Redis+RQ hoặc DB polling — quyết ở M3). Resumable upload qua tus hoặc multipart S3. Không microservice, không K8s.

## 8. Những gì tự xây vs dùng thư viện
Tự xây: mesh container, scene, renderer, brush, BVH, undo (command + snapshot lai). Dùng thư viện: glTF IO, decimate/simplify, image decode, JSON. Chi tiết: DEPENDENCY_AUDIT.md.
