#include "reactor-uc/event.h"

static lf_ret_t EventPayloadPool_free(EventPayloadPool *self, void *payload) {
  for (size_t i = 0; i < self->capacity; i++) {
    if (&self->buffer[i * self->payload_size] == payload) {
      self->used[i] = false;
      return LF_OK;
    }
  }
  return LF_INVALID_VALUE;
}

static lf_ret_t EventPayloadPool_allocate(EventPayloadPool *self, void **payload) {
  for (size_t i = self->reserved; i < self->capacity; i++) {
    if (!self->used[i]) {
      self->used[i] = true;
      *payload = &self->buffer[i * self->payload_size];
      return LF_OK;
    }
  }
  return LF_NO_MEM;
}

static lf_ret_t EventPayloadPool_allocate_reserved(EventPayloadPool *self, void **payload) {
  for (size_t i = 0; i < self->reserved; i++) {
    if (!self->used[i]) {
      self->used[i] = true;
      *payload = &self->buffer[i * self->payload_size];
      return LF_OK;
    }
  }
  return LF_NO_MEM;
}

void EventPayloadPool_ctor(EventPayloadPool *self, char *buffer, bool *used, size_t element_size, size_t capacity,
                           size_t reserved) {
  self->buffer = buffer;
  self->used = used;
  self->capacity = capacity;
  self->payload_size = element_size;
  self->reserved = reserved;

  if (self->used != NULL) {
    for (size_t i = 0; i < capacity; i++) {
      self->used[i] = false;
    }
  }

  self->allocate = EventPayloadPool_allocate;
  self->allocate_reserved = EventPayloadPool_allocate_reserved;
  self->free = EventPayloadPool_free;
}
