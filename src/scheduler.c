
#if defined(SCHEDULER_DYNAMIC)
#include "./schedulers/dynamic/scheduler.c"
#elif defined(SCHEDULER_STATIC)
#include "schedulers/static/scheduler.c"
#include "schedulers/static/instructions.c"
#else
#error "No scheduler specified"
#endif
