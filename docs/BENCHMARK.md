# Benchmark Design (M0)

> Tên dự án: chưa quyết định. "Mobile3D" là tên trung tính tạm thời, không phải branding.

## Nguyên tắc
- Phân biệt nhãn kết quả: `Emulator Tested` (chỉ chứng minh chức năng/tích hợp, **không** kết luận hiệu năng) và `Verified on Physical Device` (ghi rõ model/SoC/GPU/RAM/Android). Không ngoại suy emulator → thiết bị tầm trung.
- Rendering và sculpt/edit là hai bài đo riêng ở cùng mesh size. Không báo cáo một con số FPS chung chung.
- Mỗi bài: fixture cố định (file mesh + nguồn/license của fixture), script cố định, ≥3 lần chạy, báo cáo median và p95. Ghi nhiệt độ/thermal status đầu và cuối phiên khi chạy trên thiết bị thật.

## Chỉ số bắt buộc
| Nhóm | Chỉ số | Cách đo |
|---|---|---|
| Rendering | FPS, frame time p50/p95 (ms) | Native frame counter trong renderer, xuất qua JNI `nativeGetFrameStats()`; cửa sổ 10s sau 2s warm-up |
| Rendering | RAM PSS (MB) | `adb shell dumpsys meminfo <package>` trước/sau khi load mesh |
| Sculpt/edit | Brush latency (ms) | Thời gian từ sự kiện chạm tới frame đầu tiên hiển thị biến dạng (đo ở M2; M0 đo proxy: sửa N vertex + upload) |
| Sculpt/edit | CPU deformation (ms/stroke hoặc ms/1k vertex) | Timer native quanh hàm deform |
| Sculpt/edit | BVH build/update (ms), normal recomputation (ms) | Timer native riêng từng giai đoạn, không gộp |
| Sculpt/edit | GPU buffer update (ms, bytes) | Timer quanh `glBufferSubData` + số byte upload |
| Mesh processing | Import / decimate / recompute normals (ms, peak RSS) | Timer native + `dumpsys meminfo` |
| Undo | Bộ nhớ mỗi stroke/command (KB), trần bộ nhớ undo | Đếm allocation trong command stack (M1/M2) |

## Fixture chuẩn (tạo ở D3, lưu `cpp/tests/fixtures/`)
Mesh ~10k, ~100k, ~500k, ~1M, ~2M triangles (2M chỉ là target kiểm chứng). Ghi nguồn tạo (script/Blender) và license vào `cpp/tests/fixtures/README.md`.

## Định dạng kết quả
Mỗi lần chạy ghi 1 dòng vào bảng trong PERFORMANCE.md: ngày, nhãn (Emulator/Device + model), fixture, chỉ số, median/p95, ghi chú nhiệt. Không ghi số làm tròn kiểu marketing; giữ số thô của 3 lần chạy ở phụ lục hoặc file log đi kèm.
