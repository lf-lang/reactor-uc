#ifndef REACTOR_UC_PLATFORM_PATMOS_H
#define REACTOR_UC_PLATFORM_PATMOS_H

#include "reactor-uc/platform.h"
#include "stdbool.h"

typedef struct {
  Platform super;
  bool async_event;
} PlatformPatmos;

void PlatformPatmos_ctor(Platform *self);
#endif
