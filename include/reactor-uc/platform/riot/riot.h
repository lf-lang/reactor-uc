#ifndef REACTOR_UC_PLATFORM_RIOT_H
#define REACTOR_UC_PLATFORM_RIOT_H

#include "mutex.h"
#include "reactor-uc/platform.h"

typedef struct {
  Platform super;
  mutex_t lock;
} PlatformRiot;

typedef struct {
  Mutex super;
  mutex_t mutex;
} MutexRiot;

#define PLATFORM_T PlatformRiot
#define MUTEX_T MutexRiot

#endif
