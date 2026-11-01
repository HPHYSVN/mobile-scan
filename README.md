# Mobile3D (tên tạm, chưa phải branding)

Ứng dụng Android tạo/chỉnh sửa 3D trên mobile. Hiện ở **M0 — Technical Validation, Ngày 1**: chỉ có viewport OpenGL ES 3.0 tối thiểu qua Kotlin → JNI → C++ (`libmobile3d.so`). Chưa có tính năng sản phẩm (không sculpt, capture, backend, AI).

Tài liệu: `docs/` — bắt đầu từ `docs/VALIDATION_PLAN.md`, `docs/ARCHITECTURE.md`, `docs/PERFORMANCE.md`.

## Build
Yêu cầu: JDK 17+, Android SDK (platform 35, build-tools 35/36), NDK 27.2.12479018, CMake 3.22.1 (xem `docs/ENVIRONMENT.md`).

```bash
./gradlew :app:assembleDebug        # APK: app/build/outputs/apk/debug/app-debug.apk
cmake -S cpp/tests -B /tmp/host-tests && cmake --build /tmp/host-tests && ctest --test-dir /tmp/host-tests
```

## Cấu trúc
- `app/` — Android app (Kotlin) + `app/src/main/cpp/` (core/render/bridge)
- `cpp/tests/` — host test cho code C++ độc lập nền tảng
- `docs/` — spec, kiến trúc, quyết định kỹ thuật, benchmark
