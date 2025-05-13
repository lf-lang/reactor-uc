#ifndef REACTOR_UC_PLATFORM_POSIX_H
#define REACTOR_UC_PLATFORM_POSIX_H

#include <pthread.h>
#include <stdbool.h>
#include "reactor-uc/platform.h"

typedef struct {
  Mutex super;
  pthread_mutex_t lock;
} MutexPosix;

typedef struct {
  Platform super;
  pthread_cond_t cond;
  MutexPosix mutex;
  bool new_async_event;
} PlatformPosix;

#define PLATFORM_T PlatformPosix
#define MUTEX_T MutexPosix

#endif
