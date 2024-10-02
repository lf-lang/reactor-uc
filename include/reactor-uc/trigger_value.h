#ifndef REACTOR_UC_TRIGGER_VALUE_H
#define REACTOR_UC_TRIGGER_VALUE_H

#include <stdbool.h>
#include <stddef.h>
typedef struct TriggerValue TriggerValue;

// FIXME: Handle "void" TriggerValues
/**
 * @brief A TriggerValue is a type wrapping the memory associated with a Trigger.
 * In reactor-uc this memory must be statically allocated and TriggerValue
 * implements a FIFO around it. The TriggerValue needs a pointer to the allocated
 * memory and the allocated size as well as the size of each element. Then we
 * can stage, push and pop data from the runtime without knowing about the types
 * involved..
 *
 */
struct TriggerValue {
  char *buffer;
  size_t read_idx;
  size_t write_idx;
  size_t value_size;
  size_t capacity;
  bool empty;
  bool staged;

  /**
   * @brief This function copies `value` into the buffer at the place
   * designated by `write_idx`, but does not increment `write_idx`. As such,
   * the value is just staged and can be overwritten later. This is to support
   * multiple writes to a trigger in a single tag and "last write wins."
   *
   */
  int (*stage)(TriggerValue *, const void *value);

  /**
   * @brief Pushes the staged value into the FIFO.
   *
   */
  int (*push)(TriggerValue *);

  /**
   * @brief Increments the `read_idx` and as such pops the head of the
   * queue. This function does not return the head of the queue. Only increments
   * the pointers.
   */
  int (*pop)(TriggerValue *);
};

void TriggerValue_ctor(TriggerValue *self, char *buffer, size_t value_size, size_t capacity);

#endif
