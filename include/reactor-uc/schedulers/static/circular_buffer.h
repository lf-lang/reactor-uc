#ifndef CircularBuffer_H
#define CircularBuffer_H

#include <stdlib.h>
#include <string.h>

typedef struct CircularBuffer
{
    void *buffer;     // data buffer
    void *buffer_end; // end of data buffer
    size_t capacity;  // maximum number of items in the buffer
    size_t count;     // number of items in the buffer
    size_t sz;        // size of each item in the buffer
    void *head;       // pointer to head
    void *tail;       // pointer to tail
} CircularBuffer;

void cb_init(CircularBuffer *cb, size_t capacity, size_t sz);
void cb_free(CircularBuffer *cb);
void cb_push_back(CircularBuffer *cb, const void *item);
void cb_pop_front(CircularBuffer *cb, void *item);
void cb_remove_front(CircularBuffer *cb);
void* cb_peek(CircularBuffer *cb);
void cb_dump_events(CircularBuffer *cb);

#endif