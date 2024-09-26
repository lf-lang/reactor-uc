
#ifndef REACTOR_UC_PLATFORM_ZEPHYR_H
#define REACTOR_UC_PLATFORM_ZEPHYR_H

#include "reactor-uc/platform.h"
#include <zephyr/kernel.h>

typedef struct {
  Platform super;
  struct k_sem sem;
  unsigned irq_mask;
} PlatformZephyr;

#endif
