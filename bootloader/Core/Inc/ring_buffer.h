#ifndef __RING_BUFFER_H
#define __RING_BUFFER_H
#include <stdint.h>
#include <stdbool.h>

#define RING_BUFFER_SIZE 512 

typedef struct RingBuffer { 
    uint8_t buffer[RING_BUFFER_SIZE]; 
    volatile uint16_t head; 
    volatile uint16_t tail; 
} RingBuffer;

void RingBuffer_Init(struct RingBuffer *rb);
bool RingBuffer_Put(struct RingBuffer *rb, uint8_t data);
bool RingBuffer_Get(struct RingBuffer *rb, uint8_t *data);
uint16_t RingBuffer_Available(struct RingBuffer *rb);
bool RingBuffer_IsFull(struct RingBuffer *rb);
bool RingBuffer_IsEmpty(struct RingBuffer *rb);

#endif /* __RING_BUFFER_H */