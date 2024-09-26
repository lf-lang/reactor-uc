
#if PLATFORM==POSIX
#include "platform/posix.c"
#elif PLATFORM==RIOT
#include "platform/riot.c"
#else
#error  "NO PLATFORM SPECIFIED"
#endif
