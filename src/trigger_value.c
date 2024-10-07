#include "reactor-uc/trigger_value.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>

lf_ret_t TriggerValue_stage(TriggerValue *self, const void *value) {
  if (!self->empty && self->read_idx == self->write_idx) {
    return LF_OUT_OF_BOUNDS;
  }
  memcpy(self->buffer + self->write_idx * self->value_size, value, self->value_size); // NOLINT
  self->staged = true;
  return LF_OK;
}

lf_ret_t TriggerValue_push(TriggerValue *self) {
  if (!self->staged) {
    return LF_INVALID_VALUE;
  }

  self->write_idx = (self->write_idx + 1) % self->capacity;
  self->empty = false;
  self->staged = false;

  return 0;
}

lf_ret_t TriggerValue_pop(TriggerValue *self) {
  if (self->empty) {
    return LF_EMPTY;
  }

  self->read_idx = (self->read_idx + 1) % self->capacity;
  if (self->read_idx == self->write_idx) {
    self->empty = true;
  }

  return LF_OK;
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
