#ifndef REACTOR_UC_PLATFORM_RIOT_H
#define REACTOR_UC_PLATFORM_RIOT_H

#include "mutex.h"
#include "reactor-uc/platform.h"

typedef struct {
  Platform super;
  mutex_t lock;
  volatile unsigned irq_mask;
  volatile int num_nested_critical_sections;
} PlatformRiot;

void PlatformRiot_ctor(Platform *super);
#endif
