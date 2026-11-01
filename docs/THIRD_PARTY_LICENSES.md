# Third-Party Licenses

> Tên dự án: chưa quyết định. "Mobile3D" là tên trung tính tạm thời, không phải branding.

| Thư viện | Phiên bản | License | Cách dùng | Vị trí |
|---|---|---|---|---|
| tinygltf (gồm nlohmann/json, stb_image đi kèm) | v2.9.3 | MIT (json: MIT, stb_image: public domain/MIT dual) | Vendor header-only; chỉ parse glTF geometry, stb image bị tắt bằng `TINYGLTF_NO_STB_IMAGE` | `app/src/main/cpp/third_party/tinygltf/` (kèm file LICENSE gốc) |

`core/project.cpp` dùng nlohmann/json từ cùng bản vendor trên (không thêm file mới).

Chưa có dependency nào khác. Mọi dependency mới phải có dòng ở đây **trước** khi merge vào build (xem DEPENDENCY_AUDIT.md).
