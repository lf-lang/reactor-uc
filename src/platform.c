#if defined(PLATFORM_POSIX)
#include "platform/posix.c"
#elif defined(PLATFORM_RIOT)
#include "platform/riot.c"
#else
#error "NO PLATFORM SPECIFIED"
#endif
