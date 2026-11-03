# Mobile3D Studio (tên tạm, chưa phải branding)

Ứng dụng Android tạo/chỉnh sửa/điêu khắc 3D trên mobile: Kotlin → JNI → C++17 (`libmobile3d.so`, OpenGL ES 3.0). Trạng thái: studio mini chạy được end-to-end trên emulator (toàn bộ là `Emulator Tested` — chưa benchmark/xác nhận trên thiết bị vật lý, xem `docs/SPRINT_M0_REPORT.md` và `docs/TESTING.md`).

## Tính năng hiện có
- **Editor**: scene nhiều vật thể, thêm Cube/Sphere/Cylinder/Plane, Import File (.glb/.obj qua SAF) hoặc fixture mẫu, chọn vật bằng chạm (raycast) hoặc Prev/Next, Duplicate, Delete.
- **Chế độ View/Edit/Sculpt** với gesture: View (1 ngón xoay, 2 ngón zoom/pan, 3 ngón zoom), Edit (kéo di chuyển, kéo góc handle để scale, pinch scale, xoay 2 ngón, nâng/hạ 3 ngón, chạm 2 lần nhân bản), Sculpt (brush Draw/Dig/Smooth/Inflate, chỉnh bán kính/lực, undo/redo theo stroke).
- **Material** màu cơ bản theo vật thể; transform PosX/RotY/Scale có Undo/Redo; autosave; project format thư mục (`manifest.json` + `scene.json` + `meshes/` + `source/`), ghi file nguyên tử.
- **Capture**: chụp ảnh hướng dẫn vào `source/` bằng CameraX; kiểm tra khả năng ARCore (chưa chạy AR session, chưa tái tạo 3D tự động trong bản offline này).
- **Export**: theo vật thể — GLB, OBJ triangle, OBJ polygon-quad — kèm độ phân giải (decimation bằng meshoptimizer) và Subdiv 0–2; Export Scene ra 1 GLB đã bake transform. File GLB qua kiểm chứng Khronos glTF-Validator (0 lỗi).
- **Hướng dẫn** trong app (mở lần đầu tự hiện, mở lại bằng nút Help).

## Build
Yêu cầu: JDK 17+, Android SDK (platform 35, build-tools 35/36), NDK 27.2.12479018, CMake 3.22.1 (xem `docs/ENVIRONMENT.md`).

```bash
./gradlew :app:assembleDebug        # APK debug
./gradlew :app:assembleRelease      # APK release (chưa ký)
cmake -S cpp/tests -B /tmp/host-tests && cmake --build /tmp/host-tests && ctest --test-dir /tmp/host-tests
scripts/run_benchmarks.sh           # benchmark lặp lại (host + adb device/emulator)
```

## Cấu trúc
- `app/` — Android app (Kotlin) + `app/src/main/cpp/` (core/render/io/bridge, thư viện vendor trong `third_party/`)
- `cpp/tests/` — host test cho code C++ độc lập nền tảng (8 test)
- `scripts/` — sinh fixture GLB, chạy benchmark
- `docs/` — spec, kiến trúc, quyết định kỹ thuật, audit dependency/license, benchmark, testing
