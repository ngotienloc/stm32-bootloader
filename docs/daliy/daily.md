1. Reset Handler: 
- Chứa địa chỉ các lệnh / hàm thực thi chương trình. 
- Thực hiện: 
    - Bật CLock 
    - Copy .data từ flash sang SRAM 
    - Xóa phân vùng .bss trong SRAM
    - main()
2. Linker / Linker script: 
- Compiler biên dịch .c về .o  các file riêng biệt
- Linker đọc kịch bản liên kết ( Linker script ) và đặt vào địa chỉ thực tế. 
