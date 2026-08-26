# 📝 Ghi Chú: CRC32 trong STM32 Bootloader Project

---

## 1. Vấn đề CRC giải quyết

Khi truyền dữ liệu qua UART (dễ nhiễu), cần một cách để bên nhận tự kiểm tra dữ liệu còn nguyên vẹn hay không mà không cần bản gốc để so sánh trực tiếp. CRC tạo ra 1 "vân tay" 32-bit từ dữ liệu — chỉ cần 1 bit đổi, vân tay này đổi khác hẳn.

---

## 2. Lý thuyết toán học

### 2.1 Coi dữ liệu như đa thức

Một dãy bit `1011` được coi là đa thức: `1·x³ + 0·x² + 1·x¹ + 1·x⁰ = x³ + x + 1`

### 2.2 Phép cộng/trừ là XOR (GF(2))

CRC hoạt động trong **GF(2)** (Galois Field bậc 2) — cộng và trừ đều là XOR, không có "nhớ"/"mượn" như số học thường.

### 2.3 CRC = phần dư của phép chia đa thức

```
Dữ liệu (đã nối thêm n bit 0, n = bậc polynomial) ÷ Polynomial = Thương số + PHẦN DƯ
                                                                    ↑
                                                              Đây là CRC
```

### 2.4 Thuật toán bit-by-bit (cách CPU/hardware thực thi)

```
Khởi tạo thanh ghi 32-bit = Init value
Với mỗi byte dữ liệu:
    XOR byte vào 8 bit cao nhất của thanh ghi
    Lặp 8 lần (1 lần / bit):
        Nhìn bit cao nhất (MSB) hiện tại
        Dịch trái thanh ghi 1 bit
        Nếu bit vừa nhìn (trước khi dịch) là 1 → XOR thanh ghi với Polynomial
Giá trị còn lại trong thanh ghi = CRC
```

**Ví dụ tay đã làm:** Data `110101`, Polynomial `10011` (CRC-4) → kết quả CRC = `0000` (data chia hết tuyệt đối cho polynomial, không dư).

---

## 3. Bốn tham số biến thể — nguồn gốc mọi nhầm lẫn

| Tham số | Ý nghĩa |
|---|---|
| **Init** | Giá trị khởi tạo thanh ghi (VD `0xFFFFFFFF`) |
| **RefIn** | Có đảo bit từng byte đầu vào không |
| **RefOut** | Có đảo bit toàn bộ kết quả cuối không |
| **XorOut** | Có XOR kết quả cuối với giá trị cố định không |

### CRC-32 chuẩn (IEEE 802.3 — Ethernet, `zlib.crc32()`)
```
Polynomial = 0x04C11DB7 | Init = 0xFFFFFFFF | RefIn = True | RefOut = True | XorOut = 0xFFFFFFFF
```

### CRC-32/MPEG-2 (STM32 hardware CRC unit)
```
Polynomial = 0x04C11DB7 | Init = 0xFFFFFFFF | RefIn = False | RefOut = False | XorOut = 0x00000000
```

> ⚠️ **Không được dùng `zlib.crc32()` của Python để đối chiếu với STM32** — 2 biến thể khác nhau, cùng polynomial nhưng khác RefIn/RefOut/XorOut → kết quả khác hoàn toàn dù dữ liệu giống hệt.

### Vì sao STM32F1 không cho tùy chỉnh
Mạch CRC là phần cứng cố định, chỉ chạy đúng 1 biến thể (MPEG-2). Các dòng cao hơn (F4/F7) mới có thanh ghi cho phép cấu hình RefIn/RefOut.

---

## 4. Implementation trên STM32

### 4.1 Bật CRC peripheral (CubeMX)
```
Pinout & Configuration → Computing → CRC → tick "Activated" → Generate Code
```
CubeMX tự sinh: `CRC_HandleTypeDef hcrc;`, hàm `MX_CRC_Init()`, thêm file HAL CRC vào Makefile.

### 4.2 API HAL quan trọng

| Hàm | Công dụng |
|---|---|
| `HAL_CRC_Init(&hcrc)` | Khởi tạo, tự động bật clock |
| `HAL_CRC_Calculate(&hcrc, buf, len)` | Tính CRC 1 lần, **tự reset trước khi tính** — KHÔNG dùng cho running CRC |
| `HAL_CRC_Accumulate(&hcrc, buf, len)` | Cộng dồn tiếp từ giá trị hiện tại — dùng cho running CRC |
| `__HAL_CRC_DR_RESET(&hcrc)` | Reset thanh ghi về `0xFFFFFFFF` |
| `hcrc.Instance->DR` | Đọc trực tiếp giá trị CRC hiện tại (không có hàm HAL riêng để đọc) |

**Lưu ý:** `HAL_CRC_Accumulate()`/`HAL_CRC_Calculate()` nhận độ dài tính theo **word (4 byte)**, không phải byte.

### 4.3 API tự thiết kế (`crc32.h`)

```c
void CRC32_Reset(void);
uint32_t CRC32_FeedData(uint8_t *data, uint32_t len);
uint32_t CRC32_GetCurrentValue(void);
```

---

## 5. 🐛 BUG đã gặp và cách sửa — Byte Order (Endianness)

### Hiện tượng
Code ban đầu:
```c
HAL_CRC_Accumulate(&hcrc, (uint32_t*)data, fullwords);  // ❌ SAI
```
Test với `data = [0x01, 0x02, 0x03, 0x04]` → kết quả STM32 **không khớp** với Python.

