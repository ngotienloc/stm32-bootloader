#include "crc32.h"

extern CRC_HandleTypeDef hcrc;

void CRC32_Reset(void)
{
  __HAL_CRC_DR_RESET(&hcrc);
}

uint32_t CRC32_FeedData(uint8_t *data, uint32_t len)
{  
    uint32_t fullwords = len / 4; 
    uint32_t remaining_bytes = len % 4; 

    for (uint32_t w = 0; w < fullwords; w++) {
        uint32_t word = ((uint32_t)data[w*4 + 0] << 24) |
                         ((uint32_t)data[w*4 + 1] << 16) |
                         ((uint32_t)data[w*4 + 2] << 8)  |
                         ((uint32_t)data[w*4 + 3]);
        HAL_CRC_Accumulate(&hcrc, &word, 1);
    }

    if (remaining_bytes > 0) {
        uint8_t padded[4] = {0, 0, 0, 0};
        for (uint32_t i = 0; i < remaining_bytes; i++) {
            padded[i] = data[fullwords * 4 + i]; 
        }
        uint32_t word = ((uint32_t)padded[0] << 24) |
                         ((uint32_t)padded[1] << 16) |
                         ((uint32_t)padded[2] << 8)  |
                         ((uint32_t)padded[3]);
        HAL_CRC_Accumulate(&hcrc, &word, 1);
    }

    return hcrc.Instance->DR;
}

uint32_t CRC32_GetCurrentValue(void)
{
    return hcrc.Instance->DR;
}

