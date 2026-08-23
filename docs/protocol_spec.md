# UART Bootloader Protocol Specification (UART-BLP v1.0)

## 1. Overview
The UART Bootloader Protocol (UART-BLP) is a lightweight, reliable packet-based transport layer designed for reprogramming STM32 microcontrollers over standard UART / USART interfaces from an ESP32-S3 Gateway or Host PC.

---

## 2. Packet Framing Format

All communication frames follow a structured packet layout with 32-bit hardware CRC verification:

```
+---------------+---------------+---------------+---------------+---------------+---------------+
| Header (2B)   | Command (1B)  | Length (2B)   | Payload (N B) | CRC32 (4B)    | Tail (1B)     |
| 0xAA 0x55     | CMD_TYPE      | Big-Endian    | Data bytes    | CRC-32/MPEG-2 | 0x0D          |
+---------------+---------------+---------------+---------------+---------------+---------------+
```

### Field Descriptions:
- **Header (`0xAA 0x55`):** 2 fixed synchronization bytes.
- **Command / Response ID (`1 Byte`):** Specifies request or response type. Requests use `0x00..0x7F`, responses use `0x80..0xFF`.
- **Length (`2 Bytes`, Big-Endian):** Total byte length $N$ of the payload field ($0 \le N \le 1024$). For a `CMD_WRITE_DATA` packet with 256-byte firmware chunk, $N = 4\text{B (Offset)} + 256\text{B (Chunk)} = 260\text{ bytes}$ (`0x0104`).
- **Payload (`N Bytes`):** Parameters, commands, or raw firmware chunks.
- **CRC32 (`4 Bytes`, Big-Endian):** Non-reflected `CRC-32/MPEG-2` (Polynomial `0x04C11DB7`, Init `0xFFFFFFFF`, no input/output reversal, no final XOR), natively computed by STM32's hardware CRC unit.
- **Tail (`0x0D`):** End-of-frame delimiter (`\r`).

---

## 3. Command & Response Definitions

| Command Code | Name | Source $\rightarrow$ Destination | Description |
|---|---|---|---|
| `0x00` | `CMD_START_UPDATE` | ESP32-S3 $\rightarrow$ STM32 | Send Total Size [4B] + Total Expected CRC32 [4B] to init update session |
| `0x80` | `RESP_START_ACK` | STM32 $\rightarrow$ ESP32-S3 | Acknowledge session init & signal readiness for Pass 1 pre-verification |
| `0x01` | `CMD_PING` | ESP32-S3 $\rightarrow$ STM32 | Query target MCU status & active mode |
| `0x81` | `RESP_PONG` | STM32 $\rightarrow$ ESP32-S3 | Returns mode (`0x01`: Bootloader, `0x02`: App), Version, Valid App flag |
| `0x02` | `CMD_ERASE` | ESP32-S3 $\rightarrow$ STM32 | Manual / debug erase of application flash (`0x08004400`..`0x08010000`). *(Automated Pass 2 erases internally)* |
| `0x82` | `RESP_ERASE` | STM32 $\rightarrow$ ESP32-S3 | Flash erase status (`0x00`: Success, `0x01`: Error) |
| `0x03` | `CMD_WRITE_DATA` | ESP32-S3 $\rightarrow$ STM32 | Write binary chunk (`Flash Offset [4B]` + `Length [2B]` + `Chunk Data [NB]`) |
| `0x83` | `RESP_WRITE_ACK` | STM32 $\rightarrow$ ESP32-S3 | Chunk write result (`0x00`: ACK, `0x01`: NACK / CRC error, `0x02`: Flash error) |
| `0x04` | `CMD_VERIFY_CRC` | ESP32-S3 $\rightarrow$ STM32 | Trigger hardware CRC32 check on programmed flash |
| `0x84` | `RESP_VERIFY` | STM32 $\rightarrow$ ESP32-S3 | Verification result (`0x00`: Match OK, `0x01`: Mismatch) |
| `0x05` | `CMD_JUMP_APP` | ESP32-S3 $\rightarrow$ STM32 | Command bootloader to branch to Application vector table (`0x08004400`) |
| `0x06` | `CMD_END_PASS1` | ESP32-S3 $\rightarrow$ STM32 | Signal end of Pass 1 stream & trigger running CRC comparison |
| `0x86` | `RESP_PASS1_RESULT` | STM32 $\rightarrow$ ESP32-S3 | Pass 1 result (`0x00`: MATCH — proceed to Pass 2; `0x01`: MISMATCH — abort) |

---

## 4. Detailed Packet Payload Structures

### 4.1 `CMD_START_UPDATE` (0x00) & `RESP_START_ACK` (0x80)
- **`CMD_START_UPDATE` Payload (8 Bytes):**
  - Bytes [0..3]: `Total Firmware Size (uint32_t Big-Endian)`
  - Bytes [4..7]: `Total Expected CRC32 (uint32_t Big-Endian, CRC-32/MPEG-2)`
