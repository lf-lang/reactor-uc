
#if defined(SCHEDULER_DYNAMIC)
#include "./schedulers/dynamic/scheduler.c"
#elif defined(SCHEDULER_STATIC)
#include "schedulers/static/scheduler.c"
#include "schedulers/static/instructions.c"
#include "schedulers/static/circular_buffer.c"
#else
#error "Unsupported scheduler macro"
#endif
