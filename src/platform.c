#if defined(PLATFORM_POSIX)
#include "platform/posix.c"
#elif defined(PLATFORM_RIOT)
#include "platform/riot.c"
#elif defined(PLATFORM_ZEPHYR)
#include "platform/zephyr.c"
#else
#error "NO PLATFORM SPECIFIED"
#endif
