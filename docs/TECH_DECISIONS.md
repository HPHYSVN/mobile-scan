# Tech Decisions (ADR log, Discovery v0.1)


> Tên dự án: chưa quyết định. "Mobile3D" trong tài liệu này là tên trung tính tạm thời, không phải branding. Thư viện native: `libmobile3d.so`, package: `com.mobile3d.*`.
Định dạng: Quyết định / Lý do / Trade-off / Điều kiện xem xét lại.

## TD-01: Kotlin + Jetpack Compose cho UI — `Proposed`
Lý do: stack Android chính thống duy nhất hợp lý cho 1 dev; Compose giảm code view. Trade-off: viewport 3D không nằm trong Compose — dùng `AndroidView` bọc `SurfaceView`, Compose chỉ chứa chrome xung quanh. Xem lại nếu: cần UI trong AR overlay phức tạp (M5).

## TD-02: C++17 (không C++20) cho native core — `Proposed`
Lý do: NDK r26+ hỗ trợ cả hai, nhưng C++17 giảm rủi ro toolchain/libc++ với thư viện bên thứ ba (OpenCV, meshoptimizer đều ổn C++17). Tính năng C++20 cần (concepts/ranges) không thiết yếu. Xem lại: khi mọi dependency đã audit C++20.

## TD-03: OpenGL ES 3.0 trước, hoãn Vulkan vô thời hạn — `Proposed`
Lý do: phủ thiết bị tối đa, ít code nhất cho 1 dev, đủ cho ≤2M tris PBR-lite. ARCore cũng mặc định GLES. Trade-off: mất compute shader tiện lợi cho sculpt/remesh nặng (có thể dùng CPU SIMD thay thế ở quy mô này). Xem lại nếu: benchmark M0 cho thấy GPU-bound bởi GLES driver trên thiết bị tham chiếu, hoặc M5 cần splatting.

## TD-04: glTF IO — tinygltf (MIT) cho spike M0, fastgltf (MIT) chưa benchmark — `Emulator Tested`
Cập nhật 2026-11-01: đã tích hợp tinygltf v2.9.3 trước vì header-only, giảm chi phí build khi cần trả lời câu hỏi import sớm. Đây không phải quyết định cuối: nếu benchmark trên thiết bị thật (chưa có) cho thấy parse là nút thắt, đánh giá fastgltf bằng cùng fixture trong `cpp/tests/fixtures/`.
Cả hai MIT (đã fetch LICENSE trực tiếp 2026-11-01). fastgltf nhanh và hiện đại hơn nhưng trẻ hơn; tinygltf chậm nhưng cực kỳ phổ biến/ổn định. Quyết định cuối sau spike D3 (import file 100MB GLB, đo thời gian + bộ nhớ + hỗ trợ extensions KHR_materials). Không dùng cgltf (C, tốt) trừ khi cần giảm binary size — thêm 1 lựa chọn là thêm chi phí đánh giá.

## TD-05: meshoptimizer (MIT) cho simplify/LOD/index optimization — `Proposed`
MIT (đã verify LICENSE). Thư viện mesh processing hiếm hoi vừa nhẹ, vừa bảo trì tích cực, vừa thiết kế cho game pipeline đúng use case Mobile3D. Dùng cho: simplification, vertex cache/fetch optimization, LOD chain. Không dùng nó cho remesh/sculpt.

## TD-06: Tránh libigl / CGAL / OpenMesh ở core — `Proposed`
libigl là MPL-2.0 (đã verify LICENSE.MPL2) — dùng được trong app thương mại nhưng kéo Eigen + nhiều header, build Android nặng, và nhiều module của nó (đặc biệt phần giao với CGAL) có điều kiện GPL. CGAL phần lớn GPL/LGPL — **rủi ro license cao cho app closed-source**, loại trừ. OpenMesh BSD nhưng API nặng, ít giá trị hơn tự viết mesh container + meshoptimizer. Xem lại: nếu M2/M4 cần remesh/boolean thật, đánh giá lại từng thuật toán cụ thể thay vì kéo cả framework.

## TD-07: Project format tự thiết kế (folder + manifest JSON + binary mesh) — `Proposed`
Xem ARCHITECTURE.md §5. Trade-off: tự chịu migration; đổi lại kiểm soát undo/source/checkpoint. Dùng nlohmann/json (MIT) phía C++ hoặc parse manifest phía Kotlin bằng kotlinx.serialization — quyết ở M1, ưu tiên Kotlin parse để C++ không phụ thuộc JSON.

## TD-08: Không OpenCV ở M0–M2 — `Proposed`
OpenCV (Apache-2.0 bản 4.x) chỉ cần từ M3 (đánh giá chất lượng ảnh). Kéo sớm = +10–20MB APK và thời gian build. M3 sẽ dùng OpenCV Android SDK chính thức qua Maven nếu có, nếu không dùng prebuilt .aar tự host.

## TD-09: ONNX Runtime — hoãn tới M5 — `Deferred`
Apache-2.0/MIT hỗn hợp theo component, runtime lớn; không có nhu cầu inference on-device trước M5. Image-to-3D nếu làm sẽ chạy cloud trước.

## TD-10: Backend FastAPI + object storage, 1 service — `Deferred to M3`
Lý do: 1 dev vận hành được. Không quyết định chi tiết (queue, auth) cho tới khi M3 bắt đầu; mọi quyết định cloud trước đó là lãng phí.

## TD-11: Không game engine (Unity/Unreal/Filament/Babylon) làm renderer — `Proposed`
Filament (Apache-2.0) là ứng viên nghiêm túc và được cân nhắc: renderer PBR production-ready của Google. Bác bỏ cho M0 vì: sculpt cần kiểm soát buffer ở mức thấp, Filament thêm lớp trừu tượng và chi phí học API cho 1 dev, và việc thay renderer sau M0 khó hơn xây renderer mỏng tự viết cho đúng nhu cầu. **Đây là quyết định có trade-off lớn nhất trong discovery** — nếu spike D5–D7 (sculpt dirty-update) thất bại, phương án dự phòng là đánh giá lại Filament thay vì tự viết Vulkan.

## TD-12: Tên trung tính (2026-11-01)
Không dùng tên sản phẩm chưa chốt ở bất kỳ đâu trong code/tài liệu: native lib `libmobile3d.so`, package `com.mobile3d.validation` cho app validation. Đổi tên sau này là thao tác rename cơ học, không ảnh hưởng kiến trúc.
