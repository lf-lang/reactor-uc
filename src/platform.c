#if defined(PLATFORM_POSIX)
#include "platform/posix/posix.c"
#include "platform/posix/tcp_ip_channel.c"
#elif defined(PLATFORM_RIOT)
#include "platform/riot/riot.c"
#elif defined(PLATFORM_ZEPHYR)
#include "platform/zephyr/tcp_ip_channel.c"
#include "platform/zephyr/zephyr.c"
#else
#error "NO PLATFORM SPECIFIED"
#endif
