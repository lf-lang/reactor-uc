#include "reactor-uc/trigger_value.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>

void TriggerValue_push(TriggerValue *self, const void *value) {
  assert(self->empty || self->read_idx != self->write_idx);
  // FIXME: Make this cleaner.
  char *_buffer = (char *)self->buffer;
  memcpy(_buffer + self->write_idx * self->value_size, value, self->value_size);
  self->write_idx = (self->write_idx + 1) % self->capacity;
  self->empty = false;
}

void TriggerValue_pop(TriggerValue *self) {
  assert(!self->empty);
  self->read_idx = (self->read_idx + 1) % self->capacity;
  if (self->read_idx == self->write_idx) {
    self->empty = true;
  }
}

void TriggerValue_ctor(TriggerValue *self, void *buffer, size_t value_size, size_t capacity) {
  self->buffer = buffer;
  self->value_size = value_size;
  self->capacity = capacity;
  self->read_idx = 0;
  self->write_idx = 0;
  self->empty = true;
  self->push = TriggerValue_push;
  self->pop = TriggerValue_pop;
}
