#ifndef REACTOR_UC_PLATFORM_FLEXPRET_H
#define REACTOR_UC_PLATFORM_FLEXPRET_H

#include "reactor-uc/platform.h"
#include <flexpret/flexpret.h>

typedef struct {
  Platform super;
  fp_lock_t lock;
  unsigned irq_mask;
} PlatformFlexpret;

#endif
