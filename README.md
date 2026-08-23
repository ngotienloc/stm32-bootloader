# STM32 UART Bootloader & ESP32 Wireless Gateway

[![Target STM32](https://img.shields.io/badge/MCU-STM32F103C8T6%20(ARM%20Cortex--M3)-002B49?logo=stmicroelectronics)](https://www.st.com)
[![Gateway ESP32](https://img.shields.io/badge/Gateway-ESP32%20%2F%20ESP32--S3-E7352C?logo=espressif)](https://www.espressif.com)
[![Bus](https://img.shields.io/badge/Protocol-UART%20%2F%20Packet--Based-00599C)](https://en.wikipedia.org/wiki/Universal_asynchronous_receiver-transmitter)
[![Language](https://img.shields.io/badge/Language-C%20%7C%20Python%20%7C%20C%2B%2B-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

A robust, production-grade custom **UART Bootloader** for **STM32F103C8T6** (ARM Cortex-M3) paired with an **ESP32 Wireless Gateway**. This project enables reliable In-Application Programming (IAP) and Over-The-Air (OTA) firmware updates over a simple full-duplex UART interface (115200 - 921600 bps), eliminating the need for dedicated ST-Link/JTAG debuggers during field maintenance.

---

## Table of Contents
- [1. System Architecture](#1-system-architecture)
- [2. Key Features](#2-key-features)
- [3. Hardware Specifications & Pinout](#3-hardware-specifications--pinout)
- [4. Memory Map & Vector Table Relocation](#4-memory-map--vector-table-relocation)
- [5. UART Communication & Flashing Protocol (Packet CRC + ACK/NACK)](#5-uart-communication--flashing-protocol-packet-crc--acknack)
  - [5.1 Protocol Design Rationale: Raw vs. Protected Framing](#51-protocol-design-rationale-raw-transmission-vs-protected-framing)
  - [5.2 Packet Structure & Framing Format](#52-packet-structure--framing-format)
  - [5.3 Transmission Flow & ACK/NACK Auto-Retry Mechanism](#53-transmission-flow--acknack-auto-retry-mechanism)
  - [5.4 Command Set](#54-command-set)
  - [5.5 Engineering Highlights & Interview Value](#55-engineering-highlights--interview-value)
- [6. Fail-Safe & Anti-Bricking Mechanisms](#6-fail-safe--anti-bricking-mechanisms)
  - [6.1 Fail-Safe Rollback & Pre-Verification (Option B)](#61-fail-safe-rollback--pre-verification-flow-option-b)
  - [6.2 Additional Safety Guards](#62-additional-safety-guards)
- [7. Directory Structure](#7-directory-structure)
- [8. Getting Started & Build Guide](#8-getting-started--build-guide)
- [9. Testing & Verification](#9-testing--verification)
- [10. Roadmap](#10-roadmap)
- [11. License & References](#11-license--references)

---

## 1. System Architecture

The system decouples network communication and target execution:
1. **Host PC / Web Client:** Transmits the compiled `.bin` / `.hex` binary via Web UI, MQTT/HTTP OTA, or Python CLI.
2. **ESP32 Gateway:** Receives binary stream over Wi-Fi / Bluetooth / USB, chunks the payload into verified UART packets, manages flow control, and drives STM32 reset/boot pins if hardware-assisted boot is enabled.
3. **STM32F103 Target (Bootloader):** Receives structured UART frames, validates CRC32 / Checksums, flashes internal Flash pages (1 KB granularity), verifies programmed memory, and jumps execution to the User Application.

```
+------------------+         WiFi (HTTP / WebSockets / OTA)     +--------------------+
|  Host Machine    | -----------------------------------------> |   ESP32 Gateway    |
|  (PC / Web UI /  |         USB-CDC / Serial CLI               |  - Web OTA Server  |
|   Python Tool)   | <----------------------------------------> |  - UART Packetizer |
+------------------+                                            +--------------------+
                                                                           |
                                                             [ Direct 3.3V UART (TX/RX) ]
                                                             [ Optional: NRST / BOOT0   ]
                                                                           |
                                                                +--------------------+
                                                                | STM32F103 (Target) |
                                                                | - USART Peripheral |
                                                                | - Flash Controller |
                                                                | - VTOR Relocator   |
                                                                +--------------------+
```

---

## 2. Key Features

- **High-Speed & Simple Wiring:** Operates directly over standard 3.3V UART logic levels (115200 to 921600 bps) with just 2 communication wires (`TX`/`RX`) + `GND`. No extra transceivers or termination resistors required.
- **Packet-Based Frame Protocol:**
  - Robust framing with Header (`0xAA 0x55`), Packet Type, Length, Payload, and CRC16/CRC32.
  - Chunked binary transfer with configurable buffer size (128B, 256B, 512B, 1024B) for high throughput.
  - Strict ACK / NACK and Flow Control handshake.
- **Hardware-Level Vector Table Relocation:** Clean separation between Bootloader (`0x08000000`) and User Application (`0x08004000`) with dynamic `SCB->VTOR` remap and Main Stack Pointer (MSP) integrity verification prior to branching.
- **Dual Verification Engine:** Frame-level CRC16 check + Image-wide hardware CRC32 validation over programmed Flash pages.
- **Fail-Safe & Anti-Brick Boot Sequence:**
  - Application Header validation (checks initial MSP points to valid SRAM range `0x20000000 - 0x20005000` and Reset Vector points to valid Flash address `0x08004000 - 0x08010000`).
  - Emergency Boot Pin: Force bootloader mode if user button / jumper (`PA0`) is held low during reset.
  - Interrupted flash recovery: If power drops mid-write, the bootloader remains intact and waits for re-transmission.
- **Optional Hardware Flow & Reset Control:** ESP32 can optionally toggle `NRST` and `BOOT0` pins to automate entry into boot mode or system recovery without user intervention.

---

## 3. Hardware Specifications & Pinout

### 3.1 Components
| Item | Part / Model | Quantity | Notes |
|---|---|---|---|
| Target MCU | STM32F103C8T6 (Blue Pill) | 1 | 64 KB Flash, 20 KB SRAM, Cortex-M3 @ 72 MHz |
| Gateway MCU | ESP32 / ESP32-S3 / ESP8266 | 1 | Wi-Fi / BLE connectivity, Dual-core |
| Programmer | ST-Link V2 | 1 | Used once to flash the initial bootloader |
| Interconnect | Jumper wires | - | Female-to-Female / Breadboard |

### 3.2 Interconnection & Wiring

#### STM32F103 <-> ESP32 Direct UART Connection
| STM32 Pin | ESP32 GPIO | Function / Description |
|---|---|---|
| **PA9 (USART1_TX)** | **GPIO 16 (UART_RX)** | STM32 Transmit $\rightarrow$ ESP32 Receive |
| **PA10 (USART1_RX)**| **GPIO 17 (UART_TX)** | STM32 Receive $\leftarrow$ ESP32 Transmit |
| **3.3V** | **3.3V** | Logic Level / Power Supply |
| **GND** | **GND** | Common Ground (Mandatory) |
| **NRST** *(Optional)* | **GPIO 18** | ESP32-controlled Hardware Reset |
| **PA0 / BOOT0** *(Opt)* | **GPIO 19** | ESP32-controlled Force Bootloader Mode |
| **PC13** | - | On-board LED (Status Indicator) |

> [!IMPORTANT]
> Both STM32 and ESP32 operate at **3.3V logic levels**. Do NOT connect 5V UART signals directly to STM32 pins without level shifting.

---

## 4. Memory Map & Vector Table Relocation

The STM32F103C8T6 internal 64 KB Flash memory is partitioned into two regions:

```
0x08000000 +-----------------------------------------------+
           |                                               |
           |      Bootloader Firmware (16 KB)              |
           |      - USART Driver & Ring Buffer             |
           |      - Flash Page Writer / Eraser             |
           |      - Packet Parser & CRC Engine             |
           |                                               |
0x08004000 +-----------------------------------------------+ <--- Application Base Address
           |      App Vector Table (0x08004000)            |      (SCB->VTOR = 0x08004000)
           |      - Word 0: Initial Main Stack Pointer     |
           |      - Word 1: Reset Handler Address          |
           |      - Words 2-N: ISR Vectors                 |
           +-----------------------------------------------+
           |                                               |
           |      User Application Firmware (48 KB)        |
           |                                               |
0x08010000 +-----------------------------------------------+ <--- End of Flash (64 KB)
```

### Jump to Application Routine Sequence
Before transferring execution to the user application, the bootloader performs an atomic teardown:
1. **Disable all interrupts:** `__disable_irq();`
2. **De-initialize peripherals:** Reset USART, SysTick Timer, and GPIOs back to default reset states.
3. **Verify Valid Application:**
   ```c
   uint32_t app_msp = *(__IO uint32_t*)APP_START_ADDRESS;
   // Check if Stack Pointer falls within 20KB SRAM (0x20000000 to 0x20005000)
   if ((app_msp & 0x2FFE0000) == 0x20000000) {
       uint32_t app_reset_handler = *(__IO uint32_t*)(APP_START_ADDRESS + 4);
       pFunction app_entry = (pFunction)app_reset_handler;
       
       // Relocate Vector Table Offset Register
       SCB->VTOR = APP_START_ADDRESS;
       
       // Initialize Main Stack Pointer
       __set_MSP(app_msp);
       
       // Enable interrupts and jump
       __enable_irq();
       app_entry();
   }
   ```

---

## 5. UART Communication & Flashing Protocol (Packet CRC + ACK/NACK)

### 5.1 Protocol Design Rationale: Raw Transmission vs. Protected Framing

#### The Problem with Naive Raw Transmission (No Protection)
In basic UART bootloader implementations, streaming raw `.bin` bytes directly ("read byte, write byte") introduces critical reliability issues:
1. **EMI / Electrical Noise:** UART lines are susceptible to electrical interference, causing dropped or flipped bytes $\rightarrow$ STM32 silently writes corrupt data to Flash, bricking the device on boot.
2. **Zero Feedback:** The transmitter (ESP32/PC) cannot verify whether the STM32 received all bytes correctly or suffered buffer overrun.
3. **No Granular Recovery:** If transmission fails midway, the system cannot identify where the fault occurred, forcing a complete restart from byte 0.

#### Industry Standard Solution — Framed Packets with Protection
Instead of streaming an entire binary at once, firmware is split into small packets (e.g., **256 bytes / packet**), each protected by a structured frame:

```
[START_BYTE] [Packet_ID / CMD] [Length] [Data Payload...] [CRC32 / CRC16] [END_BYTE]
```

---

### 5.2 Packet Structure & Framing Format

```
+---------------+---------------+---------------+---------------+---------------+---------------+
| Header (2B)   | Command / ID  | Length (2B)   | Payload (N B) | CRC (2B/4B)   | Tail (1B)     |
| 0xAA 0x55     | CMD_TYPE / ID | Big-Endian    | Data bytes    | CRC16 / CRC32 | 0x0D          |
+---------------+---------------+---------------+---------------+---------------+---------------+
```

- **Header (`0xAA 0x55` / `START_BYTE`):** 2 synchronization bytes identifying frame start.
- **Packet_ID / Command (`1 Byte`):** Operation command code (e.g. `0x03` = `CMD_WRITE_DATA`).
- **Length (`2 Bytes`, Big-Endian):** Size of the payload field ($0 \le N \le 1024$ bytes, default 256B/packet).
- **Payload (`N Bytes`):** Target Flash memory offset (4B) + raw binary chunk.
- **CRC Checksum (`CRC16-CCITT` / `CRC32`):** Computed over `[Command + Length + Payload]` for bit error detection.
- **Tail (`0x0D` / `END_BYTE`):** Frame delimiter (`\r`).

---

### 5.3 Transmission Flow & ACK/NACK Auto-Retry Mechanism

Communication between ESP32 (Sender) and STM32 (Receiver) follows a strict synchronous handshake:

1. **ESP32 transmits Packet $N$:** Chunks 256 bytes with payload CRC.
2. **STM32 validates packet:** Computes CRC over received payload and compares with the frame's CRC field.
3. **Case 1 — CRC Match (Integrity OK):**
   - STM32 writes 256 bytes to Flash.
   - STM32 replies with **`ACK`** (`0x06` / `RESP_WRITE_ACK 0x00`).
   - ESP32 proceeds to transmit Packet $N+1$.
4. **Case 2 — CRC Mismatch (Noise Detected):**
   - STM32 discards the packet (no flash write).
   - STM32 replies with **`NACK`** (`0x15` / `RESP_WRITE_ACK 0x01`).
   - ESP32 **retransmits only Packet $N$** (no full restart needed).
5. **Case 3 — Lost Packet / Timeout:**
   - If ESP32 receives no response within $X$ ms (default 500 ms timeout), it automatically retransmits Packet $N$.
   - Retries up to **3 times** before aborting and reporting a fatal error.

#### Sequence Flow Diagram

```mermaid
sequenceDiagram
    autonumber
    participant G as ESP32 Gateway (Sender)
    participant T as STM32 Target (Receiver)

    Note over G,T: [Packet 1: Normal Transfer - ACK]
    G->>T: Packet #1 [Offset 0x0000, Data 256B, CRC]
    Note over T: Calc CRC == Packet CRC (Match!)<br/>Flash Write OK
    T-->>G: ACK (0x06 / RESP_WRITE_ACK OK)

    Note over G,T: [Packet 2: Line Noise Detected - NACK]
    G->>T: Packet #2 [Offset 0x0100, Data 256B, CRC]
    Note over T: Noise detected! Calc CRC != Packet CRC
    T-->>G: NACK (0x15 / RESP_WRITE_ACK CRC_ERROR)
    Note over G: Received NACK -> Resend ONLY Packet #2
    G->>T: Packet #2 [Offset 0x0100, Data 256B, CRC] (Retry 1)
    Note over T: CRC Matched! Flash Write OK
    T-->>G: ACK (0x06 / RESP_WRITE_ACK OK)

    Note over G,T: [Packet 3: Dropped Frame / Timeout]
    G->>T: Packet #3 [Offset 0x0200, Data 256B, CRC] (Lost)
    Note over G: Wait ACK/NACK (500ms Timeout expired)
    G->>T: Packet #3 [Offset 0x0200, Data 256B, CRC] (Retry 1 of 3)
    Note over T: CRC Matched! Flash Write OK
    T-->>G: ACK (0x06 / RESP_WRITE_ACK OK)
```

---

### 5.4 Command Set

| Command Code | Name | Direction | Description |
|---|---|---|---|
| `0x01` | `CMD_PING` | ESP32 $\rightarrow$ STM32 | Query target MCU status & active mode |
| `0x81` | `RESP_PONG` | STM32 $\rightarrow$ ESP32 | Target responds with status & bootloader version |
| `0x02` | `CMD_ERASE` | ESP32 $\rightarrow$ STM32 | Request erase of application flash sector |
| `0x82` | `RESP_ERASE` | STM32 $\rightarrow$ ESP32 | Flash erase result (Success / Error) |
| `0x03` | `CMD_WRITE_DATA` | ESP32 $\rightarrow$ STM32 | Binary chunk (Offset + Length + Data bytes) |
| `0x83` | `RESP_WRITE_ACK` | STM32 $\rightarrow$ ESP32 | Chunk write acknowledgment (`0x00`: ACK, `0x01`: NACK CRC, `0x02`: Flash Error) |
| `0x04` | `CMD_VERIFY_CRC` | ESP32 $\rightarrow$ STM32 | Send expected CRC32 for target Flash verification |
| `0x84` | `RESP_VERIFY` | STM32 $\rightarrow$ ESP32 | Image verification result (MATCH_OK / MISMATCH) |
| `0x05` | `CMD_JUMP_APP` | ESP32 $\rightarrow$ STM32 | Command bootloader to branch to Application |

---

### 5.5 Engineering Highlights & Interview Value

- **Industry Protocol Parallels:** Implements the same core principles found in proven industrial standards (packetization and ACK handshakes in **TCP**, CRC error detection in **Modbus RTU/CAN**, and block-by-block retransmission in **Xmodem / Ymodem**).
- **Bare-Metal Mastery:** Building the framing parser, state machine, CRC verification, and retry logic from scratch demonstrates low-level transport layer competence without relying on third-party black-box libraries.
- **Key Interview Takeaway:** Concrete answer to the classic embedded interview question: *"How do you guarantee firmware integrity and recover from transmission errors over a noisy serial interface?"*

---

## 6. Fail-Safe & Anti-Bricking Mechanisms

Designed specifically for resource-constrained microcontrollers (STM32F103 has 20KB RAM, insufficient to buffer an entire 40–50KB firmware image):

### 6.1 Fail-Safe Rollback & Pre-Verification Flow (Option B)

```
[Phase 1: Pre-Verify]  ESP32 streams file (SPIFFS) ──> STM32 Hardware Running CRC ──> Matches Total CRC32
                                                                                            │ (Set Ready Flag)
                                                                                            ▼
[Phase 2: Safe Write]  Set Flag "IN_PROGRESS" ──> Erase Flash ──> Write Data ──> Verify OK ──> Clear Flag
                                                       │ (Power Loss Interruption)
                                                       ▼
[Recovery on Reboot]   Bootloader detects "IN_PROGRESS" Flag ──> App Corrupt ──> Blink LED / Wait Reflash
```

1. **Header Handshake:** STM32 receives Header (`Total Size` + `Total CRC32`) and caches parameters in RAM.
2. **Streaming Pre-verification via Running CRC:**
   - Instead of buffering 40–50KB in 20KB RAM, target receives individual chunks (128B / 256B).
   - After verifying each packet CRC, data is fed incrementally into the **STM32 Hardware CRC Unit** (**Running / Incremental CRC**).
   - Once all packets arrive, accumulated CRC is compared against Header CRC32. If matched $\rightarrow$ set *"Ready to Write"* flag.
3. **Metadata "IN_PROGRESS" State Flag:**
   - ESP32 (storing cached image on SPIFFS/LittleFS) initiates Pass 2 for flash writing.
   - Before Flash Erase/Write, STM32 writes a state flag to Metadata sector: `FLAG = 0x55` (**IN_PROGRESS**).
   - Once all pages are written and full Flash CRC32 is verified $\rightarrow$ Clears flag: `FLAG = 0x00` (**COMPLETED**).
4. **Automatic Power-Loss Recovery:**
   - If power is cut during flashing $\rightarrow$ On reboot, Bootloader detects `FLAG == IN_PROGRESS`.
   - Bootloader knows the User Application is incomplete/corrupted $\rightarrow$ **Refuses to jump to App**, blinks `PC13` LED / logs UART alert, and safely remains in Bootloader mode awaiting reflash.

### 6.2 Additional Safety Guards
- **Stack Pointer (MSP) & Vector Table Check:** Validates initial MSP lies in valid SRAM (`0x20000000 - 0x20005000`) before jumping.
- **Emergency Hardware Boot Pin (`PA0`):** Pulling `PA0` low during reset forces bootloader execution regardless of flash state.
- **Session Timeout (5000ms):** Automatically resets state machine back to IDLE on communication stall.

---

## 7. Directory Structure

```
stm32-bootloader/
├── README.md                          # Project overview & documentation
├── docs/                              # Detailed specifications & schematics
│   ├── protocol_spec.md               # UART protocol framing & packet format
│   ├── hardware_schematic.png         # Wiring diagrams & pinouts
│   └── memory_layout.md               # Flash sector & linker configuration details
├── bootloader/                        # STM32 Bootloader Firmware (Target)
│   ├── Core/
│   │   ├── Inc/
│   │   │   ├── bootloader.h           # Jump, Flash & Protocol headers
│   │   │   ├── uart_driver.h          # USART configuration & ring buffer
│   │   │   └── crc32.h                # Hardware/Software CRC calculation
│   │   └── Src/
│   │       ├── bootloader.c           # State machine, Flash write/erase
│   │       ├── uart_driver.c          # Non-blocking UART driver
│   │       ├── crc32.c                # CRC32 calculation routines
│   │       └── main.c                 # Bootloader entry point & boot pin check
│   ├── STM32F103C8Tx_FLASH_BOOT.ld    # Linker script (16KB Flash allocation)
│   └── Makefile / CMakeLists.txt      # Build configuration
├── application/                       # STM32 Demo User Application (Target)
│   ├── Core/
│   │   └── Src/
│   │       └── main.c                 # Application logic (Blink LED / UART echo)
│   ├── STM32F103C8Tx_FLASH_APP.ld     # Linker script (Offset 0x08004000, 48KB)
│   └── Makefile / CMakeLists.txt      # Build configuration
├── gateway_esp32/                     # ESP32 Gateway Firmware
│   ├── main/
│   │   ├── uart_flasher.c             # ESP32 UART master packetizer
│   │   ├── wifi_ota_server.c          # Web server for browser-based .bin upload
│   │   └── main.c                     # Gateway controller entry
│   └── CMakeLists.txt                 # ESP-IDF build system
└── tools/                             # Host Deployment & Test Utilities
    ├── uart_uploader.py               # Python CLI direct UART flashing script
    ├── bin_checksum_patcher.py        # Pre-process binary, inject CRC32 header
    └── requirements.txt               # Python dependencies (pyserial)
```

---

## 8. Getting Started & Build Guide

### 8.1 Prerequisites
- **ARM GCC Toolchain:** `arm-none-eabi-gcc` (v10+), `make` or `ninja`, `openocd` or `stlink-tools`.
- **ESP32 Toolchain:** [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/) or PlatformIO / Arduino IDE.
- **Python Environment:** Python 3.9+ with `pyserial`.

### 8.2 Building the STM32 Bootloader
```bash
cd bootloader
make -j4
# Flash bootloader once via ST-Link
st-flash write build/bootloader.bin 0x08000000
```

### 8.3 Building the STM32 User Application
```bash
cd application
make -j4
# Output: build/application.bin
```

### 8.4 Building the ESP32 Gateway
```bash
cd gateway_esp32
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

---

## 9. Testing & Verification

1. **Direct PC-to-STM32 UART Flashing (via USB-TTL converter):**
   ```bash
   python3 tools/uart_uploader.py \
       --port /dev/ttyUSB0 \
       --baudrate 115200 \
       --file application/build/application.bin
   ```
2. **OTA Flash Update via ESP32 Web Interface:**
   - Connect to ESP32 Wi-Fi AP: `STM32-UART-Gateway`
   - Navigate to `http://192.168.4.1`
   - Select `application.bin` and click **Flash Target**.
   - Monitor live upload progress and status on Web UI.

---

## 10. Roadmap

- [x] Initial Architecture & Memory Map Design
- [ ] G0: Hardware verification (Direct UART loopback & Pin verification)
- [ ] G1: Minimal UART-based Bootloader with Flash write & Jump logic
- [ ] G2: Packet framing protocol with CRC16 & ACK/NACK
- [ ] G3: Python flashing tool (`uart_uploader.py`)
- [ ] G4: ESP32 Gateway firmware (Web OTA / Serial Forwarding)
- [ ] G5: Hardening against power drop & corrupted transmission
- [ ] G6: Web UI Dashboard on ESP32 & OLED status display

---

## 11. License & References

- **License:** Distributed under the MIT License. See `LICENSE` for details.
- **References:**
  - STM32F103xC/D/E Reference Manual (*RM0008*) — Flash Memory Controller & USART.
  - ST AN2606: *STM32 microcontroller system memory boot mode*.
  - ST AN3155: *USART protocol used in the STM32 bootloader*.
