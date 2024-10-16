#ifndef REACTOR_UC_TRIGGER_DATA_QUEUE_H
#define REACTOR_UC_TRIGGER_DATA_QUEUE_H

#include "reactor-uc/error.h"
#include <stdbool.h>
#include <stddef.h>
typedef struct TriggerDataQueue TriggerDataQueue;

/**
 * @brief A TriggerDataQueue is a type wrapping the memory associated with a Trigger.
 * In reactor-uc this memory must be statically allocated and TriggerDataQueue
 * implements a FIFO around it. The TriggerDataQueue needs a pointer to the allocated
 * memory and the allocated size as well as the size of each element. Then we
 * can stage, push and pop data from the runtime without knowing about the types
 * involved..
 *
 */
struct TriggerDataQueue {
  char *buffer;
  size_t read_idx;
  size_t write_idx;
  size_t value_size; // TODO: BEtter name.
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
  lf_ret_t (*stage)(TriggerDataQueue *, const void *value);

  /**
   * @brief Pushes the staged value into the FIFO.
   *
   */
  lf_ret_t (*push)(TriggerDataQueue *);

  /**
   * @brief Increments the `read_idx` and as such pops the head of the
   * queue. This function does not return the head of the queue. Only increments
   * the pointers.
   */
  lf_ret_t (*pop)(TriggerDataQueue *);
};

void TriggerDataQueue_ctor(TriggerDataQueue *self, char *buffer, size_t value_size, size_t capacity);

#endif
