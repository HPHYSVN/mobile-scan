# Changelog

> Tên dự án: chưa quyết định. "Mobile3D" là tên trung tính tạm thời, không phải branding.

## 2026-11-01 — M0 Validation, Ngày 1
- Technical Discovery được human duyệt với điều chỉnh: tên trung tính (`libmobile3d.so`, `com.mobile3d.*`), chính sách license chỉ MIT/Apache-2.0/BSD, benchmark tách rendering vs sculpt/edit, nhãn `Emulator Tested` tách khỏi `Verified on Physical Device`. Tài liệu discovery chuyển vào `docs/` và sửa theo các điều chỉnh.
- Cài toolchain qua sdkmanager (NDK 27.2.12479018, CMake 3.22.1, platform 35) — chi tiết `docs/ENVIRONMENT.md`. `git init` (chưa commit theo chỉ đạo).
- Scaffold Android tối thiểu: Kotlin Activity + `GLSurfaceView` → JNI → C++17 GLES 3.0 renderer vẽ cube quay, frame stats qua JNI. Thêm `-Wl,-z,max-page-size=16384` sau khi emulator 16K báo không tương thích.
- Host test cho `core/mesh` pass; APK build thành công; emulator smoke test pass (`Emulator Tested`). Chưa có benchmark thiết bị thật.

## 2026-11-01 — M0 Validation, Ngày 3–4
- Vendor tinygltf v2.9.3 (MIT, gồm `tiny_gltf.h`/`json.hpp`/`stb_image.h` + LICENSE) vào `app/src/main/cpp/third_party/tinygltf/`. Loader `io/gltf_loader` (geometry-only spike: bỏ qua material/texture, sinh flat normal khi thiếu).
- Fixture generator `scripts/generate_fixtures.py` + 4 sphere GLB (6k–393k tris); `bench_import` host + `test_loader` (CTest 2/2 pass).
- Android: load fixture qua JNI trên GL thread, orbit bằng drag/pinch. Sửa race load trước khi surface init.

## 2026-11-01 — M0 Validation, Ngày 5–7
- Thêm `core/deform` (adjacency, chọn region, displace theo base normal, recompute normals) + host test/bench. Renderer tích hợp deform spike với timer riêng cho deform/normals/upload (`glBufferSubData` theo dirty run), stats mở rộng qua JNI (9 giá trị). App tự bật deform trên sphere_98k.
- Kết quả emulator: upload là giai đoạn đắt nhất trên SwiftShader (~6 ms/4.1k verts); host cho thấy deform/normals tỉ lệ theo region. Go/No-Go renderer vẫn chờ thiết bị thật.

## 2026-11-01 — M0 Validation, Ngày 8–11
- D8: `io/gltf_exporter` (GLB qua tinygltf, validate 0 errors bằng Khronos glTF-Validator) + `io/obj_loader` tối thiểu (fan triangulation, negative index, tự tính normal). Host test round-trip pass.
- D9: `nativePing` đo JNI round-trip trong app (0.14 µs/call trên emulator x86).
- D10: `core/undo` — snapshot command theo vùng vertex (52 B/vert), UndoStack có cap và tổng bộ nhớ đo được; host test bit-exact.
- D11: kiểm lifecycle trên emulator: background/foreground x3, xoay màn hình, force-stop/relaunch — không crash.

## 2026-11-01 — M0 đóng sprint (D12–D14)
- `scripts/run_benchmarks.sh`: benchmark lặp lại (host + Android qua adb, tự gắn nhãn Emulator/Device), kết quả chạy đầu ở `docs/benchmark-results/2026-11-01-emulator.md` (import 98k median 146.2 ms trên emulator).
- Clean build kiểm chứng, không còn placeholder. Báo cáo sprint `docs/SPRINT_M0_REPORT.md`: M0 đạt chức năng/tích hợp, Go/No-Go renderer vẫn mở chờ thiết bị vật lý; đề xuất phạm vi M1 chờ duyệt.
