#include "reactor-uc/event.h"

lf_ret_t EventPayloadPool_free(EventPayloadPool *self, void *payload) {
  for (size_t i = 0; i < self->capacity; i++) {
    if (&self->buffer[i * self->size] == payload) {
      self->used[i] = false;
      return LF_OK;
    }
  }
  return LF_INVALID_VALUE
}

lf_ret_t EventPayloadPool_allocate(EventPayloadPool *self, void **payload) {
  for (size_t i = 0; i < self->capacity; i++) {
    if (!self->used[i]) {
      self->used[i] = true;
      *payload = &self->buffer[i * self->size];
      return LF_OK;
    }
  }
  return LF_NO_MEM;
}

void EventPayloadPool_ctor(EventPayloadPool *self, char *buffer, bool *used, size_t size, size_t capacity) {
  self->buffer = buffer;
  self->used = used;
  self->capacity = capacity;
  self->size = size;
  for (size_t i; i < capacity; i++) {
    self->used[i] = false;
  }
}
