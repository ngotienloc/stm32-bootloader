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

    if(fullwords >0) {
        crc = HAL_CRC_Accumulate(&hcrc, (uint32_t*)data, fullwords);
    }

    if(remaining_bytes > 0){
        uint8_t padded[4] = {0, 0, 0, 0};
        for(uint32_t i = 0 ; i < remaining_bytes; i++){
            padded[i] = data[fullwords *4 + i]; 
        }
        crc = HAL_CRC_Accumulate(&hcrc, (uint32_t*)padded, 1);
    }

    return hcrc.Instance->DR;
}

uint32_t CRC32_GetCurrentValue(void)
{
    return hcrc.Instance->DR;
}

