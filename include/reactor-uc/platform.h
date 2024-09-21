#ifndef REACTOR_UC_PLATFORM_H
#define REACTOR_UC_PLATFORM_H

#include "reactor-uc/tag.h"
typedef struct Platform Platform;

struct Platform {
  void (*initialize)(Platform *self);
  instant_t (*get_physical_time)(Platform *self);
  int (*wait_until)(Platform *self, instant_t wakeup_time);
  int (*wait_until_interruptable)(Platform *self, instant_t wakeup_time);
  void (*enter_critical_section)(Platform *self);
  void (*leave_critical_section)(Platform *self);
  void (*new_async_event)(Platform *self);
};

Platform *Platform_new(void);
void Platform_ctor(Platform *self);

#endif
