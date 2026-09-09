#!/usr/bin/env python3
import serial

try:
    s = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)
    s.reset_input_buffer()
    s.reset_output_buffer()
    s.write(b"HELLO")
    data = s.read(5)
    print("\n" + "="*40)
    print("KET QUA LOOPBACK:", data)
    if data == b"HELLO":
        print(" CH340 HOAT DONG TOT! TU GUI TU NHAN OK!")
    else:
        print(" CH340 KHONG NHAN DUOC DU LIEU! KIEM TRA JUMPER HOAC DAY!")
    print("="*40 + "\n")
    s.close()
except Exception as e:
    print("Loi:", e)
