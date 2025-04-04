
#ifndef REACTOR_UC_PLATFORM_ADUCM355_H
#define REACTOR_UC_PLATFORM_ADUCM355_H

#include "reactor-uc/platform.h"
#include <stdbool.h>

typedef struct {
  Platform super;
  volatile instant_t epoch;
  uint32_t ticks_last;
  volatile bool notify;
  volatile uint32_t num_nested_critical_sections;
} PlatformAducm355;

#endif
