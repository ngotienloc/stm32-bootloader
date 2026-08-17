# Kế hoạch dự án: CAN Bootloader cho STM32F103 (Gateway ESP32-S3)

## 1. Mục tiêu dự án

Xây dựng một bootloader chạy trên STM32F103C8T6 (Blue Pill) cho phép cập nhật firmware qua CAN bus, với ESP32-S3 đóng vai trò gateway nhận file `.bin` (qua WiFi/USB) và chuyển tiếp qua CAN theo giao thức tự định nghĩa (lấy cảm hứng từ UDS).

**Giá trị show trong CV:**
- Hiểu low-level flash memory, linker script, vector table
- Thiết kế giao thức truyền dữ liệu qua CAN (multi-frame)
- Tư duy production-grade: an toàn khi mất điện, verify checksum, fail-safe
- Kỹ năng tích hợp 2 nền tảng MCU khác nhau (STM32 + ESP32)

---

## 2. Kiến trúc hệ thống

```
┌─────────────┐   WiFi/USB    ┌──────────────────┐   CAN bus   ┌─────────────────────┐
│  PC / Phone │ ───────────▶ │  ESP32-S3 Gateway │ ──────────▶ │  STM32F103 (Target)  │
│ (upload .bin)│               │  (TWAI + web UI)  │             │  Bootloader + App    │
└─────────────┘               └──────────────────┘             └─────────────────────┘
```

**Memory map STM32F103C8T6 (64KB flash):**

| Vùng | Địa chỉ | Kích thước | Nội dung |
|---|---|---|---|
| Bootloader | 0x08000000 – 0x08004000 | 16KB | Code cố định, không bị ghi đè |
| Application | 0x08004000 – 0x08010000 | 48KB | Firmware chính, được cập nhật qua CAN |

---

## 3. Danh sách linh kiện (recap)

- 1x STM32F103C8T6 (Blue Pill)
- 1x ESP32-S3
- 2x CAN transceiver (TJA1050/SN65HVD230)
- 2x điện trở 120Ω (termination)
- 1x ST-Link V2
- Breadboard, dây jumper, nút nhấn, LED (tùy chọn: OLED SSD1306)

---

## 4. Lộ trình theo giai đoạn

### Giai đoạn 0 — Chuẩn bị & xác minh phần cứng (2-3 ngày)
- [ ] Test Blue Pill: blink LED, kiểm tra ST-Link nạp code OK
- [ ] Đấu CAN transceiver cho STM32, đo thử tín hiệu CAN_H/CAN_L bằng đồng hồ VOM (không cần lên bus thật)
- [ ] Setup ESP32-S3 dev environment (ESP-IDF hoặc Arduino), test TWAI driver với ví dụ loopback
- [ ] Đọc datasheet STM32F103 phần Flash Memory + Reference Manual phần bxCAN

**Mốc hoàn thành:** 2 board chạy code cơ bản ổn định, đo được tín hiệu CAN.

---

### Giai đoạn 1 — Bootloader qua UART (1 tuần)
Làm trước qua UART để nắm chắc cơ chế flash/jump mà không phải lo giao thức CAN cùng lúc.

- [ ] Viết linker script riêng cho bootloader (FLASH origin = 0x08000000, size = 16K)
- [ ] Viết linker script riêng cho application (FLASH origin = 0x08004000, size = 48K)
- [ ] Bootloader: nhận file qua UART (giao thức đơn giản: STX - length - data - CRC - ETX)
- [ ] Implement flash erase (page 1KB) + flash write (`HAL_FLASH_Program`)
- [ ] Implement verify CRC32 sau khi ghi xong
- [ ] Implement `jump_to_app()` với `SCB->VTOR` remap đúng
- [ ] Application demo: chỉ cần blink LED khác pattern để phân biệt rõ đang chạy app, không phải bootloader
- [ ] Viết tool Python đơn giản gửi file qua UART (dùng `pyserial`)

**Mốc hoàn thành:** Nạp bootloader 1 lần qua ST-Link, sau đó có thể update application liên tục qua UART mà không cần ST-Link nữa.

**Rủi ro thường gặp:**
- Quên remap VTOR → app chạy loạn interrupt, hang máy khó hiểu
- Quên `HAL_FLASH_Unlock()` trước khi ghi → ghi flash thất bại âm thầm
- Application code compile với offset sai → nhảy vào app bị lỗi ngay lập tức

---

### Giai đoạn 2 — Thiết lập CAN bus (3-4 ngày)
- [ ] Cấu hình bxCAN trên STM32 (baudrate 500kbps, remap chân nếu cần)
- [ ] Cấu hình TWAI trên ESP32-S3 (cùng baudrate)
- [ ] Test gửi/nhận frame đơn giản 2 chiều (loopback thật giữa 2 board qua bus)
- [ ] Gắn điện trở termination 120Ω 2 đầu, đo trở kháng bus (~60Ω đo được là đúng)
- [ ] Debug bằng logic analyzer nếu có lỗi ACK/frame lỗi

