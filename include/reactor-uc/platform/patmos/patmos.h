#ifndef REACTOR_UC_PLATFORM_PATMOS_H
#define REACTOR_UC_PLATFORM_PATMOS_H

#include "reactor-uc/platform.h"
#include "stdbool.h"

typedef struct {
  Platform super;
  volatile bool async_event;
  volatile int num_nested_critical_sections;
} PlatformPatmos;

void PlatformPatmos_ctor(Platform *self);
#endif
