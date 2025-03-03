#include "reactor-uc/event.h"

static lf_ret_t EventPayloadPool_free(EventPayloadPool *self, void *payload) {
  for (size_t i = 0; i < self->capacity; i++) {
    if (&self->buffer[i * self->payload_size] == payload) {
      self->used[i] = false;
      self->num_allocated--;
      return LF_OK;
    }
  }
  return LF_INVALID_VALUE;
}

static lf_ret_t EventPayloadPool_allocate(EventPayloadPool *self, void **payload) {
  if (self->capacity == 0) {
    return LF_NO_MEM;
  }

  for (size_t i = 0; i < self->capacity; i++) {
    if (!self->used[i]) {
      self->used[i] = true;
      *payload = &self->buffer[i * self->payload_size];
      self->num_allocated++;
      return LF_OK;
    }
  }
  return LF_NO_MEM;
}

static lf_ret_t EventPayloadPool_allocate_with_reserved(EventPayloadPool *self, void **payload, size_t num_reserved) {
  if (self->num_allocated + num_reserved >= self->capacity) {
    return LF_NO_MEM;
  } else {
    return self->allocate(self, payload);
  }
}

void EventPayloadPool_ctor(EventPayloadPool *self, char *buffer, bool *used, size_t element_size, size_t capacity) {
  self->buffer = buffer;
  self->used = used;
  self->capacity = capacity;
  self->num_allocated = 0;
  self->payload_size = element_size;

  if (self->used != NULL) {
    for (size_t i = 0; i < capacity; i++) {
      self->used[i] = false;
    }
  }

  self->allocate = EventPayloadPool_allocate;
  self->allocate_with_reserved = EventPayloadPool_allocate_with_reserved;
  self->free = EventPayloadPool_free;
}
