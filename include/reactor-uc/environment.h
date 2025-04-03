#ifndef REACTOR_UC_ENVIRONMENT_H
#define REACTOR_UC_ENVIRONMENT_H

#include "reactor-uc/builtin_triggers.h"
#include "reactor-uc/error.h"
#include "reactor-uc/platform.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/scheduler.h"
#include "reactor-uc/queues.h"

typedef struct Environment Environment;
extern Environment *_lf_environment; // NOLINT

struct Environment {
  Reactor *main;        // The top-level reactor of the program.
  Scheduler *scheduler; // The scheduler in charge of executing the reactions.
  Platform *platform;   // The platform that provides the physical time and sleep functions.
  bool has_async_events;
  bool fast_mode;
  BuiltinTrigger *startup;  // A pointer to a startup trigger, if the program has one.
  BuiltinTrigger *shutdown; // A pointer to a chain of shutdown triggers, if the program has one.
  /**
   * @private
   * @brief Assemble the program by computing levels for each reaction and setting up the scheduler.
   */
  void (*assemble)(Environment *self);

  /**
   * @private
   * @brief Start the program.
   */
  void (*start)(Environment *self);

  /**
   * @private
   * @brief Sleep until the wakeup time.
   * @param self The environment.
   * @param wakeup_time The absolute time to wake up.
   *
   * If the program has physical actions or is federated, then the sleep will be interruptible.
   * This function must be called from a critical section.
   *
   */
  lf_ret_t (*wait_until_locked)(Environment *self, instant_t wakeup_time);

  /**
   * @brief Get the elapsed logical time since the start of the program.
   * @param self The environment.
   *
   * The elapsed logical time is equal to the tag of the currently executing reaction
   * minus the start tag of the program.

   * @returns The elapsed logical time.
   */
  interval_t (*get_elapsed_logical_time)(Environment *self);

  /**
   * @brief Get the current logical time of the program.AbstractEvent
   * @param self The environment.
   *
   * The current logical time is equal to the tag of the currently executing reaction.
   *
   * @returns The current logical time.
   */
  instant_t (*get_logical_time)(Environment *self);

  /**
   * @brief Get the elapsed physical time since the start of the program.
   * @param self The environment.
   *
   * The elapsed physical time is the current wall-clock time as reported by
   * the underlying platform minus the start time of the program.
   *
   * @returns The elapsed physical time.
   */
  interval_t (*get_elapsed_physical_time)(Environment *self);

  /**
   * @brief Get the current physical time.
   * @param self The environment.
   *
   * The current physical time as reported by the underlying platform. May or may
   * not be synchronized to UTC time.
   *
   * @returns The current physical time.
   */
  instant_t (*get_physical_time)(Environment *self);

  /**
   * @private
   * @brief Enter a critical section. Either by disabling interrupts, using a mutex or both.
   */
  void (*enter_critical_section)(Environment *self);

  /**
   * @private
   * @brief Leave a critical section. Either by enabling interrupts, releasing a mutex or both.
   */
  void (*leave_critical_section)(Environment *self);

  /**
   * @brief Request the termination of the program.
   * @param self The environment.
   *
   * This function will request the shutdown of the program at the earliest possible time.
   * Any reaction triggered by the shutdown trigger will be executed before the program terminates.
   * If the program is not federated, then the shutdown will occur at the next microstep.
   * If the program is federated, then the shutdown tag will be negotiated with the other federates.
   */
  void (*request_shutdown)(Environment *self);

  /**
   * @private
   * @brief Acquire permission to execute a requested tag.
   * @param self The environment.
   * @param tag The tag that is requested to execute.
   *
   * This function is invoked from the scheduler when it wants to execute a tag.
   * In a federated setting, we might have to wait before doing this. We might
   * wait for a STA offset or send out a coordination message to the upstream.
   */
  lf_ret_t (*acquire_tag)(Environment *self, tag_t tag);

  /**
   * @private
   * @brief Poll any needed network channels
   * @param self The environment.
   *
   * This function should only be supplied in a federated environment. It should
   * poll all the PolledNetworkChannels that the federate has.
   */
  lf_ret_t (*poll_network_channels)(Environment *self);
};

void Environment_ctor(Environment *self, Reactor *main, Scheduler *scheduler, bool fast_mode);
void Environment_free(Environment *self);

#endif
