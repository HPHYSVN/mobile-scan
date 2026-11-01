# Product Specification (Discovery Draft v0.1)


> Tên dự án: chưa quyết định. "Mobile3D" trong tài liệu này là tên trung tính tạm thời, không phải branding. Thư viện native: `libmobile3d.so`, package: `com.mobile3d.*`.
Status: `Not verified` — chờ human developer xác nhận. Ngày: 2026-11-01.

## 1. Vấn đề & người dùng
Indie dev / sinh viên game / 3D artist cần asset game-ready nhưng pipeline hiện tại rời rạc: scan bằng 1 app (RealityScan/Polycam/KIRI), sculpt bằng app khác (Nomad), optimize/UV/export trên desktop (Blender). Mobile3D hợp nhất trên mobile: `Capture → Create → Edit → Optimize → Export`.

Không định vị: thay thế Blender, photogrammetry chuyên nghiệp, CAD.

## 2. Lợi thế cạnh tranh (giả thuyết, cần validate ở M3/M4)
| Đối thủ | Mạnh | Khoảng trống Mobile3D khai thác |
|---|---|---|
| RealityScan/Polycam/KIRI | Reconstruction chất lượng | Kết quả thường dừng ở mesh thô; ít tool đưa về game-ready trên cùng thiết bị |
| Nomad Sculpt | Sculpt mobile tốt nhất | Không capture, không pipeline LOD/UV/export game |
| Blender | Đầy đủ pipeline | Desktop-only, learning curve cao |

Khác biệt cốt lõi: **một project duy nhất đi từ capture tới GLB export có LOD + PBR**, offline-first, không khóa tính năng edit sau paywall cloud. Đây là giả thuyết sản phẩm — chưa có user test nào chứng minh.

## 3. Scope theo milestone (chỉ cam kết từng milestone một)
- **M0 Validation (14 ngày):** native viewport + import GLB/OBJ + orbit camera + benchmark. Không UI sản phẩm.
- **M1 Editor MVP:** project CRUD local, scene graph phẳng (≤3 cấp), transform gizmo, primitive (cube/sphere/cylinder/plane), material màu/PBR cơ bản, undo/redo, autosave, export GLB/OBJ.
- **M2 Sculpt:** raycast + Draw/Smooth/Inflate/Crease, symmetry X, mask. Remesh/decimate qua thư viện (xem DEPENDENCY_AUDIT).
- **M3 Capture:** CameraX capture session + chấm điểm ảnh (blur/exposure/overlap heuristic), import ảnh có sẵn; cloud reconstruction là tính năng *optional* sau khi có backend. ARCore chỉ dùng cho preview/scale nếu device hỗ trợ — không phải điều kiện bắt buộc.
- **M4 Game pipeline:** decimate, LOD chain, UV unwrap cơ bản, export preset Unity/Unreal (scale, axis, LOD naming).
- **M5:** Image-to-3D, AI texture, splatting — **Deferred**, không thiết kế kiến trúc xoay quanh M5.

## 4. Non-goals (v1)
Không: rigging/animation, node material, scripting,多人 collaboration, marketplace, iOS, retopo tự động chất lượng production (M4 chỉ cam kết decimate+LOD; retopo thật là stretch).

## 5. Ràng buộc thiết bị
Tham chiếu: Android 10+ (API 29), RAM 6–8GB, Adreno 6xx/Mali-G7x. Thiết bị 4GB: giới hạn mesh viewport (cảnh báo ở 500k tris, hard-limit tùy benchmark M0). Không giả định LiDAR/ToF. Mọi tính năng depth/AR phải degrade về capture thường.

## 6. Tiêu chí thành công đo được
- M0: xem VALIDATION_PLAN.md (frame time, memory, import time trên thiết bị thật).
- M1: tạo project → thêm primitive → transform → undo → kill app → mở lại → export GLB mở được trong Blender/gltf-validator không lỗi.
- Mỗi milestone có acceptance riêng, viết trước khi code.

## 7. Câu hỏi mở (chờ quyết định)
Q1 monetization (free+export watermark? subscription cloud?) — ảnh hưởng thiết kế backend. Q2: có cần tài khoản ở M1 không (đề xuất: không). Q3: ngôn ngữ UI đầu tiên (VI/EN).
