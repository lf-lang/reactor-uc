#ifndef REACTOR_UC_PLATFORM_H
#define REACTOR_UC_PLATFORM_H

#include "reactor-uc/error.h"
#include "reactor-uc/tag.h"
typedef struct Platform Platform;

struct Platform {
  lf_ret_t (*initialize)(Platform *self);
  instant_t (*get_physical_time)(Platform *self);
  lf_ret_t (*wait_until)(Platform *self, instant_t wakeup_time);
  // FIXME: Consider naming it _locked since it needs to be in a critical section when called.
  lf_ret_t (*wait_until_interruptable)(Platform *self, instant_t wakeup_time);
  void (*enter_critical_section)(Platform *self);
  void (*leave_critical_section)(Platform *self);
  void (*new_async_event)(Platform *self);
};

// Return a pointer to a Platform object. Must be implemented for each
// target platform.
Platform *Platform_new(void);

// Construct a Platform object. Must be implemented for each target platform.
void Platform_ctor(Platform *self);

#endif
