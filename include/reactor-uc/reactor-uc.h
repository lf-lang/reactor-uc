#ifndef REACTOR_UC_REACTOR_UC_H
#define REACTOR_UC_REACTOR_UC_H

#if defined(SCHEDULER_DYNAMIC)
#include "./schedulers/dynamic/scheduler.h"
#elif defined(SCHEDULER_STATIC)
#include "schedulers/static/scheduler.h"
#include "schedulers/static/instructions.h"
#else
#endif

#include "reactor-uc/action.h"
#include "reactor-uc/builtin_triggers.h"
#include "reactor-uc/connection.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/network_channel.h"
#include "reactor-uc/error.h"
#include "reactor-uc/federated.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/platform.h"
#include "reactor-uc/port.h"
#include "reactor-uc/serialization.h"
#include "reactor-uc/util.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/timer.h"
#include "reactor-uc/trigger.h"
#include "reactor-uc/queues.h"
#include "reactor-uc/macros_internal.h"
#include "reactor-uc/macros_api.h"
#include <assert.h>
#include <stdio.h>

#endif
