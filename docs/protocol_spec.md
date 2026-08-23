# UART Bootloader Protocol Specification (UART-BLP v1.0)

## 1. Overview
The UART Bootloader Protocol (UART-BLP) is a lightweight, reliable packet-based transport layer designed for reprogramming STM32 microcontrollers over standard UART / USART interfaces from an ESP32 Gateway or Host PC.

---

## 2. Packet Framing Format

All communication frames follow a structured packet layout with checksum verification:

```
+---------------+---------------+---------------+---------------+---------------+---------------+
| Header (2B)   | Command (1B)  | Length (2B)   | Payload (N B) | CRC16 (2B)    | Tail (1B)     |
| 0xAA 0x55     | CMD_TYPE      | Big-Endian    | Data bytes    | CRC-16-CCITT  | 0x0D          |
+---------------+---------------+---------------+---------------+---------------+---------------+
```

### Field Descriptions:
- **Header (`0xAA 0x55`):** 2 fixed synchronization bytes.
- **Command / Response ID (`1 Byte`):** Specifies request or response type. Requests use `0x01..0x7F`, responses use `0x81..0xFF`.
- **Length (`2 Bytes`, Big-Endian):** Total byte length of the payload field ($0 \le N \le 1024$).
- **Payload (`N Bytes`):** Parameters, commands, or raw firmware chunks.
- **CRC16 (`2 Bytes`, Big-Endian):** CRC-16-CCITT (Polynomial `0x1021`, Init `0xFFFF`) calculated over `[Command + Length + Payload]`.
- **Tail (`0x0D`):** End-of-frame delimiter (`\r`).

---

## 3. Command & Response Definitions

| Command Code | Name | Source $\rightarrow$ Destination | Description |
|---|---|---|---|
| `0x01` | `CMD_PING` | ESP32 / Host $\rightarrow$ STM32 | Query target MCU status & active mode |
| `0x81` | `RESP_PONG` | STM32 $\rightarrow$ ESP32 / Host | Returns mode (`0x01`: Bootloader, `0x02`: App), Version, Valid App flag |
| `0x02` | `CMD_ERASE` | ESP32 / Host $\rightarrow$ STM32 | Erase application flash region |
| `0x82` | `RESP_ERASE` | STM32 $\rightarrow$ ESP32 / Host | Flash erase status (`0x00`: Success, `0x01`: Error) |
| `0x03` | `CMD_WRITE_DATA` | ESP32 / Host $\rightarrow$ STM32 | Write binary chunk (`Flash Offset [4B]` + `Chunk Data [NB]`) |
| `0x83` | `RESP_WRITE_ACK` | STM32 $\rightarrow$ ESP32 / Host | Chunk write result (`0x00`: ACK, `0x01`: NACK / CRC error, `0x02`: Flash error) |
| `0x04` | `CMD_VERIFY_CRC` | ESP32 / Host $\rightarrow$ STM32 | Send expected CRC32 of full image |
| `0x84` | `RESP_VERIFY` | STM32 $\rightarrow$ ESP32 / Host | Verification result (`0x00`: Match OK, `0x01`: Mismatch) |
| `0x05` | `CMD_JUMP_APP` | ESP32 / Host $\rightarrow$ STM32 | Command bootloader to branch to Application vector table |

---

## 4. Detailed Packet Payload Structures

### 4.1 `CMD_PING` (0x01) & `RESP_PONG` (0x81)
- **`CMD_PING` Payload:** 0 bytes.
- **`RESP_PONG` Payload (4 Bytes):**
  - Byte 0: `Device State` (`0x01` = Bootloader, `0x02` = Application)
  - Byte 1: `Bootloader Major Version`
  - Byte 2: `Bootloader Minor Version`
  - Byte 3: `Valid Application Present` (`0x01` = Valid, `0x00` = Invalid / Corrupt)

---

### 4.2 `CMD_ERASE` (0x02) & `RESP_ERASE` (0x82)
- **`CMD_ERASE` Payload (4 Bytes):**
  - Bytes [0..3]: `Total Firmware Size (uint32_t Big-Endian)`
- **`RESP_ERASE` Payload (2 Bytes):**
  - Byte 0: `Status` (`0x00` = SUCCESS, `0x01` = INVALID_SIZE, `0x02` = FLASH_LOCKED_OR_ERROR)
  - Byte 1: `Number of Pages Erased (uint8_t)`

---

### 4.3 `CMD_WRITE_DATA` (0x03) & `RESP_WRITE_ACK` (0x83)
- **`CMD_WRITE_DATA` Payload (4 + N Bytes):**
  - Bytes [0..3]: `Flash Memory Offset (uint32_t Big-Endian)` relative to `APP_BASE_ADDRESS` (`0x08004000`)
  - Bytes [4..4+N-1]: `Binary payload chunk (e.g. 128, 256, or 512 bytes)`
- **`RESP_WRITE_ACK` Payload (5 Bytes):**
  - Byte 0: `Status` (`0x00` = ACK, `0x01` = NACK_CRC, `0x02` = FLASH_WRITE_FAIL)
  - Bytes [1..4]: `Acknowledged Flash Offset (uint32_t)`

---

### 4.4 `CMD_VERIFY_CRC` (0x04) & `RESP_VERIFY` (0x84)
- **`CMD_VERIFY_CRC` Payload (8 Bytes):**
  - Bytes [0..3]: `Total App Size (uint32_t)`
  - Bytes [4..7]: `Expected IEEE 802.3 CRC32 (uint32_t)`
- **`RESP_VERIFY` Payload (5 Bytes):**
  - Byte 0: `Status` (`0x00` = MATCH_OK, `0x01` = CRC_MISMATCH)
  - Bytes [1..4]: `Calculated Flash CRC32 (uint32_t)`

---

### 4.5 `CMD_JUMP_APP` (0x05)
- **`CMD_JUMP_APP` Payload (1 Byte):**
  - Byte 0: `0x55` (Magic execution confirmation key)
- Target executes deinitialization, remaps `SCB->VTOR`, updates MSP, and branches to Application entry point.

---

## 5. Timeout & Error Recovery Matrix

| Fault Condition | STM32 Action | ESP32 / Host Action |
|---|---|---|
| UART Frame CRC16 Mismatch | Send `RESP_WRITE_ACK` with `0x01` (NACK) | Re-transmit current chunk up to 3 retries |
| Flash Write / Page Error | Send `RESP_WRITE_ACK` with `0x02` (ERROR) | Abort flashing session and alert operator |
| Session Timeout (> 5000ms idle) | Reset internal receiver state to IDLE | Restart handshake starting from `CMD_PING` |
| Application Vector Invalid | Reject `CMD_JUMP_APP`, remain in Bootloader | Report corrupted binary and trigger reflash |
