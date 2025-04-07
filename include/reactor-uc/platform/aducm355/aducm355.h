
#ifndef REACTOR_UC_PLATFORM_ADUCM355_H
#define REACTOR_UC_PLATFORM_ADUCM355_H

#include "reactor-uc/platform.h"
#include <stdbool.h>


typedef struct {
  Mutex super;
} MutexAducm355;

typedef struct {
  Platform super;
  MutexAducm355 mutex;
  volatile instant_t epoch;
  uint32_t ticks_last;
  volatile bool new_async_event;
  volatile uint32_t num_nested_critical_sections;
} PlatformAducm355;

#define PLATFORM_T PlatformAducm355
#define MUTEX_T MutexAducm355

#endif