- **`RESP_START_ACK` Payload (2 Bytes):**
  - Byte 0: `Status` (`0x00` = READY_FOR_PREVERIFY, `0x01` = SIZE_OVERFLOW, `0x02` = BUSY)
  - Byte 1: `Accepted Chunk Size (e.g. 0x01 = 256B)`

---

### 4.2 `CMD_PING` (0x01) & `RESP_PONG` (0x81)
- **`CMD_PING` Payload:** 0 bytes.
- **`RESP_PONG` Payload (4 Bytes):**
  - Byte 0: `Device State` (`0x01` = Bootloader, `0x02` = Application)
  - Byte 1: `Bootloader Major Version`
  - Byte 2: `Bootloader Minor Version`
  - Byte 3: `Valid Application Present` (`0x01` = Valid, `0x00` = Invalid / Corrupt)

---

### 4.3 `CMD_ERASE` (0x02) & `RESP_ERASE` (0x82)
- **`CMD_ERASE` Payload (4 Bytes):**
  - Bytes [0..3]: `Total Firmware Size (uint32_t Big-Endian)`
- **`RESP_ERASE` Payload (2 Bytes):**
  - Byte 0: `Status` (`0x00` = SUCCESS, `0x01` = INVALID_SIZE, `0x02` = FLASH_LOCKED_OR_ERROR)
  - Byte 1: `Number of Pages Erased (uint8_t)`

---

### 4.4 `CMD_WRITE_DATA` (0x03) & `RESP_WRITE_ACK` (0x83)
- **`CMD_WRITE_DATA` Payload ($N = 4 + \text{data\_len}$ Bytes):**
  - Bytes [0..3]: `Flash Memory Offset (uint32_t Big-Endian)` relative to `APP_BASE_ADDRESS` (`0x08004400`)
  - Bytes [4..N-1]: `Binary payload chunk (e.g. 128, 256, or 512 bytes)`
- **`RESP_WRITE_ACK` Payload (5 Bytes):**
  - Byte 0: `Status` (`0x00` = ACK, `0x01` = NACK_CRC, `0x02` = FLASH_WRITE_FAIL)
  - Bytes [1..4]: `Acknowledged Flash Offset (uint32_t)`

> [!NOTE]
> In Pass 1, only bytes [4..N-1] (data chunk) are fed to the running CRC32 accumulator. The 4-byte offset is omitted from the image checksum.

---

### 4.5 `CMD_VERIFY_CRC` (0x04) & `RESP_VERIFY` (0x84)
- **`CMD_VERIFY_CRC` Payload (8 Bytes):**
  - Bytes [0..3]: `Total App Size (uint32_t)`
  - Bytes [4..7]: `Expected CRC-32/MPEG-2 (uint32_t)`
- **`RESP_VERIFY` Payload (5 Bytes):**
  - Byte 0: `Status` (`0x00` = MATCH_OK, `0x01` = CRC_MISMATCH)
  - Bytes [1..4]: `Calculated Flash CRC32 (uint32_t)`

---

### 4.6 `CMD_JUMP_APP` (0x05)
- **`CMD_JUMP_APP` Payload (1 Byte):**
  - Byte 0: `0x55` (Magic execution confirmation key)
- Target executes deinitialization, remaps `SCB->VTOR = 0x08004400`, updates MSP, and branches to Application entry point.

---

### 4.7 `CMD_END_PASS1` (0x06) & `RESP_PASS1_RESULT` (0x86)
- **`CMD_END_PASS1` Payload:** 0 bytes.
- **`RESP_PASS1_RESULT` Payload (5 Bytes):**
  - Byte 0: `Status` (`0x00` = MATCH_PROCEED_PASS2, `0x01` = CRC_MISMATCH_ABORT)
  - Bytes [1..4]: `Accumulated Running CRC32 (uint32_t)`

---

## 5. Timeout & Error Recovery Matrix

| Fault Condition | STM32 Action | ESP32-S3 / Host Action |
|---|---|---|
| UART Frame CRC32 Mismatch | Send `RESP_WRITE_ACK` with `0x01` (NACK) | Re-transmit current chunk up to 3 retries |
| Flash Write / Page Error | Send `RESP_WRITE_ACK` with `0x02` (ERROR) | Abort flashing session and alert operator |
| Per-Packet Timeout (> 500ms) | Internal UART buffer reset | Re-transmit current packet up to 3 retries |
| Session Inactivity Timeout (> 5000ms idle) | Reset internal receiver state to IDLE | Restart handshake starting from `CMD_PING` |
| Application Vector Invalid | Reject `CMD_JUMP_APP`, remain in Bootloader | Report corrupted binary and trigger reflash |
