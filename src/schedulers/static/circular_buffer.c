/**
 * @file CircularBuffer.c
 * @brief A circular buffer implementation from stack overflow
 * (https://stackoverflow.com/questions/827691/how-do-you-implement-a-circular-buffer-in-c) 
 * 
 * @copyright Copyright (c) 2024
 * 
 */

 #include "util.h"
 #include "reactor-uc/schedulers/static/circular_buffer.h"
 #include "reactor-uc/event.h"
 
void cb_init(CircularBuffer *cb, size_t capacity, size_t sz)
{
    cb->buffer = malloc(capacity * sz);
    if(cb->buffer == NULL) {
        // handle error
        printf("ERROR: Fail to allocate memory to circular buffer.\n");
        return;
    }
    cb->buffer_end = (char *)cb->buffer + capacity * sz;
    cb->capacity = capacity;
    cb->count = 0;
    cb->sz = sz;
    cb->head = cb->buffer;
    cb->tail = cb->buffer;
}
 
void cb_free(CircularBuffer *cb)
{
    free(cb->buffer);
    // clear out other fields too, just to be safe
}
 
void cb_push_back(CircularBuffer *cb, const void *item)
{
    if(cb->count == cb->capacity){
        printf("ERROR: Buffer is full. Some in-flight events will be overwritten!\n");
    }
    memcpy(cb->head, item, cb->sz);
    cb->head = (char*)cb->head + cb->sz;
    if(cb->head == cb->buffer_end)
        cb->head = cb->buffer;
    cb->count++;
}
 
void cb_pop_front(CircularBuffer *cb, void *item)
{
    if(cb->count == 0){
        // handle error
        printf("ERROR: Popping from an empty buffer!\n");
        return;
    }
    memcpy(item, cb->tail, cb->sz);
    cb->tail = (char*)cb->tail + cb->sz;
    if(cb->tail == cb->buffer_end)
        cb->tail = cb->buffer;
    cb->count--;
}
 
void cb_remove_front(CircularBuffer *cb)
{
    if(cb->count == 0){
        // handle error
        printf("ERROR: Removing from an empty buffer!\n");
        return;
    }
    cb->tail = (char*)cb->tail + cb->sz;
    if(cb->tail == cb->buffer_end)
        cb->tail = cb->buffer;
    cb->count--;
}
 
void* cb_peek(CircularBuffer *cb)
{
    if(cb->count == 0)
        return NULL;
    return cb->tail;
}
 
void cb_dump_events(CircularBuffer *cb)
{
    printf("*** Dumping Events ***\n");
    void *p = cb->tail;
    if (cb->count > 0) {
        do {
            Event* e = (Event*)p;
            printf("Event @ %lld w/ payload %p\n", e->tag.time, e->payload);
            p += cb->sz;
            if (p == cb->buffer_end) p = cb->buffer;
        } while (p != cb->head);
    } else {
        printf("(Empty)\n");
    }
    printf("**********************\n");
}