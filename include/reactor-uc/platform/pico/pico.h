
#ifndef REACTOR_UC_PLATFORM_PICO_H
#define REACTOR_UC_PLATFORM_PICO_H

#include "reactor-uc/platform.h"
#include <pico/stdlib.h>
#include <pico/sync.h>

typedef struct {
  Platform super;
  semaphore_t sem;
  volatile unsigned num_nested_critical_sections;
} PlatformPico;

typedef struct {
  Mutex super;
  critical_section_t crit_sec;
} MutexPico;

#define PLATFORM_T PlatformPico
#define MUTEX_T MutexPico
#define THREAD_T void *

#endif
