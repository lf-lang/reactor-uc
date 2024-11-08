#if defined(PLATFORM_POSIX)
#ifdef NETWORK_CHANNEL_TCP_POSIX
#include "platform/posix/tcp_ip_channel.c"
#endif

#elif defined(PLATFORM_ZEPHYR)
#ifdef NETWORK_CHANNEL_TCP_POSIX
#include "platform/posix/tcp_ip_channel.c"
#endif

#elif defined(PLATFORM_RIOT)
#ifdef NETWORK_CHANNEL_TCP_POSIX
#include "platform/posix/tcp_ip_channel.c"
#endif

#elif defined(PLATFORM_PICO)
#ifdef NETWORK_CHANNEL_TCP_POSIX
#error "NETWORK_POSIC_TCP not supported on PICO"
#endif

#else
#error "Platform not supported"
#endif
