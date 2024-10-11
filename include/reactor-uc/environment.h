#ifndef REACTOR_UC_ENVIRONMENT_H
#define REACTOR_UC_ENVIRONMENT_H

#include "reactor-uc/builtin_triggers.h"
#include "reactor-uc/error.h"
#include "reactor-uc/network_channel.h"
#include "reactor-uc/platform.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/scheduler.h"

typedef struct Environment Environment;
typedef struct TcpIpChannel TcpIpChannel;

struct Environment {
  Reactor *main;
  Scheduler scheduler;
  Platform *platform;
  tag_t stop_tag;
  tag_t current_tag;
  instant_t start_time;
  bool keep_alive;
  bool has_async_events; // Whether the environment either has an action, or has a connection to an upstream federate.
  Startup *startup;
  Shutdown *shutdown;
  NetworkChannel **net_channels;
  size_t net_channel_size;
  void (*assemble)(Environment *self);
  void (*start)(Environment *self);
  void (*set_start_time)(Environment *self);
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
