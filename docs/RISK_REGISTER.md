# Risk Register (Discovery v0.1)


> Tên dự án: chưa quyết định. "Mobile3D" trong tài liệu này là tên trung tính tạm thời, không phải branding. Thư viện native: `libmobile3d.so`, package: `com.mobile3d.*`.
Thang: Xác suất × Tác động (Cao/TB/Thấp). Chủ sở hữu mặc định: human + AI cùng theo dõi mỗi milestone.

| ID | Rủi ro | XS | TĐ | Giảm thiểu | Tín hiệu sớm |
|---|---|---|---|---|---|
| R1 | Sculpt dirty-buffer update không đạt 60fps ở 500k tris trên Adreno/Mali tầm trung | TB | Cao | Spike ngay D5–D7 của sprint; nếu fail: giảm ngân sách tris, chia mesh theo chunk spatially, hoặc xét Filament (TD-11) | Frame time spike >16ms khi brush |
| R2 | Không có thiết bị thật để benchmark — hiện tại máy dev **chưa kết nối thiết bị nào, chưa cài NDK** (đã kiểm chứng 2026-11-01: `adb devices` rỗng, SDK thiếu `ndk/` và `cmake/`) | **Cao (đang xảy ra)** | Cao | Ngày 1 của sprint: cài NDK qua sdkmanager, chuẩn bị ít nhất 1 máy 6–8GB RAM (Adreno hoặc Mali). Emulator x86_64 hiện có chỉ để test logic, **không** dùng kết luận hiệu năng GPU | Không có số benchmark ngày 7 |
| R3 | ARCore/Depth không có trên máy người dùng mục tiêu | TB | TB | Capture thường bằng CameraX là đường chính; ARCore là enhancement. Không để feature nào phụ thuộc cứng vào Depth | % máy hỗ trợ đo bằng Play Console sau này |
| R4 | Photogrammetry cloud tốn chi phí vượt kiểm soát (GPU job, storage) | TB | Cao | M3 bắt đầu bằng: giới hạn ảnh/job, hàng đợi 1 worker, đo chi phí/job thực trước khi mở cho user; cân nhắc dùng engine mã nguồn mở (COLMAP/Meshroom — cần audit license riêng ở M3) thay vì API trả phí | Chi phí/job > ngưỡng đặt ở M3 |
| R5 | Scope creep: cố làm M2–M5 song song | Cao | Cao | Roadmap khóa theo milestone; tính năng mới vào backlog, không vào sprint đang chạy | Task ngoài VALIDATION_PLAN xuất hiện giữa sprint |
| R6 | 1 dev + AI: bus factor, AI sinh code native khó debug (JNI crash, UB) | TB | Cao | Native code bắt buộc có unit test chạy trên host (x86) bằng CMake thường, ASan khi debug; JNI surface giữ nhỏ; mọi crash native phải tái hiện được bằng test | Crash chỉ thấy trên device, không tái hiện host |
| R7 | License GPL/MPL lọt vào core qua thư viện geometry (CGAL qua libigl...) | Thấp | Cao | Quy tắc DEPENDENCY_AUDIT; audit lại ở mỗi milestone có geometry mới | Dependency mới không có dòng trong audit |
| R8 | Thermal throttling làm benchmark sai lệch / UX sculpt giật sau 5–10 phút | TB | TB | Benchmark phải gồm phiên 10 phút liên tục, ghi nhiệt độ/`dumpsys thermalservice` nếu có; thiết kế ngân sách tris theo phiên dài, không theo 30 giây đầu | FPS giảm >20% sau 5 phút |
| R9 | Project format migration phá dữ liệu người dùng khi schema đổi | Thấp (M1+) | Cao | schemaVersion từ ngày đầu, fixture test cho từng version, không bao giờ ghi đè file gốc khi migrate lỗi | Test migration đỏ |
| R10 | Build native chậm làm vòng lặp dev/AI kém hiệu quả | TB | TB | Đo thời gian build ngày 2; cân nhắc ccache, unity build cho thư viện; chỉ tách module khi >60s incremental | Incremental build >60s |
| R11 | Chất lượng reconstruction từ ảnh điện thoại tầm trung không đủ tốt để giữ chân user (rủi ro sản phẩm, không phải kỹ thuật) | TB | Cao | M3 phải có bài test với vật thể thật + tiêu chí chất lượng định lượng trước khi xây UI capture hoàn chỉnh; nếu kém, pivot trọng tâm sang Editor/Sculpt (M1/M2 đã có giá trị độc lập) | Mesh scan không dùng được sau edit thủ công hợp lý |
