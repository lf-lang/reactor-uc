#ifndef REACTOR_UC_PLATFORM_H
#define REACTOR_UC_PLATFORM_H

#include "reactor-uc/error.h"
#include "reactor-uc/tag.h"
#include <stdarg.h>

typedef struct Platform Platform;

struct Platform {
  /**
   * @brief Perform any platform initialization such as initializing semaphores,
   * configuring clocks, etc. Called once after construction.
   */
  lf_ret_t (*initialize)(Platform *self);
  /**
   * @brief Return the current physical time in nanoseconds.
   */
  instant_t (*get_physical_time)(Platform *self);
  /**
   * @brief Put system to sleep until the wakeup time. Asynchronous events
   * does not need to be handled.
   */
  lf_ret_t (*wait_until)(Platform *self, instant_t wakeup_time);
  /**
   * @brief Put system to sleep until the wakeup time. Asynchronous events
   * does not need to be handled.
   */
  lf_ret_t (*wait_for)(Platform *self, interval_t duration);
  /**
   * @brief Put the system to sleep until the wakeup time or until an
   * asynchronous event occurs. Must be called from a critical section.
   */
  lf_ret_t (*wait_until_interruptible_locked)(Platform *self, instant_t wakeup_time);

  /**
   * @brief Signal the occurrence of an asynchronous event. This should wake
   * up the platform if it is sleeping on `wait_until_interruptible_locked`.
   */
  void (*new_async_event)(Platform *self);

  /**
   * @brief Enter and leave a critical section. This must support nested critical sections. Critical sections
   * can be implemented by disabling interrupts or acquiring a mutex.
   */
  void (*enter_critical_section)(Platform *self);
  void (*leave_critical_section)(Platform *self);
};

// Return a pointer to a Platform object. Must be implemented for each
// target platform.
Platform *Platform_new(void);

// Construct a Platform object. Must be implemented for each target platform.
void Platform_ctor(Platform *self);

// Allow each platform to provide its own implementation for printing.
void Platform_vprintf(const char *fmt, va_list args);

#endif
