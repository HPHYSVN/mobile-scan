# Roadmap (Discovery v0.1)


> Tên dự án: chưa quyết định. "Mobile3D" trong tài liệu này là tên trung tính tạm thời, không phải branding. Thư viện native: `libmobile3d.so`, package: `com.mobile3d.*`.
Ước lượng cho 1 dev + AI, giả định ~15–25 giờ dev/tuần. Đây là khoảng rộng có chủ đích — không cam kết ngày phát hành. Mỗi milestone chỉ bắt đầu khi milestone trước đạt acceptance và có bản ghi benchmark/thiết bị (hoặc ghi rõ `Not verified` + lý do).

| Milestone | Nội dung | Ước lượng | Điều kiện qua cửa (exit criteria) |
|---|---|---|---|
| **M0 Validation** | Xem VALIDATION_PLAN.md | 14 ngày (sprint cố định) | Đạt ngưỡng benchmark trên ≥1 thiết bị thật; quyết định Go/No-Go cho GLES renderer tự viết |
| **M1 Editor MVP** | Project CRUD, scene, transform, primitive, material cơ bản, undo/redo, autosave, export GLB/OBJ | 4–6 tuần sau M0 | Kịch bản end-to-end ở PRODUCT_SPEC §6 pass trên thiết bị tham chiếu; test migration schema v1→v1 (khung) |
| **M2 Sculpt** | Raycast/BVH, 4 brush, symmetry, mask, decimate | 4–8 tuần | Sculpt 500k tris đạt frame budget đo ở M0; undo cho stroke; bài test hiệu năng phiên 10 phút |
| **M3 Capture** | CameraX session, chấm điểm ảnh, (tùy chọn) backend reconstruction tối giản | 6–10 tuần + backend | Scan vật thể thật → mesh vào được editor và edit tiếp được; đo chi phí/job nếu dùng cloud; fallback không-ARCore hoạt động |
| **M4 Game pipeline** | LOD chain, UV cơ bản, export preset Unity/Unreal | 4–6 tuần | Asset xuất ra import vào Unity/Unreal (hoặc ít nhất Blender + gltf-validator) không lỗi, đúng scale/axis theo preset |
| **M5 Advanced** | Image-to-3D, AI texture, splatting | Chưa ước lượng | Chỉ lập kế hoạch khi M4 xong và có dữ liệu người dùng |

## Nguyên tắc roadmap
- Không làm song song 2 milestone. Backend (M3) không được bắt đầu trước M1 xong.
- Mỗi milestone kết thúc bằng: demo trên thiết bị thật, cập nhật PERFORMANCE/TESTING (tạo ở M0), và quyết định Go/No-Go ghi vào CHANGELOG.
- Nếu M0 No-Go ở renderer: dừng, đánh giá lại Filament (TD-11) trước khi viết tiếp code — không cố cứu bằng tối ưu vi mô.

## Backlog (không cam kết)
Retopo tự động, boolean, texture painting, animation/rig, iOS port (kiến trúc native C++ cho phép nhưng UI phải viết lại), Vulkan backend.

## Trạng thái 2026-11-03
M1: đạt trên emulator (editor, material, transform/undo, autosave, import/export, project format). M2: lát cơ bản đạt (4 brush, sculpt undo, persistence) — chưa symmetry/masking/remesh, chưa benchmark thiết bị. M3: đạt trên emulator+server Linux (Scan UI, capability detection, upload resume, photogrammetry server COLMAP→Poisson→GLB, tải về vào editor). Chưa: AR session, dense MVS, kiểm chứng Windows/thiết bị thật. M4: đạt phần export (format/topology/độ phân giải/subdiv, scene export); chưa UV/baking/LOD chain preset Unity/Unreal. M5: chưa bắt đầu. Mọi mục "đạt" đều là `Emulator Tested`, chưa `Verified on Physical Device`.
