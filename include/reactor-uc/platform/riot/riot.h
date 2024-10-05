#ifndef REACTOR_UC_PLATFORM_RIOT_H
#define REACTOR_UC_PLATFORM_RIOT_H

#include "mutex.h"
#include "reactor-uc/platform.h"

#include "mutex.h"

typedef struct {
  Platform super;
  mutex_t lock;
  unsigned irq_mask;
} PlatformRiot;

void PlatformRiot_ctor(Platform *self);
#endif
