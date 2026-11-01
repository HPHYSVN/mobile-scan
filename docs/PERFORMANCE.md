# Performance Log

> Tên dự án: chưa quyết định. "Mobile3D" là tên trung tính tạm thời, không phải branding.
> Quy tắc ghi số: xem BENCHMARK.md. Số emulator SwiftShader **không** đại diện cho GPU thật.

## 2026-11-01 — M0 Ngày 1 (Emulator Tested, KHÔNG phải benchmark hiệu năng)
Môi trường: AVD Medium_Phone, Android API 37, x86_64, GPU **SwiftShader (render bằng CPU)**. Fixture: cube 24 verts / 12 tris.

| Chỉ số | Giá trị | Ghi chú |
|---|---|---|
| Frame time avg | ~36–47 ms, giảm dần theo thời gian chạy | SwiftShader + emulator, **không dùng để kết luận gì về thiết bị thật** |
| RAM PSS của app | 37,448 KB (lần 1), 38,274 KB (lần 2) | `dumpsys meminfo` khi đang render cube |
| Build APK clean | 117 s (39 tasks, gồm tải dependency lần đầu) | Máy dev 12 core, 15 GB RAM |
| Build incremental (đổi CMake) | ~1 s (7 tasks) | |
| Host test build+run | ~1 s, 1/1 pass | `cpp/tests` |

Chưa đo (theo kế hoạch ở các ngày sau): rendering mesh lớn, sculpt/edit (deformation, BVH, normals, GPU upload, brush latency), import, undo memory. Trạng thái: `Not verified`.

## 2026-11-01 — M0 Ngày 3–4: import GLB (tinygltf)
Fixture sinh bằng `scripts/generate_fixtures.py` (xem `cpp/tests/fixtures/README.md`). Host = máy dev x86_64, **chỉ để kiểm chứng logic/độ phức tạp, không phải số thiết bị**. Emulator = SwiftShader, nhãn `Emulator Tested`.

| Fixture | Host median (3 runs) | Emulator median (3 runs) |
|---|---|---|
| sphere_6k.glb (6,144 tris) | 0.15 ms | — |
| sphere_24k.glb (24,576 tris) | 0.39 ms | 123.2 ms (87.8 / 123.2 / 133.3) |
| sphere_98k.glb (98,304 tris) | 1.78 ms | chưa đo trong app (chỉ có asset, chưa load) |
| sphere_393k.glb (393,216 tris) | 6.52 ms | chưa đo |

RAM emulator khi render sphere_24k: PSS 41,729–43,047 KB. Frame time emulator ~41–45 ms avg với SwiftShader — **không kết luận hiệu năng**. Import trên emulator chậm hơn host ~300 lần chủ yếu do CPU emulator x86 + tinygltf parse; con số tuyệt đối không ngoại suy sang ARM thật. Trạng thái thiết bị thật: `Not verified`.

## 2026-11-01 — M0 Ngày 5–7: proxy sculpt / dirty-range upload (Emulator Tested)
Thiết kế đo: tách 3 giai đoạn — CPU deform, normal recomputation (từ face adjacency), GPU upload bằng `glBufferSubData` theo run liên tục của vertex đã chọn. Region tự chọn ~4.1k vertex theo hướng normal. **Deform chạy trên GL thread trong spike này** để cô lập chi phí từng giai đoạn; mô hình threading production (worker thread) chưa quyết — không coi số này là kiến trúc cuối.

Host (máy dev, 60 iterations, median):
| Fixture | Adjacency build (1 lần) | Region | Deform | Normals |
|---|---|---|---|---|
| sphere_98k (98,304 tris) | 2.82 ms | 4,119 | 0.006 ms | 0.107 ms |
| sphere_393k (393,216 tris) | 11.38 ms | 4,494 | 0.005 ms | 0.110 ms |

Nhận xét host: chi phí deform/normals tỉ lệ với **kích thước region**, không tỉ lệ với tổng mesh; adjacency là chi phí một lần theo tổng mesh.

Emulator (SwiftShader, sphere_98k, trung bình tích lũy hiển thị trong app sau ~250 frames):
| Giai đoạn | Thời gian |
|---|---|
| Deform 4,119 verts | ~0.09–0.10 ms |
| Normal recomputation | ~1.4–1.7 ms |
| GPU dirty upload (nhiều `glBufferSubData` run) | ~5.8–7.0 ms |
| Frame time tổng | ~51 ms (SwiftShader raster 98k tris — không kết luận GPU thật) |
| RAM PSS | 49,016 KB |

Nhận xét emulator (chỉ đúng cho SwiftShader): upload là giai đoạn đắt nhất, nhiều khả năng do overhead mỗi lần gọi + driver phần mềm sao chép buffer. **Không ngoại suy sang Adreno/Mali.** Chưa đo: so sánh dirty-range vs full-buffer upload, upload 1 bounding-range gộp, phiên nhiệt 10 phút, BVH raycast (chưa có — thuộc M2), brush latency thật (chưa có input brush). Trạng thái Go/No-Go D7: **chưa quyết** — thiếu thiết bị thật, theo đúng điều kiện trong VALIDATION_PLAN.

## 2026-11-01 — M0 Ngày 8–11 (Emulator Tested / host)
| Hạng mục | Kết quả | Ghi chú |
|---|---|---|
| JNI ping (10,000 calls, sau 1,000 warm-up) | 0.14 µs/call trên emulator x86 | Chỉ có ý nghĩa so sánh tương đối; ARM thật sẽ khác. Kết luận giới hạn: overhead JNI cho lệnh rẻ không phải nút thắt ở tần suất lệnh UI thông thường — `Not verified` trên ARM |
| Adjacency build trong app (sphere_98k) | 325.3 ms trên emulator (host: 2.82 ms) | CPU emulator chậm ~100 lần so với host ở tác vụ này. Số này nằm **ngoài** thời gian `loaded ... ms` đã log trước đó — tổng chờ load+prep trên emulator ~0.5–0.6 s cho 98k tris |
| Undo snapshot (host, tính toán) | 52 bytes/vertex đã sửa (index 4B + before/after 48B) | Region 5k verts ≈ 260 KB/stroke; trần 100 commands ≈ 26 MB worst-case cho region cỡ đó — phải có ngân sách bộ nhớ undo ở M1/M2, không để mặc định vô hạn |

Chưa đo trên thiết bị thật: tất cả các số trên. `Not verified on physical device`.
