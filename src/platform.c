
#ifndef REACTOR_UC_PLATFORM
#error "NOT PLATFORM SPECIFIED"
#endif

#if REACTOR_UC_PLATFORM == POSIX
#include "platform/posix.c"
#elif REACTOR_UC_PLATFORM == RIOT
#include "platform/riot.c"
#else
#error "INVALID VALUE FOR PLATFORM"
#endif
