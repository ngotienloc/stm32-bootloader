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

3. Tổ chức bộ nhớ thực tế: 
- .ins_vector 
- . text
- . rodata 
- . data: + _sidata ( RAM ) = VMA
          + _sdata (FLASH) = LMA 
- . bss 
- . heap 
- . stack

4. Phân trang: 
- 1 vùng nhớ ( PAGE = 1 KB ) của flash 
- Muốn sửa đổi dữ liệu trên 1 ô nhớ cần xóa toàn bộ page chứa ô nhớ đó 

5. Cơ chế bảo vệ bộ nhớ: 
- Muốn thao tác với flash cần mở khóa ( trong Unlock)


