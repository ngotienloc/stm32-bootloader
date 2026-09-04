#ifndef __PARSER_H
#define __PARSER_H

#include "protocol.h"

typedef enum {
    STATE_WAIT_HEADER_1,
    STATE_WAIT_HEADER_2,
    STATE_WAIT_CMD,
    STATE_WAIT_LEN_H,
    STATE_WAIT_LEN_L,
    STATE_WAIT_PAYLOAD,
    STATE_WAIT_CRC,
    STATE_WAIT_TAIL
} ParserState_t;

void Parser_Init(void);
bool Parser_ParseByte(uint8_t byte, Protocol_Packet_t *packet);

#endif /* __PARSER_H */
