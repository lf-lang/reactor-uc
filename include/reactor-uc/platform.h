#ifndef REACTOR_UC_PLATFORM_H
#define REACTOR_UC_PLATFORM_H

#include "reactor-uc/error.h"
#include "reactor-uc/tag.h"
#include <stdarg.h>

typedef struct Platform Platform;
typedef struct Mutex Mutex;

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

/** Construct a Mutex*/
void Mutex_ctor(Mutex *super);

struct Platform {
  /**
   * @brief Return the current physical time in nanoseconds.
   */
  instant_t (*get_physical_time)(Platform *super);
  /**
   * @brief Set the priority of the current thread to schedule the current
   * reaction with deadline monotonic. A negative deadline results in setting
   * the highest priority to the current thread (e.g., when the main thread
   * is sleeping or with the TCP thread).
   */
  lf_ret_t (*set_thread_priority)(interval_t rel_deadline);
   /**
    * @brief Set the scheduling policy for the current thread. The policy is
    * specified by the LF_THREAD_POLICY macro.
    */
  lf_ret_t (*set_scheduling_policy)();
  /**
    * @brief Set the core affinity for the current thread. The number of cores
    * to use is specified by the LF_NUMBER_OF_CORES macro.
    */
  lf_ret_t (*set_core_affinity)();
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

/**
 * @brief The thread scheduling policies.
 */
 typedef enum {
  LF_SCHED_FAIR,      // Non real-time scheduling policy. Corresponds to SCHED_OTHER
  LF_SCHED_TIMESLICE, // Real-time, time-slicing priority-based policy. Corresponds to SCHED_RR.
  LF_SCHED_PRIORITY,  // Real-time, priority-only based scheduling. Corresponds to SCHED_FIFO.
} lf_scheduling_policy_type_t;


/**
 * @brief The thread scheduling policy to use.
 *
 * This should be one of   LF_SCHED_FAIR, LF_SCHED_TIMESLICE, or LF_SCHED_PRIORITY.
 * The default is LF_SCHED_FAIR, which corresponds to the Linux SCHED_OTHER.
 * LF_SCHED_TIMESLICE corresponds to Linux SCHED_RR, and LF_SCHED_PRIORITY corresponds
 * to SCHED_FIFO.
 */
 #ifndef LF_THREAD_POLICY
 #define LF_THREAD_POLICY LF_SCHED_FAIR
 #endif


/**
 * @brief The number of cores of the hardware platform to use.
 */
#ifndef LF_NUMBER_OF_CORES
#define LF_NUMBER_OF_CORES 0
#endif

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
