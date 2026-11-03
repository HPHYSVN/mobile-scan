# Third-Party Licenses

> Tên dự án: chưa quyết định. "Mobile3D" là tên trung tính tạm thời, không phải branding.

| Thư viện | Phiên bản | License | Cách dùng | Vị trí |
|---|---|---|---|---|
| tinygltf (gồm nlohmann/json, stb_image đi kèm) | v2.9.3 | MIT (json: MIT, stb_image: public domain/MIT dual) | Vendor header-only; chỉ parse glTF geometry, stb image bị tắt bằng `TINYGLTF_NO_STB_IMAGE` | `app/src/main/cpp/third_party/tinygltf/` (kèm file LICENSE gốc) |

`core/project.cpp` dùng nlohmann/json từ cùng bản vendor trên (không thêm file mới).

| meshoptimizer (subset: simplifier) | commit f0a684f | MIT | Vendor `app/src/main/cpp/third_party/meshoptimizer/` (kèm LICENSE + VERSION.txt); chỉ biên dịch `simplifier.cpp` |
| AndroidX (Activity, CameraX) | activity 1.9.3 / camera 1.4.2 | Apache-2.0 | Maven (google()) |
| ARCore (`com.google.ar:core`) | 1.48.0 | Apache-2.0 + ARCore Terms of Service của Google | Maven (google()); chỉ kiểm tra availability |

Chưa có dependency nào khác. Mọi dependency mới phải có dòng ở đây **trước** khi merge vào build (xem DEPENDENCY_AUDIT.md).
