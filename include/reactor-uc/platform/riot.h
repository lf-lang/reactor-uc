#ifndef REACTOR_UC_PLATFORM_POSIX_H
#define REACTOR_UC_PLATFORM_POSIX_H

#include "reactor-uc/platform.h"

#include "mutex.h"

typedef struct {
  Platform super;

  mutex_t lock;
} PlatformRiot;

void PlatformRiot_ctor(Platform *self);
#endif
