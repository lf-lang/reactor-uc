#ifndef REACTOR_UC_ENVIRONMENT_ENCLAVED_H
#define REACTOR_UC_ENVIRONMENT_ENCLAVED_H

#include "reactor-uc/environment.h"
#include "reactor-uc/network_channel.h"
#include "reactor-uc/startup_coordinator.h"
#include "reactor-uc/clock_synchronization.h"
#include "reactor-uc/physical_clock.h"

typedef struct EnclavedEnvironment EnclavedEnvironment;

struct EnclavedEnvironment {
  Environment super;
  Environment **enclave_environments;
  pthread_t *enclave_threads;
  size_t num_enclaves;
};

void EnclavedEnvironment_ctor(EnclavedEnvironment *self, Reactor *main, Scheduler *scheduler, bool fast_mode);
void EnclavedEnvironment_free(EnclavedEnvironment *self);

#endif
