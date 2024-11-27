#ifndef REACTOR_UC_PLATFORM_FLEXPRET_H
#define REACTOR_UC_PLATFORM_FLEXPRET_H

#include "reactor-uc/platform.h"
#include <flexpret/flexpret.h>

typedef struct {
  Platform super;
  volatile bool async_event_occurred;
  bool in_critical_section;
  fp_lock_t lock;
} PlatformFlexpret;

void PlatformFlexpret_ctor(Platform *self);
#endif
