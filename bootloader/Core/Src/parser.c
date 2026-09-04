#include "parser.h"
#include "crc32.h"

static ParserState_t current_state = STATE_WAIT_HEADER_1;

static Protocol_Packet_t rx_packet;

static uint16_t payload_index = 0;
static uint8_t crc_idx = 0;
static uint8_t crc_bytes[4];

bool Parser_ParseByte(uint8_t byte, Protocol_Packet_t *packet){
    switch (current_state) {
        case STATE_WAIT_HEADER_1:
            if (byte == PROTOCOL_HEADER_1) {
                current_state = STATE_WAIT_HEADER_2;
            }
            break;

        case STATE_WAIT_HEADER_2:
            if (byte == PROTOCOL_HEADER_2) {
                current_state = STATE_WAIT_CMD;
            } else {
                current_state = STATE_WAIT_HEADER_1; // Reset if not correct
            }
            break;

        case STATE_WAIT_CMD:
            rx_packet.cmd = byte;
            current_state = STATE_WAIT_LEN_H;
            break;

        case STATE_WAIT_LEN_H:
            rx_packet.length = (uint16_t)(byte << 8);
            current_state = STATE_WAIT_LEN_L;
            break;

        case STATE_WAIT_LEN_L:
            rx_packet.length |= byte;
            if (rx_packet.length > PROTOCOL_MAX_PAYLOAD_SIZE) {
                current_state = STATE_WAIT_HEADER_1;
                break;
            }
            payload_index = 0;
            current_state = (rx_packet.length > 0) ? STATE_WAIT_PAYLOAD : STATE_WAIT_CRC;
            break;

        case STATE_WAIT_PAYLOAD:
            rx_packet.payload[payload_index++] = byte;
            if (payload_index >= rx_packet.length) {
                crc_idx = 0;
                current_state = STATE_WAIT_CRC;
            }
            break;

        case STATE_WAIT_CRC:
            crc_bytes[crc_idx++] = byte;
            if (crc_idx >= 4) {
                rx_packet.crc32 = (crc_bytes[0] << 24) | (crc_bytes[1] << 16) | (crc_bytes[2] << 8) | crc_bytes[3];
                current_state = STATE_WAIT_TAIL;
            }
            break;

        case STATE_WAIT_TAIL:
            if (byte == PROTOCOL_TAIL) {
                CRC32_Reset();
                uint32_t calculated_crc = CRC32_FeedData(rx_packet.payload, rx_packet.length);

                if (calculated_crc == rx_packet.crc32) {
                    *packet = rx_packet;

                    current_state = STATE_WAIT_HEADER_1; 
                    return true; 
                }
            }
            current_state = STATE_WAIT_HEADER_1;
            break;

        default:
            current_state = STATE_WAIT_HEADER_1; 
    }
    return false; 
}

void Parser_Init(void) {
    current_state = STATE_WAIT_HEADER_1;
}