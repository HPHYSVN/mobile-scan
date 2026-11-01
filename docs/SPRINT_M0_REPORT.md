# M0 Sprint Report — Technical Validation (2026-11-01)

> Tên dự án: chưa quyết định. "Mobile3D" là tên trung tính tạm thời, không phải branding.

## Kết luận ngắn
Nền tảng Kotlin → JNI → C++17 → GLES 3.0 hoạt động và đã được kiểm chứng trên host + emulator. **Chưa đủ điều kiện Go/No-Go cho renderer** vì điều kiện đặt ra từ đầu (số liệu 500k tris trên thiết bị vật lý tầm trung) chưa thể thực hiện — chưa có thiết bị. Đề xuất: coi M0 là **"đạt về mặt chức năng/tích hợp, chờ benchmark thiết bị"**, bắt đầu M1 với các phần không phụ thuộc hiệu năng GPU (project, scene, transform, persistence), và giữ checkpoint Go/No-Go mở cho tới khi có máy thật. Không bắt đầu M2 sculpt thật trước checkpoint đó.

## Phân loại theo yêu cầu báo cáo
### Implemented
Scaffold Android (Kotlin, package `com.mobile3d.validation`), JNI handle-based, `libmobile3d.so` C++17 GLES 3.0 (16KB page-size aligned), GLB load/export + OBJ load (tinygltf v2.9.3, MIT), orbit camera, deform spike với timer 3 giai đoạn, undo snapshot theo vùng, JNI ping, fixture generator + benchmark script lặp lại (`scripts/run_benchmarks.sh`).

### Tested (host)
CTest 5/5: mesh, loader, deform, io (GLB round-trip + OBJ), undo (bit-exact + cap). Khronos glTF-Validator: file export 0 errors / 0 warnings. Clean build từ đầu: thành công (~10 s khi dependency đã cache, 117 s lần đầu).

### Emulator Tested (không ngoại suy hiệu năng)
Toàn bộ luồng Android trên AVD SwiftShader: import sphere_98k median 146.2 ms (3 cold runs, `docs/benchmark-results/2026-11-01-emulator.md`), PSS ~49 MB, deform hiển thị thật, lifecycle (xoay/Home/kill) không crash, JNI 0.14 µs/call (x86).

### Not verified
- Mọi hiệu năng trên GPU/CPU ARM thật: FPS ở bất kỳ mesh size nào, upload cost trên Adreno/Mali, nhiệt/throttling, adjacency trên CPU tầm trung.
- fastgltf, mesh 1M–2M tris, ASan build, kill đúng lúc đang import, DirectByteBuffer import.

### Deferred (có lý do đã ghi)
D6 phiên nhiệt (vô nghĩa trên emulator), D7 Go/No-Go (thiếu thiết bị), DirectByteBuffer (chưa cần), Filament (chỉ xét nếu checkpoint thiết bị thất bại).

## Phát hiện kỹ thuật chính
1. Chi phí deform/normals tỉ lệ theo kích thước region, không theo tổng mesh (host) — tín hiệu tốt cho sculpt, cần xác nhận trên ARM.
2. GPU upload là giai đoạn đắt nhất trên SwiftShader (~6 ms/4.1k verts) — chưa rõ bao nhiêu là artifact của driver phần mềm. Đây là câu hỏi số 1 cho benchmark thiết bị thật.
3. Adjacency build là chi phí một lần đáng kể trên CPU chậm (325 ms emulator / 98k tris) — M2 phải cache adjacency và không build lại mỗi stroke.
4. Undo snapshot tốn 52 bytes/vertex — ngân sách bộ nhớ undo phải là tham số thiết kế, không mặc định.

## Đề xuất phạm vi M1 (Editor MVP) — chờ duyệt
Chỉ gồm phần không phụ thuộc Go/No-Go GPU:
1. Project format `project.m3d/` theo ARCHITECTURE §5 (manifest schemaVersion=1 + migration khung + fixture test).
2. Scene graph phẳng trong native (thêm/xóa/chọn node, transform), Kotlin UI tối thiểu bằng Compose: project list + viewport + panel transform. (Compose được thêm ở milestone này, kèm audit dependency AndroidX.)
3. Primitive cube/sphere/cylinder/plane từ native, material màu cơ bản.
4. Undo/redo nối vào transform (dùng lại `core/undo` + command cho transform), autosave.
5. Export GLB qua exporter đã validate; import GLB/OBJ vào project.
Ngoài phạm vi M1: sculpt, camera/capture, gizmo kéo thả trực quan (M1 dùng slider/numeric transform trước — gizmo là M1.5 nếu thời gian cho phép), texture.

Điều kiện dừng M1: kịch bản end-to-end trong PRODUCT_SPEC §6 pass trên emulator + host tests; benchmark thiết bị thật vẫn là checkpoint riêng, không chặn M1 nhưng **chặn M2**.
