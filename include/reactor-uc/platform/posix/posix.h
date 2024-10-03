#ifndef REACTOR_UC_PLATFORM_POSIX_H
#define REACTOR_UC_PLATFORM_POSIX_H

#include "reactor-uc/platform.h"

typedef struct {
  Platform super;
} PlatformPosix;

void PlatformPosix_ctor(Platform *self);
#endif
