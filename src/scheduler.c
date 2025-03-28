
#if defined(SCHEDULER_DYNAMIC)
#include "./schedulers/dynamic/scheduler.c"
#if defined(FEDERATED)
#include "./schedulers/dynamic/scheduler_federated.c"
#endif
#elif defined(SCHEDULER_STATIC)
#include "schedulers/static/scheduler.c"
#include "schedulers/static/instructions.c"
#else
#endif
