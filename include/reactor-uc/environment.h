#ifndef REACTOR_UC_ENVIRONMENT_H
#define REACTOR_UC_ENVIRONMENT_H

#include "reactor-uc/builtin_triggers.h"
#include "reactor-uc/error.h"
#include "reactor-uc/platform.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/scheduler.h"

typedef struct Environment Environment;
typedef struct TcpIpBundle TcpIpBundle;

struct Environment {
  Reactor *main;
  Scheduler scheduler;
  Platform *platform;
  tag_t stop_tag;
  tag_t current_tag;
  instant_t start_time;
  bool keep_alive;
  bool has_async_events;
  Startup *startup;
  Shutdown *shutdown;
  TcpIpBundle **net_bundles; // FIXME: Refer to NetworkBundle instead of TcpIpBundle...
  size_t net_bundles_size;
  void (*assemble)(Environment *self);
  void (*start)(Environment *self);
  lf_ret_t (*wait_until)(Environment *self, instant_t wakeup_time);
  void (*set_timeout)(Environment *self, interval_t duration);
  interval_t (*get_elapsed_logical_time)(Environment *self);
  instant_t (*get_logical_time)(Environment *self);
  interval_t (*get_elapsed_physical_time)(Environment *self);
  instant_t (*get_physical_time)(Environment *self);
};

void Environment_ctor(Environment *self, Reactor *main);
void Environment_free(Environment *self);

#endif
