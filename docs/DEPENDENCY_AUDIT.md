# Dependency Audit (Discovery v0.1, 2026-11-01)


> Tên dự án: chưa quyết định. "Mobile3D" trong tài liệu này là tên trung tính tạm thời, không phải branding. Thư viện native: `libmobile3d.so`, package: `com.mobile3d.*`.
Trạng thái verify: `LICENSE verified` = đã fetch file LICENSE gốc trong phiên này. `Not verified` = chưa kiểm chứng trực tiếp, phải kiểm tra trước khi thêm vào build. Web search bị lỗi HTTP 402 trong phiên này nên một số mục dựa trên tri thức đến 2026-05, đã đánh dấu rõ.

| Dependency | Dùng cho | License | Android | Rủi ro | Trạng thái |
|---|---|---|---|---|---|
| AndroidX Compose/CameraX | UI, capture (M3) | Apache-2.0 | Chính thức | Thấp; CameraX vendor extension khác nhau theo OEM | Not verified (chưa fetch) |
| ARCore SDK | Preview/scale (M3, optional) | Apache-2.0 + ARCore ToS của Google | Chỉ thiết bị certified; Depth API >88% active devices theo Google (05/2026, đã fetch trang devices) | **Trung bình**: không phủ 100% thiết bị tầm trung cũ; bắt buộc graceful fallback | Trang thiết bị đã fetch |
| fastgltf | GLB import/export | MIT | C++17, header+cpp nhỏ | Trẻ hơn tinygltf; cần spike hiệu năng | LICENSE verified |
| tinygltf v2.9.3 | **Đang dùng** (M0 spike, geometry-only) | MIT | Header-only, đã build arm64+x86_64 | Parse đơn luồng; fastgltf vẫn là ứng viên thay thế nếu benchmark thiết bị thật kém | LICENSE verified + đã vendor kèm LICENSE vào `app/src/main/cpp/third_party/tinygltf/` |
| meshoptimizer | Simplify, LOD, optimize index | MIT | CMake sẵn, không phụ thuộc | Thấp — phù hợp nhất trong danh sách | LICENSE verified |
| glm | Math (nếu không tự viết) | MIT | Header-only | Thấp | Not verified |
| nlohmann/json | Manifest phía C++ (nếu cần) | MIT | Header-only | Thấp; ưu tiên parse ở Kotlin để tránh | Not verified |
| stb_image | Texture decode phía C++ (nếu cần) | Public domain/MIT | Header-only | Giải mã chậm với ảnh lớn; cân nhắc dùng Android BitmapFactory thay thế | Not verified |
| libigl | Geometry nâng cao | **MPL-2.0** | Build nặng (Eigen) | **Trung bình–cao**: file-level copyleft MPL, một số module dính CGAL/GPL. Không dùng ở core (TD-06) | LICENSE verified |
| CGAL | Boolean/remesh | **GPL/LGPL (đa số)** | Nặng, cần GMP/MPFR | **Cao — loại trừ** cho app closed-source | Not verified (tri thức chung, cần verify nếu ai đề xuất lại) |
| OpenCV 4.x | Đánh giá ảnh (M3) | Apache-2.0 | SDK Android chính thức | +10–20MB APK; chọn qua Maven/AAR ở M3 | Not verified |
| ONNX Runtime | AI inference (M5) | MIT (core) | Có Android AAR | Lớn; hoãn M5 | Not verified |
| Filament | Renderer thay thế | Apache-2.0 | Chính thức của Google | Đã bác bỏ ở TD-11, giữ làm phương án dự phòng | Not verified |

## Quy tắc thêm dependency (áp dụng từ M0)
1. Phải trả lời: tự viết tốn bao lâu, thư viện tiết kiệm gì, license gì, lần commit gần nhất, có Android CI không.
2. Ưu tiên: MIT/BSD/Apache-2.0 > MPL-2.0 (chỉ khi tách file rõ) > **cấm GPL/LGPL** nếu app closed-source (quyết định mô hình phân phối thuộc human developer — xem câu hỏi Q4 cuối báo cáo).
3. Mỗi dependency native phải qua spike build arm64-v8a trên máy hiện tại trước khi commit vào CMake chính.
4. Pin version cụ thể (không dùng `latest`), ghi vào THIRD_PARTY_LICENSES.md khi tạo project.

## Rủi ro license tổng thể
Chưa xác định Mobile3D là closed-source thương mại hay open-source — điều này thay đổi hoàn toàn mức chấp nhận GPL/MPL. **Chặn (blocking)**: cần human quyết định trước M2 (khi geometry library được chọn). Trước mắt toàn bộ lựa chọn ở M0 đều MIT/Apache nên không bị chặn ở sprint đầu.

## Điều chỉnh đã duyệt (2026-11-01)
Mô hình phân phối chưa quyết định (open/closed). Chính sách mặc định cho tới khi có quyết định khác: **chỉ MIT / Apache-2.0 / BSD**. Cấm GPL/AGPL và mọi license có điều kiện phân phối phức tạp khi chưa có chấp thuận bằng văn bản của human developer. MPL-2.0 chuyển từ "trung bình" thành **cần chấp thuận trước** (áp dụng cho cả quyết định TD-06/libigl).
