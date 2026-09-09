#!/usr/bin/env python3
"""
Test script for STM32 UART Bootloader - CMD_PING / RESP_PONG
"""

import sys
import time
import argparse
import serial
import serial.tools.list_ports

PROTOCOL_HEADER = b"\xAA\x55"
PROTOCOL_TAIL   = b"\x0D"

CMD_PING  = 0x01
RESP_PONG = 0x81

DEV_STATE_BOOTLOADER = 0x01
DEV_STATE_APP        = 0x02

def crc32_mpeg2(data: bytes) -> int:
    """Hardware CRC-32/MPEG-2 algorithm matching STM32 hardware CRC."""
    if len(data) == 0:
        return 0xFFFFFFFF
    
    padded = bytearray(data)
    rem = len(padded) % 4
    if rem != 0:
        padded.extend(b"\x00" * (4 - rem))
    
    crc = 0xFFFFFFFF
    for i in range(0, len(padded), 4):
        word = (padded[i] << 24) | (padded[i+1] << 16) | (padded[i+2] << 8) | padded[i+3]
        crc ^= word
        for _ in range(32):
            if crc & 0x80000000:
                crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF
            else:
                crc = (crc << 1) & 0xFFFFFFFF
    return crc

def make_packet(cmd: int, payload: bytes = b"") -> bytes:
    """Build a framed packet: Header + CMD + Length + Payload + CRC32 + Tail."""
    length = len(payload)
    crc = crc32_mpeg2(payload)
    
    packet = bytearray()
    packet.extend(PROTOCOL_HEADER)
    packet.append(cmd)
    packet.append((length >> 8) & 0xFF)
    packet.append(length & 0xFF)
    if length > 0:
        packet.extend(payload)
    packet.append((crc >> 24) & 0xFF)
    packet.append((crc >> 16) & 0xFF)
    packet.append((crc >> 8) & 0xFF)
    packet.append(crc & 0xFF)
    packet.extend(PROTOCOL_TAIL)
    return bytes(packet)

def find_serial_port() -> str:
    """Auto-detect or list available serial ports."""
    all_ports = list(serial.tools.list_ports.comports())
    # Filter out virtual Linux ttyS ports (ttyS0..ttyS31)
    ports = [p for p in all_ports if "USB" in p.device or "ACM" in p.device]
    if not ports:
        ports = all_ports

    if not ports:
        print("❌ Không tìm thấy cổng COM / Serial nào!")
        print("   Vui lòng cắm mạch USB-UART vào máy tính rồi thử lại.")
        sys.exit(1)
    
    if len(ports) == 1:
        selected = ports[0].device
        print(f"🔌 Tự động nhận diện và kết nối cổng: {selected} ({ports[0].description})")
        return selected

    print("📋 Tìm thấy các cổng Serial sau:")
    for idx, p in enumerate(ports):
        print(f"  [{idx}] {p.device} - {p.description}")
    
    choice = input(f"Chọn cổng (0-{len(ports)-1}) [mặc định 0]: ").strip()
    idx = int(choice) if choice.isdigit() and int(choice) < len(ports) else 0
    return ports[idx].device

