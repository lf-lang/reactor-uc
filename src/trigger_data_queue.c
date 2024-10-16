#include "reactor-uc/trigger_data_queue.h"
#include "reactor-uc/logging.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>

lf_ret_t TriggerDataQueue_stage(TriggerDataQueue *self, const void *value) {
  if (!self->empty && self->read_idx == self->write_idx) {
    LF_ERR(TRIG, "Could not stage value, TriggerDataQueue %p is full", self);
    return LF_OUT_OF_BOUNDS;
  }
  memcpy(self->buffer + self->write_idx * self->value_size, value, self->value_size); // NOLINT
  self->staged = true;
  return LF_OK;
}

lf_ret_t TriggerDataQueue_push(TriggerDataQueue *self) {
  if (!self->staged) {
    LF_ERR(TRIG, "Could not push value, no value staged in TriggerDataQueue %p", self);
    return LF_INVALID_VALUE;
  }

  self->write_idx = (self->write_idx + 1) % self->capacity;
  self->empty = false;
  self->staged = false;

  return 0;
}

lf_ret_t TriggerDataQueue_pop(TriggerDataQueue *self) {
  if (self->empty) {
    LF_ERR(TRIG, "Could not pop value, TriggerDataQueue %p is empty", self);
    return LF_EMPTY;
  }

  self->read_idx = (self->read_idx + 1) % self->capacity;
  if (self->read_idx == self->write_idx) {
    self->empty = true;
  }

  return LF_OK;
}

void TriggerDataQueue_ctor(TriggerDataQueue *self, char *buffer, size_t value_size, size_t capacity) {
  self->buffer = buffer;
  self->value_size = value_size;
  self->capacity = capacity;
  self->read_idx = 0;
  self->write_idx = 0;
  self->empty = true;
  self->staged = true;
  self->push = TriggerDataQueue_push;
  self->pop = TriggerDataQueue_pop;
  self->stage = TriggerDataQueue_stage;
}
