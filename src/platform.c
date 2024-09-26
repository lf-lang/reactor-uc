
#ifndef PLATFORM
#error "NOT PLATFORM SPECIFIED"
#endif

#if PLATFORM == POSIX
#include "platform/posix.c"
#elif PLATFORM == RIOT
#include "platform/riot.c"
#else
#error "INVALID VALUE FOR PLATFORM"
#endif
