#ifndef REACTOR_UC_PLATFORM_FLEXPRET_H
#define REACTOR_UC_PLATFORM_FLEXPRET_H

#include "reactor-uc/platform.h"
#include <flexpret/flexpret.h>

typedef struct {
  Mutex super;
  fp_lock_t mutex;
} MutexFlexpret;

typedef struct {
  Platform super;
  fp_lock_t mutex;
  volatile bool async_event_occurred;
  volatile int num_nested_critical_sections;
} PlatformFlexpret;

void PlatformFlexpret_ctor(Platform *super);

#define PLATFORM_T PlatformFlexpret
#define MUTEX_T MutexFlexpret
#define THREAD_T void*

#endif
