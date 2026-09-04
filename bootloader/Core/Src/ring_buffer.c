#include "ring_buffer.h"

void RingBuffer_Init(struct RingBuffer *rb) {
    rb->head = 0; 
    rb->tail = 0;
}

bool RingBuffer_IsEmpty(struct RingBuffer *rb){
    return rb->head == rb->tail;
}

bool RingBuffer_IsFull(struct RingBuffer *rb){
    return ((rb->head + 1) % RING_BUFFER_SIZE) == rb->tail;
}

bool RingBuffer_Put(struct RingBuffer *rb, uint8_t data) { 
    if (RingBuffer_IsFull(rb)) {
        return false; 
    }
    rb->buffer[rb->head] = data; 
    rb->head = (rb->head + 1) % RING_BUFFER_SIZE;
    return true;
}

bool RingBuffer_Get(struct RingBuffer *rb, uint8_t *data) { 
    if (RingBuffer_IsEmpty(rb)) {
        return false; 
    }
    *data = rb->buffer[rb->tail]; 
    rb->tail = (rb->tail + 1) % RING_BUFFER_SIZE;
    return true;
}

uint16_t RingBuffer_Available(struct RingBuffer *rb){
    return (rb->head - rb->tail + RING_BUFFER_SIZE) % RING_BUFFER_SIZE;
}