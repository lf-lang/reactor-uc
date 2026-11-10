
#ifndef REACTOR_UC_PLATFORM_PICO_H
#define REACTOR_UC_PLATFORM_PICO_H

#include "reactor-uc/platform.h"
#include <pico/stdlib.h>
#include <pico/sync.h>

typedef struct {
  Platform super;
  semaphore_t sem;
  volatile unsigned num_nested_critical_sections;
  // Interrupt state saved by the outermost Mutex lock
  uint32_t saved_irq_state;
} PlatformPico;

typedef struct {
  Mutex super;
} MutexPico;

#define PLATFORM_T PlatformPico
#define MUTEX_T MutexPico

#endif
