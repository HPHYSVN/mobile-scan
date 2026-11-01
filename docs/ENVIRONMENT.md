# Environment Record

> Tên dự án: chưa quyết định. "Mobile3D" là tên trung tính tạm thời, không phải branding.

## Thay đổi môi trường ngày 2026-11-01 (đã được human duyệt, không dùng sudo)
Máy dev ban đầu **không có** `sdkmanager`, NDK, CMake của SDK. Đã thực hiện:
1. Tải Android command-line tools `commandlinetools-linux-11076708_latest.zip` từ `dl.google.com` vào `$HOME/Android/Sdk/cmdline-tools/latest/` (trước đó thư mục này không tồn tại).
2. `sdkmanager --licenses` (chấp nhận license SDK).
3. `sdkmanager "ndk;27.2.12479018" "cmake;3.22.1" "platforms;android-35"` — lần cài NDK đầu bị timeout ở 120s làm hỏng thư mục NDK, đã cài lại với timeout dài, exit 0, kiểm chứng qua `source.properties`.
4. Tải Gradle 8.9 vào `/tmp/opencode/gradle/` (ngoài repo, không phải package hệ thống) để sinh wrapper; repo dùng `./gradlew`.

Không cài package hệ thống nào, không sửa cấu hình ngoài `$HOME/Android/Sdk` và `/tmp/opencode`.

## Phiên bản đã chốt
| Thành phần | Phiên bản |
|---|---|
| JDK (host) | OpenJDK 21.0.13 (build target Java 17) |
| Android SDK platform | 35 (compile/target), máy cũng có 36, 37.0 |
| Build-tools | 35.0.0, 36.0.0 |
| NDK | 27.2.12479018 (r27c) |
| CMake (SDK) | 3.22.1 |
| Gradle / AGP / Kotlin | 8.9 / 8.7.3 / 2.0.21 |
| Emulator | AVD `Medium_Phone`, system image `android-37.2/google_apis_playstore_ps16k/x86_64`, GPU SwiftShader (phần mềm) |

## Thiết bị kiểm chứng
- Emulator ở trên: **chỉ** `Emulator Tested`.
- Thiết bị vật lý: chưa có. Mọi kết luận hiệu năng trên thiết bị thật: `Not verified`.