### Nguyên nhân
ARM Cortex-M3 là kiến trúc **little-endian**. Khi ép kiểu `(uint32_t*)data` trực tiếp, 4 byte trong bộ nhớ `[0x01][0x02][0x03][0x04]` (địa chỉ tăng dần) bị đọc thành số:
```
word = 0x04030201   (byte đầu = LSB, byte cuối = MSB — ngược ý muốn)
```
Trong khi ý định (và cách thuật toán CRC xử lý) cần byte **đầu tiên** là byte **cao nhất** (MSB):
```
word mong muốn = 0x01020304
```

→ Hardware CRC xử lý nhầm thứ tự byte thành `[04, 03, 02, 01]` thay vì `[01, 02, 03, 04]`.

### Cách sửa — tự ghép word bằng dịch bit, không ép kiểu con trỏ

```c
uint32_t word = ((uint32_t)data[i+0] << 24) |
                 ((uint32_t)data[i+1] << 16) |
                 ((uint32_t)data[i+2] << 8)  |
                 ((uint32_t)data[i+3]);
HAL_CRC_Accumulate(&hcrc, &word, 1);
```

Việc tự dịch bit chủ động chọn đúng vị trí từng byte, không phụ thuộc vào cách CPU tự diễn giải bộ nhớ theo native endianness.

### Cách debug đã dùng (phương pháp, không chỉ đáp án)
1. Viết hàm Python độc lập tính CRC-32/MPEG-2 đúng chuẩn (thuật toán bit-by-bit, tự viết, không dùng `zlib`)
2. Lấy giá trị STM32 qua debugger (Cortex-Debug + OpenOCD, breakpoint + Watch)
3. So sánh 2 giá trị — lệch nhau
4. Đặt giả thuyết cụ thể (byte bị đảo ngược) → tính thử CRC với thứ tự byte đảo ngược bằng Python → khớp với giá trị STM32 → xác nhận đúng nguyên nhân
5. Sửa code theo đúng chẩn đoán → test lại → khớp

---

## 6. Xử lý phần dư (khi `len` không chia hết 4)

### Vấn đề
STM32 CRC hardware chỉ nhận theo word (4 byte). Gói cuối cùng của file firmware thường không tròn kích thước packet (VD dư 2-3 byte lẻ).

### Giải pháp: Pad thêm `0x00` cho đủ word

```c
uint32_t fullwords = len / 4;
uint32_t remaining_bytes = len % 4;

// Xử lý phần chia hết trước (mỗi word ghép bằng dịch bit)
for (uint32_t w = 0; w < fullwords; w++) {
    uint32_t word = ((uint32_t)data[w*4+0] << 24) | ((uint32_t)data[w*4+1] << 16) |
                     ((uint32_t)data[w*4+2] << 8)  | ((uint32_t)data[w*4+3]);
    HAL_CRC_Accumulate(&hcrc, &word, 1);
}

// Xử lý phần dư — pad 0x00 vào cuối
if (remaining_bytes > 0) {
    uint8_t padded[4] = {0, 0, 0, 0};
    for (uint32_t i = 0; i < remaining_bytes; i++) {
        padded[i] = data[fullwords * 4 + i];
    }
    uint32_t word = ((uint32_t)padded[0] << 24) | ((uint32_t)padded[1] << 16) |
                     ((uint32_t)padded[2] << 8)  | ((uint32_t)padded[3]);
    HAL_CRC_Accumulate(&hcrc, &word, 1);
}
```

> ⚠️ **QUAN TRỌNG cho `tools/bin_checksum_patcher.py` sau này:** khi tính Total CRC32 của file `.bin` gốc bằng Python, phải **pad `0x00` vào cuối theo đúng cách này** nếu tổng độ dài file không chia hết 4. Nếu Python và C xử lý phần dư khác nhau (dù cùng thuật toán CRC), 2 bên sẽ ra kết quả khác nhau — **đây không phải lỗi CRC, mà là lỗi "quy ước xử lý phần dư không nhất quán"**.

---

## 7. Kết quả verify cuối cùng

| Test case | Data | Python | STM32 | Kết quả |
|---|---|---|---|---|
| Chia hết 4 | `[01,02,03,04]` | `0x793737CD` | `0x793737CD` | ✅ Match |
| Có phần dư | `[01,02,03,04,05,06]` (pad `00,00`) | `0x05CC1E1A` | `0x05CC1E1A` | ✅ Match |

---

## 8. Bài học rút ra (quan trọng để không quên)

1. **CRC là thuật toán, hardware CRC unit là 1 mạch chuyên chạy sẵn thuật toán đó** — "tự viết bootloader" là tự thiết kế logic gọi/dùng mạch này, không nhất thiết phải tự code lại thuật toán bit-by-bit bằng software.
2. **Luôn cẩn thận byte order khi ép kiểu con trỏ trên kiến trúc little-endian** — đây là lỗi rất phổ biến khi làm việc với dữ liệu dạng byte-stream nhưng xử lý bằng word.
3. **Mọi quyết định "tự chọn" (không phải chuẩn CRC bắt buộc) như cách pad phần dư, phải nhất quán tuyệt đối giữa mọi thành phần hệ thống** (ở đây là giữa code C trên STM32 và code Python trên host) — sai 1 trong 2 bên là toàn bộ hệ thống không khớp dù logic từng bên đều đúng.
4. **Phương pháp debug khoa học:** khi 2 kết quả lệch nhau, đặt giả thuyết cụ thể về nguyên nhân, dùng công cụ độc lập (ở đây là Python) để kiểm chứng giả thuyết trước khi sửa code — tránh sửa mò.