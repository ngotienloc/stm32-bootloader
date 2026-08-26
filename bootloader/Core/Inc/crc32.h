#ifndef __CRC32_H
#define __CRC32_H

#include "main.h"

void CRC32_Reset(void);
uint32_t CRC32_FeedData(uint8_t *data, uint32_t len);
uint32_t CRC32_GetCurrentValue(void);

#endif /* CRC32_H */