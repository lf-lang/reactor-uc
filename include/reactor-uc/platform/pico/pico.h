
#ifndef REACTOR_UC_PLATFORM_PICO_H
#define REACTOR_UC_PLATFORM_PICO_H

#include "reactor-uc/platform.h"
#include <pico/stdlib.h>
#include <pico/sync.h>

typedef struct {
  Platform super;
  critical_section_t crit_sec;
  semaphore_t sem;
  volatile int num_nested_critical_sections;
} PlatformPico;

#endif
