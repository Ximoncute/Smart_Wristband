# ⌚ Vòng Tay Thông Minh (Smart Health Bracelet)

Dự án Vòng tay thông minh sử dụng vi điều khiển ESP32 kết hợp với các cảm biến để theo dõi sức khỏe và phát hiện té ngã, có ứng dụng AI nhúng (TinyML).

## 🌟 Chức năng chính
- **Đo nhịp tim & SpO2:** Sử dụng cảm biến `MAX30102`. Cảnh báo bằng còi (buzzer) khi nhịp tim < 50 BPM hoặc SpO2 < 92%.
- **Phát hiện té ngã (Fall Detection):** Sử dụng cảm biến quán tính `BMI160` kết hợp mô hình AI (Edge Impulse). Bật đèn LED cảnh báo ngay khi phát hiện người dùng bị ngã.
- **Giao tiếp:** 2 mạch ESP32 giao tiếp với nhau qua chuẩn UART.

## 🛠 Cấu trúc dự án
- `mach_dien/NODE_DO_NHIP_TIM`: Code ESP32 xử lý cảm biến nhịp tim MAX30102.
- `mach_dien/NODE_THEO_DOI_CHUYEN_DONG`: Code ESP32 xử lý cảm biến chuyển động BMI160 và chạy AI nhận diện té ngã.
- `train_ai/`: Chứa dữ liệu (CSV) và tài nguyên phục vụ huấn luyện mô hình học máy.
- `docs/`: Tài liệu liên quan đến dự án.

## 🚀 Hướng dẫn nhanh
1. Nạp code `NODE_DO_NHIP_TIM_1.ino` vào ESP32 thứ nhất.
2. Nạp code `NODE_THEO_DOI_CHUYEN_DONG_1.ino` vào ESP32 thứ hai.
3. Nối dây giao tiếp UART giữa 2 mạch (TX mạch này nối RX mạch kia và nối chung chân GND).
4. Cấp nguồn và mở Serial Monitor (Baudrate: `115200`) để xem thông số sức khỏe và trạng thái chuyển động.