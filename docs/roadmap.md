# Kế hoạch dự án: UART Bootloader cho STM32F103 (Gateway ESP32)

## 1. Mục tiêu dự án

Xây dựng một custom bootloader chạy trên STM32F103C8T6 (Blue Pill) cho phép cập nhật firmware qua giao tiếp **UART**, với ESP32 đóng vai trò gateway nhận file `.bin` (qua WiFi Web OTA / USB Serial) và truyền qua UART sang STM32 theo giao thức packet-based tin cậy (có CRC, ACK/NACK, Flow Control).

**Giá trị nổi bật cho CV / Đồ án:**
- Hiểu sâu về low-level flash memory, linker script, vector table (`SCB->VTOR`), con trỏ hàm Reset Handler.
- Thiết kế giao thức truyền nhận UART có đóng khung (framing), kiểm tra lỗi CRC16 từng gói & CRC32 toàn file.
- Tư duy production-grade: an toàn khi mất điện (anti-brick), fail-safe, emergency boot pin, timeout watchdog.
- Kỹ năng tích hợp hệ thống nhúng lai (STM32 Bare-metal/HAL + ESP32 FreeRTOS/ESP-IDF/Arduino).

---

## 2. Kiến trúc hệ thống

```
┌─────────────┐   WiFi / USB    ┌──────────────────┐   Direct UART   ┌─────────────────────┐
│  PC / Phone │ ───────────▶ │   ESP32 Gateway  │ ─────────────▶ │ STM32F103 (Target)  │
│ (upload .bin)│               │ (Web OTA Server) │  (115200 bps)   │  Bootloader + App   │
└─────────────┘               └──────────────────┘                 └─────────────────────┘
```

**Phân vùng Flash STM32F103C8T6 (64KB Flash, 20KB SRAM):**

| Vùng | Địa chỉ | Kích thước | Nội dung |
|---|---|---|---|
| **Bootloader** | `0x08000000` – `0x08004000` | 16 KB | Firmware bootloader cố định (nhận UART, nạp flash, verify, jump) |
| **Application** | `0x08004000` – `0x08010000` | 48 KB | Firmware chính của người dùng, được cập nhật từ xa |

---

## 3. Danh sách linh kiện

- 1x **STM32F103C8T6** (Blue Pill)
- 1x **ESP32** (ESP32-WROOM-32 hoặc ESP32-S3)
- 1x **Mạch nạp ST-Link V2** (dùng nạp bootloader lần đầu tiên)
- 1x **Mạch chuyển USB-UART** (CH340/CP2102/FT232 - dùng để test trực tiếp từ PC)
- Dây jumper, nút nhấn, LED breadboard

---

## 4. Lộ trình triển khai theo giai đoạn

### Giai đoạn 0 — Chuẩn bị & Xác minh phần cứng (1-2 ngày)
- [ ] Test STM32 Blue Pill: blink LED, kiểm tra nạp code qua ST-Link OK.
- [ ] Test UART giữa STM32 và máy tính (qua USB-TTL) ở tốc độ 115200 bps.
- [ ] Test UART giữa STM32 và ESP32 (đấu chéo TX-RX, nối chung GND).
- [ ] Đọc tài liệu: Reference Manual STM32F103 (phần Flash & USART), AN2606, AN3155.

---

### Giai đoạn 1 — Lập trình Bootloader Core trên STM32 (3-4 ngày)
- [ ] Viết Linker script cho Bootloader (`FLASH_BOOT.ld`: Origin = `0x08000000`, Size = 16K).
- [ ] Viết Linker script cho Application (`FLASH_APP.ld`: Origin = `0x08004000`, Size = 48K).
- [ ] Xây dựng các hàm xử lý Flash: `flash_unlock()`, `flash_erase_pages()`, `flash_write()`.
- [ ] Xây dựng thuật toán tính CRC32 phần cứng / phần mềm để verify firmware.
- [ ] Viết hàm `jump_to_app()`: tắt interrupt, de-init USART/SysTick, remap `SCB->VTOR = 0x08004000`, nạp lại MSP và gọi Reset Handler của Application.