**Mốc hoàn thành:** 2 board gửi/nhận CAN frame ổn định, không mất gói ở tốc độ 500kbps.

---

### Giai đoạn 3 — Thiết kế giao thức flash qua CAN (3-4 ngày)
Vì 1 CAN frame chỉ chứa 8 byte data, cần chia nhỏ file `.bin` thành nhiều frame (giống ISO-TP).

- [ ] Định nghĩa CAN ID cho từng loại message (request download, transfer data, transfer exit, ACK/NACK)
- [ ] Thiết kế multi-frame: First Frame (báo tổng độ dài) → Consecutive Frame (data từng đoạn) → Frame cuối kèm CRC
- [ ] Xử lý timeout: nếu không nhận được frame tiếp theo trong X ms → hủy transfer, báo lỗi
- [ ] Xử lý flow control cơ bản (STM32 báo "sẵn sàng nhận frame tiếp" để tránh ESP32 gửi dồn dập làm tràn buffer)

**Mốc hoàn thành:** Giao thức được viết ra giấy/document rõ ràng trước khi code (định nghĩa từng CAN ID, cấu trúc payload).

---

### Giai đoạn 4 — Tích hợp bootloader + CAN (1 tuần)
- [ ] Chuyển bootloader từ nhận qua UART sang nhận qua CAN (dùng lại toàn bộ logic flash/verify/jump ở Giai đoạn 1)
- [ ] ESP32-S3: viết firmware gateway, đọc file `.bin` từ SPIFFS/LittleFS, chia frame, gửi qua TWAI
- [ ] Test full flow: reset STM32 → vào chế độ chờ update → ESP32-S3 gửi firmware mới → STM32 flash + verify → jump vào app mới

**Mốc hoàn thành:** Update firmware thành công qua CAN từ đầu đến cuối, không cần ST-Link.

---

### Giai đoạn 5 — Hardening & an toàn (3-4 ngày)
- [ ] Xử lý tình huống mất điện giữa chừng khi đang ghi flash (kiểm tra flag "valid app" trước khi jump, nếu flash dở dang thì ở lại bootloader chờ update lại)
- [ ] Cơ chế vào bootloader thủ công (giữ nút nhấn khi reset)
- [ ] Log lỗi rõ ràng (qua UART debug hoặc LED pattern) khi CRC sai, timeout, hoặc flash write fail
- [ ] Test thử các trường hợp lỗi cố ý: rút nguồn giữa chừng, gửi file sai CRC, gửi file quá lớn so với vùng app

**Mốc hoàn thành:** Bootloader không bị "brick" (không thể phục hồi) trong các tình huống lỗi thường gặp.

---

### Giai đoạn 6 (tùy chọn) — Nâng cấp trải nghiệm (nếu còn thời gian)
- [ ] Web UI trên ESP32-S3: trang upload file `.bin` qua trình duyệt, hiển thị % tiến độ flash
- [ ] Màn OLED trên STM32 hiển thị trạng thái: "waiting" / "flashing X%" / "done" / "error"
- [ ] Thêm 1-2 service ID kiểu UDS thật (0x34, 0x36, 0x37) để giống chuẩn ngành hơn

---

## 5. Timeline tổng quan (ước lượng ~4-5 tuần, làm bán thời gian)

| Tuần | Nội dung |
|---|---|
| 1 | Giai đoạn 0 + Giai đoạn 1 (bootloader UART) |
| 2 | Giai đoạn 2 + Giai đoạn 3 (CAN bus + thiết kế giao thức) |
| 3 | Giai đoạn 4 (tích hợp CAN bootloader) |
| 4 | Giai đoạn 5 (hardening) |
| 5 (nếu còn) | Giai đoạn 6 + quay video demo + viết README |

---

## 6. Checklist sản phẩm cuối cùng (để show CV)

- [ ] Git repo public, code chia rõ thư mục `bootloader/`, `application/`, `gateway_esp32/`
- [ ] README: sơ đồ kiến trúc, hướng dẫn build/flash, giải thích giao thức CAN tự thiết kế
- [ ] Sơ đồ đấu nối phần cứng (có thể vẽ tay chụp ảnh hoặc dùng Fritzing/KiCad)
- [ ] Video demo 2-3 phút: reset board → update firmware qua CAN → app mới chạy
- [ ] Document riêng mô tả giao thức (CAN ID table, cấu trúc frame) — thể hiện tư duy thiết kế, không chỉ code

---

## 7. Tài liệu tham khảo nên đọc

- STM32F103 Reference Manual (RM0008) — phần Flash memory & bxCAN
- AN2606 (STM32 bootloader application note) — tham khảo cách ST tự thiết kế bootloader chuẩn
- ESP-IDF TWAI driver documentation
- ISO 15765-2 (ISO-TP) — tham khảo cách multi-frame CAN transport hoạt động (không cần implement đúng 100%, hiểu ý tưởng là đủ)
