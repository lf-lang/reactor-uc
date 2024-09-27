#ifndef REACTOR_UC_TRIGGER_VALUE_H
#define REACTOR_UC_TRIGGER_VALUE_H

#include <stdbool.h>
#include <stddef.h>
typedef struct TriggerValue TriggerValue;

struct TriggerValue {
  void *buffer;
  size_t read_idx;
  size_t write_idx;
  size_t value_size;
  size_t capacity;
  bool empty;

  int (*push)(TriggerValue *, const void *value);
  int (*pop)(TriggerValue *);
};

void TriggerValue_ctor(TriggerValue *self, void *buffer, size_t value_size, size_t capacity);

#endif
