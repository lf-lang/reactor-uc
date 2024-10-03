#include "reactor-uc/trigger_value.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int TriggerValue_stage(TriggerValue *self, const void *value) {
  if (!self->empty && self->read_idx == self->write_idx) {
    return -1;
  }
  memcpy(self->buffer + self->write_idx * self->value_size, value, self->value_size);
  self->staged = true;
  return 0;
}

int TriggerValue_push(TriggerValue *self) {
  if (!self->staged) {
    return -1;
  }
  self->write_idx = (self->write_idx + 1) % self->capacity;
  self->empty = false;
  self->staged = false;

  return 0;
}

int TriggerValue_pop(TriggerValue *self) {
  if (self->empty) {
    return -1;
  }

  self->read_idx = (self->read_idx + 1) % self->capacity;
  if (self->read_idx == self->write_idx) {
    self->empty = true;
  }

  return 0;
}

void TriggerValue_ctor(TriggerValue *self, char *buffer, size_t value_size, size_t capacity) {
  self->buffer = buffer;
  self->value_size = value_size;
  self->capacity = capacity;
  self->read_idx = 0;
  self->write_idx = 0;
  self->empty = true;
  self->staged = true;
  self->push = TriggerValue_push;
  self->pop = TriggerValue_pop;
  self->stage = TriggerValue_stage;
}