---

### Giai đoạn 2 — Giao thức UART Framing & Packet Parser (3-4 ngày)
- [ ] Xây dựng State Machine giải mã gói tin UART (Header `0xAA 0x55` -> CMD -> Length -> Data -> CRC16 -> Tail `0x0D`).
- [ ] Xử lý các lệnh:
  - `CMD_PING` (0x01) -> Phản hồi `RESP_PONG` (0x81).
  - `CMD_ERASE` (0x02) -> Xóa các page Flash của vùng Application -> Phản hồi `RESP_ERASE` (0x82).
  - `CMD_WRITE_DATA` (0x03) -> Ghi chunk vào Flash -> Phản hồi `RESP_WRITE_ACK` (0x83).
  - `CMD_VERIFY_CRC` (0x04) -> Kiểm tra CRC32 toàn bộ vùng nhớ Flash -> Phản hồi `RESP_VERIFY` (0x84).
  - `CMD_JUMP_APP` (0x05) -> Thực thi nhảy vào User Application.
- [ ] Viết script Python `tools/uart_uploader.py` để test nạp trực tiếp từ máy tính qua cổng COM.

---

### Giai đoạn 3 — Viết Firmware Gateway trên ESP32 (4-5 ngày)
- [ ] Cấu hình UART trên ESP32 giao tiếp với STM32 (Baudrate 115200 hoặc 230400 bps).
- [ ] Tạo Web Server trên ESP32 (chế độ WiFi Access Point hoặc kết nối WiFi sẵn có).
- [ ] Giao diện Web HTML/JS cho phép người dùng chọn file `.bin` và bấm nút "Upload Firmware".
- [ ] ESP32 nhận file `.bin`, bóc tách thành từng gói packet theo giao thức UART-BLP, gửi sang STM32 và chờ ACK.
- [ ] Hiển thị thanh tiến trình (% upload) và trạng thái trực tiếp trên trình duyệt.

---

### Giai đoạn 4 — Hardening, Fail-Safe & Tối ưu hóa (2-3 ngày)
- [ ] Kiểm tra tính hợp lệ của Application trước khi nhảy (kiểm tra địa chỉ MSP nằm trong SRAM `0x20000000 - 0x20005000`).
- [ ] Cơ chế Anti-Brick: nếu quá trình nạp bị ngắt nguồn giữa chừng, bootloader vẫn hoạt động và sẵn sàng nhận nạp lại.
- [ ] Chân Boot khẩn cấp (Emergency Boot Pin - ví dụ `PA0`): giữ nút nhấn khi reset để luôn ở lại Bootloader.
- [ ] Thêm chân điều khiển Reset (`NRST`) và `BOOT0` từ ESP32 sang STM32 để ESP32 có thể tự động reset/cưỡng chế nạp STM32 khi cần.

---

### Giai đoạn 5 — Tài liệu & Đóng gói sản phẩm (1-2 ngày)
- [ ] Hoàn thiện `README.md` với đầy đủ sơ đồ nối dây, bảng lệnh, hướng dẫn biên dịch và sử dụng.
- [ ] Tạo demo application (chớp LED, gửi bản tin UART định kỳ).
- [ ] Quay video demo quá trình nạp OTA từ điện thoại/máy tính qua ESP32 vào STM32.

---

## 5. Tài liệu tham khảo hữu ích
- STM32F103 Reference Manual (RM0008) — Flash programming & USART controller.
- ST AN2606: *STM32 microcontroller system memory boot mode*.
- ST AN3155: *USART protocol used in the STM32 bootloader*.
- Cortex-M3 Programming Manual (PM0056) — SCB->VTOR & Exception handling.
