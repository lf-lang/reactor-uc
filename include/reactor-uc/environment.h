#ifndef REACTOR_UC_ENVIRONMENT_H
#define REACTOR_UC_ENVIRONMENT_H

#include "reactor-uc/builtin_triggers.h"
#include "reactor-uc/error.h"
#include "reactor-uc/network_channel.h"
#include "reactor-uc/platform.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/scheduler.h"

typedef struct Environment Environment;

struct Environment {
  Reactor *main;        // The top-level reactor of the program.
  Scheduler *scheduler; // The scheduler in charge of executing the reactions.
  Platform *platform;   // The platform that provides the physical time and sleep functions.
  bool has_async_events;
  BuiltinTrigger *startup;                 // A pointer to a startup trigger, if the program has one.
  BuiltinTrigger *shutdown;                // A pointer to a chain of shutdown triggers, if the program has one.
  FederatedConnectionBundle **net_bundles; // A pointer to an array of NetworkChannel pointers that are used to
                                           // communicate with other federates running in different environments.
  size_t net_bundles_size;                 // The number of NetworkChannels in the net_channels array.
  /**
   * @brief Assemble the program by computing levels for each reaction and setting up the scheduler.
   */
  void (*assemble)(Environment *self);

  /**
   * @brief Start the program.
   */
  void (*start)(Environment *self);

  /**
   * @brief Wrapper around `wait_until` exposed by the platform.
   */
  lf_ret_t (*wait_until)(Environment *self, instant_t wakeup_time);

  /**
   * @brief Get the elapsed logical time since the start of the program.
   */
  interval_t (*get_elapsed_logical_time)(Environment *self);

  /**
   * @brief Get the current logical time
   */
  instant_t (*get_logical_time)(Environment *self);
  /**
   * @brief Get the elapsed physical time since the start of the program.
   */
  interval_t (*get_elapsed_physical_time)(Environment *self);
  /**
   * @brief Get the current physical time.
   */
  instant_t (*get_physical_time)(Environment *self);

  void (*enter_critical_section)(Environment *self);
  void (*leave_critical_section)(Environment *self);

  void (*request_shutdown)(Environment *self);
};

void Environment_ctor(Environment *self, Reactor *main);
void Environment_free(Environment *self);

#endif
