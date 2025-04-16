#ifndef REACTOR_UC_ENCLAVE_ENVIRONMENT_H
#define REACTOR_UC_ENCLAVE_ENVIRONMENT_H

#include "reactor-uc/environment.h"

#include <pthread.h>

typedef struct EnclaveEnvironment EnclaveEnvironment;

struct EnclaveEnvironment {
  Environment super;
  THREAD_T thread;
};

void EnclaveEnvironment_ctor(EnclaveEnvironment *self, Reactor *main, Scheduler *scheduler, bool fast_mode);
void EnclaveEnvironment_free(EnclaveEnvironment *self);

#endif
