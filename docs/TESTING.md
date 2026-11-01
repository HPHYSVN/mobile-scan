# Testing

> Tên dự án: chưa quyết định. "Mobile3D" là tên trung tính tạm thời, không phải branding.

## Hiện có (2026-11-01)
| Lớp | Lệnh | Kết quả |
|---|---|---|
| Host C++ test (`cpp/tests/`, assert-based, không thêm framework) | `cmake -S cpp/tests -B /tmp/host-tests && cmake --build /tmp/host-tests && ctest --test-dir /tmp/host-tests` | 2/2 pass — cube 24 verts/12 tris + glTF loader (đúng 3,185 verts/6,144 tris, file thiếu bị từ chối có error) |
| Android build | `./gradlew :app:assembleDebug` | BUILD SUCCESSFUL, APK chứa `lib/arm64-v8a/libmobile3d.so` + `lib/x86_64/libmobile3d.so` |
| Emulator smoke test | Cài APK, `am start`, xem logcat `Mobile3D`, screenshot, restart app lần 2 | GL init `OpenGL ES 3.0 SwiftShader`, `renderer init ok: verts=24 tris=12`, không crash, cube hiển thị đúng (screenshot kiểm chứng trực quan). Nhãn: `Emulator Tested` |
| 16 KB page size | `llvm-readelf -l libmobile3d.so` | LOAD align `0x4000`, emulator 16K không còn cảnh báo tương thích |

## Chưa có
Unit test Kotlin, instrumented test, benchmark tự động (đang thiết kế ở BENCHMARK.md), test trên thiết bị vật lý (`Not verified`). Không có test nào bị bỏ qua/che giấu: các mục trên là toàn bộ phạm vi Ngày 1.

## 2026-11-01 — Ngày 3–4
- `bench_import` host: chạy đủ 4 fixture x 3 runs (số ở PERFORMANCE.md).
- Emulator: app load `sphere_24k.glb` từ assets 3 lần (median 123.2 ms), render sphere đúng (screenshot), swipe orbit không crash, restart OK. Phát hiện và sửa race load-trước-init (mesh bị cube ghi đè). Nhãn: `Emulator Tested`.

## 2026-11-01 — Ngày 5–7
- Host: CTest 3/3 pass (thêm `test_deform`: region +Z của cube đúng 4 verts, displace đúng 0.25, normal sau recompute vẫn đơn vị và hướng +Z, mặt không chọn không đổi). `bench_deform` 98k/393k x 60 iterations (số ở PERFORMANCE.md).
- Emulator: sphere_98k + deform bật tự động, 2 screenshot cách nhau 1s cho thấy vùng deform thay đổi hình dạng, không crash, stats 3 giai đoạn hiển thị đúng. Nhãn: `Emulator Tested`.

## 2026-11-01 — Ngày 8–11
- Host: CTest **5/5 pass** — thêm `test_io` (GLB export→load round-trip khớp từng float; OBJ quad fan + negative index + normals; OBJ không normal tự tính) và `test_undo` (undo khôi phục bit-exact, redo áp lại, cap 3/5 commands, 52 bytes/vert).
- glTF-Validator chính thức của Khronos (npm `gltf-validator`, cài ở `/tmp` — không phải dependency repo): `roundtrip.glb` do exporter sinh ra **0 errors / 0 warnings**; fixture `sphere_24k.glb` cũng 0/0.
- Emulator D9: `jni ping: 10000 calls (0.14 us/call)`. D11: 3 chu kỳ Home→mở lại (pid không đổi, không crash), xoay ngang (screenshot: sphere render đúng tỉ lệ mới), force-stop→mở lại (load lại mesh + init OK, PSS 48,927 KB). Nhãn: `Emulator Tested`.
