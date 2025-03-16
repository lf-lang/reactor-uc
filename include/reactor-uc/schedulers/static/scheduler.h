//
// Created by tanneberger on 11/16/24.
//

#ifndef STATIC_SCHEDULER_H
#define STATIC_SCHEDULER_H
#include "reactor-uc/error.h"
#include "reactor-uc/queues.h"
#include "reactor-uc/scheduler.h"
#include "reactor-uc/schedulers/static/instructions.h"
#include "reactor-uc/schedulers/static/circular_buffer.h"

typedef struct StaticScheduler StaticScheduler;
typedef struct StaticSchedulerState StaticSchedulerState;
typedef struct ReactorTagPair ReactorTagPair;
typedef struct TriggerBuffer TriggerBuffer;
typedef struct Environment Environment;

/**
 * A struct storing registers and other info related to the static scheduler.
 */
struct StaticSchedulerState {
  size_t pc;
  instant_t start_time; // From application
  uint64_t timeout;     // From application
  size_t num_progress_index;
  reg_t time_offset;
  reg_t offset_inc;
  uint64_t zero;
  uint64_t one;
  // FIXME: Separate worker state from global state in another struct.
  uint64_t progress_index;
  reg_t return_addr;
  reg_t binary_sema;
  reg_t temp0;
  reg_t temp1;
};

struct ReactorTagPair {
  Reactor *reactor;
  tag_t tag;
};

struct TriggerBuffer {
  Event staged_event;    // Event popped from the circular buffer to be processed at this tag
  Trigger *trigger;       // The trigger associated with the circular buffer
  CircularBuffer buffer;  // The circular buffer for events associated with the trigger
};

struct StaticScheduler {
  Scheduler super;
  Environment *env;
  const inst_t *static_schedule;
  StaticSchedulerState state;
  ReactorTagPair *reactor_tags;
  size_t reactor_tags_size;
  TriggerBuffer *trigger_buffers;
  size_t trigger_buffers_size;
};

void StaticScheduler_ctor(StaticScheduler *self, Environment *env);

#endif // STATIC_SCHEDULER_H
