#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define PROTOCOL_HEADER_1 0xAA 
#define PROTOCOL_HEADER_2 0x55
#define PROTOCOL_TAIL     0x0D

#define PROTOCOL_CHUNK_SIZE       256
#define PROTOCOL_MAX_PAYLOAD_SIZE (4 + PROTOCOL_CHUNK_SIZE)

#define PROTOCOL_JUMP_KEY 0x55

// Command IDs
typedef enum {
    CMD_START_UPDATE = 0x00, 
    CMD_PING         = 0x01,
    CMD_ERASE        = 0x02,
    CMD_WRITE_DATA   = 0x03,
    CMD_VERIFY_CRC   = 0x04,
    CMD_JUMP_APP     = 0x05,
    CMD_END_PASS1    = 0x06
} Protocol_Cmd_t; 

// Response IDs 
typedef enum {
    RESP_START_ACK    = 0x80,
    RESP_PONG         = 0x81,
    RESP_ERASE        = 0x82,
    RESP_WRITE_ACK    = 0x83,
    RESP_VERIFY       = 0x84,
    RESP_PASS1_RESULT = 0x86
} Protocol_Resp_t;

typedef enum { 
    STATUS_OK              = 0x00,
    STATUS_NACK_CRC        = 0x01,
    STATUS_ERR_FLASH       = 0x02,
    STATUS_ERR_SIZE        = 0x03,
    STATUS_ERR_INVALID_KEY = 0x04,
    STATUS_ERR_BUSY        = 0x05
} Protocol_Status_t;

typedef enum { 
    DEV_STATE_BOOTLOADER = 0x01,
    DEV_STATE_APP        = 0x02
} Protocol_DeviceState_t;

// Packet structure
typedef struct { 
    uint8_t cmd; 
    uint16_t length; 
    uint8_t payload[PROTOCOL_MAX_PAYLOAD_SIZE];
    uint32_t crc32; 
} Protocol_Packet_t;

#endif /* __PROTOCOL_H */