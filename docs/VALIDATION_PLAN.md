# Technical Validation Sprint 14 ngày (M0)


> Tên dự án: chưa quyết định. "Mobile3D" trong tài liệu này là tên trung tính tạm thời, không phải branding. Thư viện native: `libmobile3d.so`, package: `com.mobile3d.*`.
Mục tiêu duy nhất: trả lời bằng số đo thật 3 câu hỏi — (1) JNI+native renderer GLES tự viết có đáng làm không, (2) import GLB lớn trên máy tầm trung tốn bao nhiêu, (3) cập nhật buffer theo vùng bẩn khi sculpt có đạt frame budget không. Không xây UI sản phẩm, không project format hoàn chỉnh.

## Tiền điều kiện (Ngày 1, blocking)
- [x] Cài NDK + CMake của SDK — xong 2026-11-01: NDK 27.2.12479018, CMake 3.22.1, thêm platform 35. Xem ENVIRONMENT.md.
- [x] Giai đoạn hiện tại: kiểm chứng trên emulator (kết quả ghi nhãn `Emulator Tested`). Kết nối thiết bị thật 6–8GB RAM hoãn theo chỉ đạo 2026-11-01 — mọi kết luận hiệu năng ghi `Not verified on physical device`, không ngoại suy sang Android tầm trung.
- [ ] Chốt cấu trúc repo (chờ human xác nhận, xem báo cáo cuối) rồi mới `git init` + scaffold.

## Kế hoạch theo ngày
| Ngày | Việc | Acceptance |
|---|---|---|
| D1 | ✅ Xong 2026-11-01 (sớm hơn lịch: gộp tiền điều kiện + scaffold trong cùng ngày): 1 Activity + GLSurfaceView + CMake + JNI + GLES3 cube | APK build thành công, cài và chạy trên **emulator** (cube render, không crash, restart OK). Claim: `Implemented` + `Emulator Tested`. Thiết bị thật: `Not verified` |
| D2 | Đo thời gian build clean/incremental; host-side CMake test target (GoogleTest hoặc assert tự viết — quyết nhanh, ưu tiên Catch2/GoogleTest có sẵn) | Ghi số vào PERFORMANCE.md; test host chạy xanh |
| D3 | ✅ Một phần 2026-11-01: dùng **tinygltf** thay fastgltf cho spike đầu (header-only, tích hợp nhanh hơn; fastgltf vẫn chưa benchmark — xem TD-04). Fixture tự sinh 6k/24k/98k/393k tris, host bench 3 runs + emulator 24k | Số ở PERFORMANCE.md. Peak RSS riêng cho import: chưa tách. fastgltf: `Not verified` |
| D4 | ✅ Một phần 2026-11-01: vẽ mesh import (Lambert 1 directional light, chưa PBR), orbit drag + pinch qua JNI. Chưa đạt phần 1M tris/FPS thiết bị thật | Emulator sphere_24k render đúng (screenshot). 1M tris: `Not verified` |
| D5 | ✅ Một phần 2026-11-01: deform proxy ~4.1k verts/frame, timer tách deform/normals/upload, chạy trên sphere_98k emulator (chưa phải 500k tris, chưa phải thiết bị thật) | Số ở PERFORMANCE.md. 500k tris: `Not verified` |
| D6 | ⏸ Hoãn: phiên nhiệt 10 phút trên emulator SwiftShader không có ý nghĩa. Chờ thiết bị thật | `Not verified` |
| D7 | ⏸ Chưa quyết (đúng điều kiện): chưa có số 500k tris trên thiết bị thật. Ứng viên tối ưu trước khi xét Filament: gộp upload thành bounding-range, SoA thay interleaved, chuyển deform sang worker thread | `Not verified` |
| D8 | ✅ Xong 2026-11-01 (host + validator): OBJ parser tối thiểu + GLB export qua tinygltf | Khronos glTF-Validator: 0 errors / 0 warnings cho file export; host round-trip test pass |
| D9 | ✅ Một phần 2026-11-01: JNI ping 10k calls = 0.14 µs/call (emulator x86). Chưa làm: DirectByteBuffer vs đường file | Số ở PERFORMANCE.md. DirectByteBuffer: `Deferred` — chỉ cần khi import qua stream thay vì file |
| D10 | ✅ Một phần 2026-11-01: `core/undo` snapshot theo vùng vertex + stack có cap, host test undo/redo bit-exact + đo 52 B/vert. Chưa chạy ASan build, chưa nối vào UI/sculpt | ASan: `Not verified` — bổ sung khi có thao tác người dùng thật ở M1 |
| D11 | ✅ Xong 2026-11-01 trên emulator: xoay, Home x3, force-stop/relaunch — không crash. Chưa test kill **trong lúc** import (import hiện ~0.3 s, khó trúng thời điểm; để M1 khi có file lớn) | Nhãn: `Emulator Tested` |
| D12 | Benchmark tổng hợp lặp lại D3–D6, chạy 3 lần lấy trung vị | Bảng số cuối trong PERFORMANCE.md |
| D13 | Dọn dẹp: xóa spike chết, viết README/ARCHITECTURE/PERFORMANCE/TESTING/CHANGELOG bản đầu | Repo người khác clone build được theo README |
| D14 | Báo cáo sprint: Implemented/Tested/Verified-on-device/Not-verified cho từng mục; đề xuất phạm vi M1 chính xác | Human duyệt mới sang M1 |

## Ngân sách hiệu năng mục tiêu (để kiểm chứng, không phải cam kết)
Thiết bị tham chiếu 6–8GB: viewport 500k tris sculpt p95 ≤16.6ms; 1M tris orbit-only p95 ≤16.6ms; import GLB 30MB ≤3s; peak memory khi import ≤600MB trên máy 6GB. Nếu không đạt, hạ ngân sách tris có số liệu kèm theo thay vì bỏ mục tiêu.

## Ngoài phạm vi sprint
Compose UI hoàn chỉnh, project save/load, sculpt brush thật (chỉ giả lập sửa vertex), CameraX/ARCore, backend, bất kỳ tính năng M1+ nào.

## Điều chỉnh đã duyệt (2026-11-01, ghi đè các mục mâu thuẫn ở trên)
- Mục tiêu 2M triangles **không phải** hiệu năng đã xác nhận — chỉ là benchmark target cần kiểm chứng, và phải tách: (a) rendering-only (orbit, không sửa mesh), (b) sculpt/edit (CPU deformation + BVH update + normal recomputation + GPU buffer upload + undo memory) ở cùng mesh size. Không gộp hai số này.
- Benchmark phải lặp lại được: cùng fixture mesh, cùng script, ≥3 lần chạy, báo cáo median + p95. Chỉ số bắt buộc: FPS, frame time p50/p95, RAM (PSS qua `dumpsys meminfo`), brush latency (input→phản hồi trên màn hình), thời gian xử lý mesh (import/decimate/recompute normals), undo memory/mỗi stroke. Thiết kế chi tiết: BENCHMARK.md.
- Không tự viết lại thuật toán phổ biến khi đã có thư viện license sạch (MIT/Apache-2.0/BSD) và benchmark tốt hơn bản tự viết.
- Filament chỉ là phương án thay thế **renderer**. Nó không thay thế geometry/sculpt engine.
