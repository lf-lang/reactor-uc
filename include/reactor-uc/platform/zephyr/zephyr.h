
#ifndef REACTOR_UC_PLATFORM_ZEPHYR_H
#define REACTOR_UC_PLATFORM_ZEPHYR_H

#include "reactor-uc/platform.h"
#include <zephyr/kernel.h>

typedef struct {
  Platform super;
  struct k_sem sem;
} PlatformZephyr;

typedef struct {
  Mutex super;
  struct k_mutex mutex;
} MutexZephyr;

#define PLATFORM_T PlatformZephyr
#define THREAD_T void
#define MUTEX_T MutexZephyr

#endif
