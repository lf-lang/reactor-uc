#ifndef REACTOR_UC_PLATFORM_POSIX_H
#define REACTOR_UC_PLATFORM_POSIX_H

#include "reactor-uc/platform.h"
#include <pthread.h>

typedef struct {
  Platform super;
  pthread_mutex_t lock;
  pthread_cond_t cond;
} PlatformPosix;

void PlatformPosix_ctor(Platform *super);
#endif
