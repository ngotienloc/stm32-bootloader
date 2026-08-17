# CAN Bootloader Protocol Specification (CAN-BLP v1.0)

## 1. Overview
The CAN Bootloader Protocol (CAN-BLP) is a lightweight, reliable, ISO-TP/UDS inspired transport layer designed for reprogramming STM32 microcontrollers over standard CAN 2.0B networks (11-bit standard IDs).

---

## 2. CAN Frame Identifiers (Standard 11-bit)

| ID (Hex) | Acronym | Source | Destination | Purpose |
|---|---|---|---|---|
| `0x100` | `CMD_PING` | Gateway | Target STM32 | Node discovery & state inquiry |
| `0x101` | `RESP_PONG` | Target STM32 | Gateway | Current operational state & version |
| `0x110` | `CMD_ENTER_BOOT` | Gateway | Target STM32 | Force transition to Bootloader mode |
| `0x111` | `RESP_BOOT_ACK` | Target STM32 | Gateway | Status acknowledgment |
| `0x120` | `CMD_ERASE_APP` | Gateway | Target STM32 | Flash page erase command |
| `0x121` | `RESP_ERASE_ACK` | Target STM32 | Gateway | Flash erase completion result |
| `0x130` | `DATA_FIRST_FRAME` | Gateway | Target STM32 | Payload total byte count & target CRC32 |
| `0x131` | `DATA_CONSEC_FRAME`| Gateway | Target STM32 | Sequential binary stream packets |
| `0x132` | `FLOW_CONTROL` | Target STM32 | Gateway | Rate control & readiness feedback |
| `0x140` | `CMD_VERIFY_CRC` | Gateway | Target STM32 | Request complete memory verification |
| `0x141` | `RESP_VERIFY` | Target STM32 | Gateway | CRC match/mismatch status |
| `0x150` | `CMD_JUMP_APP` | Gateway | Target STM32 | Relocate vector table & boot app |

---

## 3. Frame Formats & Payload Structures

### 3.1 `CMD_PING` (0x100) & `RESP_PONG` (0x101)
- **CMD_PING Payload (0 Bytes or 1 Byte):**
  - Byte 0: `0xAA` (Magic query byte)
- **RESP_PONG Payload (4 Bytes):**
  - Byte 0: `Device State` (0x01 = Bootloader, 0x02 = Application)
  - Byte 1: `Major Version`
  - Byte 2: `Minor Version`
  - Byte 3: `Valid App Flag` (0x01 = Valid app present in Flash, 0x00 = No valid app)

---

### 3.2 `CMD_ERASE_APP` (0x120) & `RESP_ERASE_ACK` (0x121)
- **CMD_ERASE_APP Payload (4 Bytes):**
  - Bytes [0..3]: `Total Image Size (uint32_t Little-Endian)`
- **RESP_ERASE_ACK Payload (2 Bytes):**
  - Byte 0: `Status` (0x00 = SUCCESS, 0x01 = INVALID_SIZE, 0x02 = FLASH_ERROR)
  - Byte 1: `Pages Erased (uint8_t)`

---

### 3.3 Multi-Frame Data Transfer

#### Step 1: First Frame (`DATA_FIRST_FRAME` - 0x130)
- **DLC:** 8 Bytes
- **Payload:**
  - Bytes [0..3]: `Total Firmware Size (uint32_t)`
  - Bytes [4..7]: `Expected CRC32 (uint32_t IEEE 802.3)`

#### Step 2: Flow Control (`FLOW_CONTROL` - 0x132)
- **DLC:** 3 Bytes
- **Payload:**
  - Byte 0: `Flow Status`
    - `0x00`: **CTS** (Continue to send)
    - `0x01`: **WAIT** (Buffer full / writing to flash page)
    - `0x02`: **ABORT** (Sequence mismatch or timeout error)
  - Byte 1: `Block Size (BS)` (Number of consecutive frames before next Flow Control, e.g., 8 frames)
  - Byte 2: `Separation Time Minimum (STmin)` (Delay in ms between consecutive frames, e.g., 1 ms)

#### Step 3: Consecutive Frames (`DATA_CONSEC_FRAME` - 0x131)
- **DLC:** 2 to 8 Bytes
- **Payload:**
  - Byte 0: `Sequence Index (uint8_t)` (Incremented 0..255, wraps around)
  - Bytes [1..7]: `Raw Firmware Bytes (1 to 7 bytes chunk)`

---

### 3.4 Verification & Execution Branch

#### `CMD_VERIFY_CRC` (0x140) & `RESP_VERIFY` (0x141)
- **RESP_VERIFY Payload (5 Bytes):**
  - Byte 0: `Status` (0x00 = PASS, 0x01 = MISMATCH, 0x02 = UNALIGNED_ERROR)
  - Bytes [1..4]: `Calculated CRC32 on STM32 Flash (uint32_t)`

#### `CMD_JUMP_APP` (0x150)
- **Payload (1 Byte):**
  - Byte 0: `0x55` (Magic execute key)
- Target turns off bxCAN, de-initializes SysTick, resets registers, moves MSP, and executes user reset vector.

---

## 4. Timeout & Error Recovery Matrix

| Condition | Target Action | Gateway Action |
|---|---|---|
| Consecutive frame sequence dropped | Transmit `FLOW_CONTROL` with status `ABORT` (0x02) | Re-initialize transfer from last acknowledged page |
| Flash write error | Transmit `RESP_ERASE_ACK` or `FLOW_CONTROL` with `FLASH_ERROR` | Abort session, alert user via Web UI / Serial |
| Timeout (> 3000ms idle) | Reset internal protocol state machine to IDLE | Retry handshake or report connection loss |
| App reset vector invalid | Reject `CMD_JUMP_APP`, remain in Bootloader | Report corrupted binary to operator |