def main():
    parser = argparse.ArgumentParser(description="STM32 Bootloader Ping-Pong Tester")
    parser.add_argument("--port", "-p", help="Serial port (e.g. /dev/ttyUSB0)")
    parser.add_argument("--baud", "-b", type=int, default=115200, help="Baudrate (default: 115200)")
    parser.add_argument("--timeout", "-t", type=float, default=2.0, help="Read timeout in seconds")
    args = parser.parse_args()

    port = args.port if args.port else find_serial_port()

    print(f"\n🚀 Đang mở kết nối tới {port} @ {args.baud} bps...")
    try:
        ser = serial.Serial(port, args.baud, timeout=args.timeout)
    except Exception as e:
        print(f"❌ Không thể mở cổng {port}: {e}")
        print("💡 Mẹo Linux: Nếu lỗi Permission denied, chạy lệnh: sudo usermod -aG uucp $USER (hoặc dialout)")
        sys.exit(1)

    # Clear buffers
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    time.sleep(0.1)

    # 1. Prepare CMD_PING packet
    ping_packet = make_packet(CMD_PING)
    print(f"\n📤 Gửi gói CMD_PING ({len(ping_packet)} bytes):")
    print(f"   Hex: {ping_packet.hex(' ')}")

    ser.write(ping_packet)
    ser.flush()

    # 2. Wait for response
    print("⏳ Đang chờ phản hồi từ STM32...")
    start_time = time.time()
    
    # Read response header
    resp = ser.read(14) # Expected RESP_PONG length: 2 + 1 + 2 + 4 + 4 + 1 = 14 bytes
    elapsed = (time.time() - start_time) * 1000

    if not resp:
        print(f"❌ Hết thời gian chờ (Timeout {args.timeout}s)! Không nhận được phản hồi.")
        print("💡 Kiểm tra lại:")
        print("   1. Dây TX của USB-UART đã nối vào PA10 (RX1) của STM32 chưa?")
        print("   2. Dây RX của USB-UART đã nối vào PA9 (TX1) của STM32 chưa?")
        print("   3. ĐÃ NỐI CHUNG GND GIỮA 2 MẠCH CHƯA? (Rất quan trọng!)")
        print("   4. Đã nạp bootloader.bin và reset STM32 chưa?")
        ser.close()
        sys.exit(1)

    print(f"📥 Nhận được {len(resp)} bytes sau {elapsed:.1f} ms:")
    print(f"   Hex: {resp.hex(' ')}")

    # 3. Validate packet framing
    if len(resp) < 14:
        print(f"⚠️ Gói tin nhận về quá ngắn ({len(resp)} bytes, kỳ vọng 14 bytes)!")
        ser.close()
        sys.exit(1)

    if resp[:2] != PROTOCOL_HEADER:
        print(f"❌ Header không đúng: {resp[:2].hex(' ')} (Kỳ vọng: aa 55)")
        ser.close()
        sys.exit(1)

    cmd_resp = resp[2]
    if cmd_resp != RESP_PONG:
        print(f"⚠️ Nhận được mã phản hồi lạ: 0x{cmd_resp:02X} (Kỳ vọng: 0x81 RESP_PONG)")

    length = (resp[3] << 8) | resp[4]
    payload = resp[5:5+length]
    received_crc = (resp[5+length] << 24) | (resp[5+length+1] << 16) | (resp[5+length+2] << 8) | resp[5+length+3]
    tail = resp[5+length+4]

    # Calculate expected CRC
    calc_crc = crc32_mpeg2(payload)

    print("\n" + "="*50)
    print("           KẾT QUẢ TEST PING - PONG")
    print("="*50)
    print(f"  • Mã phản hồi      : 0x{cmd_resp:02X} (RESP_PONG)")
    print(f"  • Độ dài Payload   : {length} bytes")
    
    if length >= 4:
        state_str = "Bootloader (0x01)" if payload[0] == DEV_STATE_BOOTLOADER else f"Unknown (0x{payload[0]:02X})"
        major_ver = payload[1]
        minor_ver = payload[2]
        valid_app = "Có (Valid)" if payload[3] == 0x01 else "Chưa có / Hỏng (0x00)"
        
        print(f"  • Trạng thái MCU   : {state_str}")
        print(f"  • Phiên bản        : v{major_ver}.{minor_ver}")
        print(f"  • Ứng dụng Flash   : {valid_app}")

    print(f"  • CRC32 nhận được  : 0x{received_crc:08X}")
    print(f"  • CRC32 tính lại   : 0x{calc_crc:08X}")
    
    if received_crc == calc_crc and tail == 0x0D:
        print("  • Kiểm tra toàn vẹn: ✅ THÀNH CÔNG 100%! Gói tin hoàn hảo!")
        print("="*50)
        print("🎉 MILESTONE 1 HOÀN THÀNH XUẤT SẮC! Tầng UART và Parser chạy cực chuẩn!\n")
    else:
        print("  • Kiểm tra toàn vẹn: ❌ SAI CRC HOẶC TAIL!")
        print("="*50)

    ser.close()

if __name__ == "__main__":
    main()
