#ifndef REACTOR_UC_PLATFORM_H
#define REACTOR_UC_PLATFORM_H

#include "reactor-uc/error.h"
#include "reactor-uc/tag.h"
#include <stdarg.h>

typedef struct Platform Platform;
typedef struct Mutex Mutex;
typedef struct Thread Thread;

/**
 * @brief Each supported platform must provide a mutex, this is used by the runtime
 * to protect shared data structures.
 *
 * The mutex can be implemented using the underlying OS, or just disabling/enabling interrupts.
 *
 */
struct Mutex {
  void (*lock)(Mutex *super);
  void (*unlock)(Mutex *super);
};

struct Thread {};

/** Construct a Mutex*/
void Mutex_ctor(Mutex *super);

struct Platform {
  /**
   * @brief Return the current physical time in nanoseconds.
   */
  instant_t (*get_physical_time)(Platform *super);
  /**
   * @brief Put system to sleep until the wakeup time. Asynchronous events
   * does not need to be handled.
   */
  lf_ret_t (*wait_until)(Platform *super, instant_t wakeup_time);
  /**
   * @brief Put system to sleep until the wakeup time. Asynchronous events
   * does not need to be handled.
   */
  lf_ret_t (*wait_for)(Platform *super, interval_t duration);
  /**
   * @brief Put the system to sleep until the wakeup time or until an
   * asynchronous event occurs. Must be called from a critical section.
   */
  lf_ret_t (*wait_until_interruptible)(Platform *super, instant_t wakeup_time);

  /**
   * @brief Create a a new thread.
   */
  lf_ret_t (*create_thread)(Platform *super, Thread *thread, void *(*thread_func)(void *), void *arguments);

  /**
   * @brief Join a thread. Blocks until the thread has terminated.
   */
  lf_ret_t (*join_thread)(Platform *super, Thread *thread);

  /**
   * @brief Signal the occurrence of an asynchronous event. This should wake
   * up the platform if it is sleeping on `wait_until_interruptible_locked`.
   */
  void (*notify)(Platform *super);
};

// Construct a Platform object. Must be implemented for each target platform.
void Platform_ctor(Platform *super);

// Returns a pointer to the platform.P
Platform *Platform_new();

// Allow each platform to provide its own implementation for printing.
void Platform_vprintf(const char *fmt, va_list args);

#if defined(PLATFORM_POSIX)
#include "reactor-uc/platform/posix/posix.h"
#elif defined(PLATFORM_RIOT)
#include "reactor-uc/platform/riot/riot.h"
#elif defined(PLATFORM_ZEPHYR)
#include "reactor-uc/platform/zephyr/zephyr.h"
#elif defined(PLATFORM_FLEXPRET)
#include "reactor-uc/platform/flexpret/flexpret.h"
#elif defined(PLATFORM_PICO)
#include "reactor-uc/platform/pico/pico.h"
#elif defined(PLATFORM_PATMOS)
#include "reactor-uc/platform/patmos/patmos.h"
#elif defined(PLATFORM_ADUCM355)
#include "reactor-uc/platform/aducm355/aducm355.h"
#else
#error "NO PLATFORM SPECIFIED"
#endif

#define MUTEX_LOCK(Mutex) (Mutex).super.lock(&(Mutex).super)
#define MUTEX_UNLOCK(Mutex) (Mutex).super.unlock(&(Mutex).super)

#endif
