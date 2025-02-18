#ifndef REACTOR_UC_ENVIRONMENT_H
#define REACTOR_UC_ENVIRONMENT_H

#include "reactor-uc/builtin_triggers.h"
#include "reactor-uc/error.h"
#include "reactor-uc/network_channel.h"
#include "reactor-uc/startup_coordinator.h"
#include "reactor-uc/clock_synchronization.h"
#include "reactor-uc/platform.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/scheduler.h"
#include "reactor-uc/queues.h"
#include "reactor-uc/physical_clock.h"

typedef struct Environment Environment;
extern Environment *_lf_environment; // NOLINT

struct Environment {
  Reactor *main;        // The top-level reactor of the program.
  Scheduler *scheduler; // The scheduler in charge of executing the reactions.
  Platform *platform;   // The platform that provides the physical time and sleep functions.
  PhysicalClock clock; // The physical clock that provides the physical time.
  bool has_async_events;
  bool fast_mode;
  bool is_federated;
  bool do_clock_sync;
  BuiltinTrigger *startup;                 // A pointer to a startup trigger, if the program has one.
  BuiltinTrigger *shutdown;                // A pointer to a chain of shutdown triggers, if the program has one.
  FederatedConnectionBundle **net_bundles; // A pointer to an array of NetworkChannel pointers that are used to
                                           // communicate with other federates running in different environments.
  size_t net_bundles_size;                 // The number of NetworkChannels in the net_channels array.
  size_t federation_longest_path;          // The longest path in the federation.
  StartupCoordinator *startup_coordinator; // A pointer to the startup coordinator, if the program has one.
  ClockSynchronization *clock_sync;        // A pointer to the clock synchronization module, if the program has one.
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
   *
   */
  lf_ret_t (*wait_until)(Environment *self, instant_t wakeup_time);

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
};

void Environment_ctor(Environment *self, Reactor *main, interval_t duration, EventQueue *event_queue,
                      EventQueue *system_event_queue, ReactionQueue *reaction_queue, bool keep_alive, bool is_federated,
                      bool fast_mode, FederatedConnectionBundle **net_bundles, size_t net_bundles_size,
                      StartupCoordinator *startup_coordinator, ClockSynchronization *clock_sync);
void Environment_free(Environment *self);

#endif
