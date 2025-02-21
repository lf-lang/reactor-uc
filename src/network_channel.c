#include "reactor-uc/network_channel.h"

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
#ifdef NETWORK_CHANNEL_COAP
#include "platform/riot/coap_udp_ip_channel.c"
#endif
#ifdef NETWORK_CHANNEL_UART
#include "platform/riot/uart_channel.c"
#endif

#elif defined(PLATFORM_PICO)
#ifdef NETWORK_CHANNEL_TCP_POSIX
#error "NETWORK_POSIC_TCP not supported on PICO"
#endif
// #ifdef NETWORK_CHANNEL_UART
#include "platform/pico/uart_channel.c"
// #endif

#elif defined(PLATFORM_FLEXPRET)
#ifdef NETWORK_CHANNEL_TCP_POSIX
#error "NETWORK_POSIC_TCP not supported on FlexPRET"
#endif

#elif defined(PLATFORM_PATMOS)
#ifdef NETWORK_CHANNEL_TCP_POSIX
#error "NETWORK_POSIX_TCP not supported on Patmos"
#endif
#else
#error "Platform not supported"
#endif

char *NetworkChannel_state_to_string(NetworkChannelState state) {
  switch (state) {
  case NETWORK_CHANNEL_STATE_UNINITIALIZED:
    return "UNINITIALIZED";
  case NETWORK_CHANNEL_STATE_OPEN:
    return "OPEN";
  case NETWORK_CHANNEL_STATE_CONNECTION_IN_PROGRESS:
    return "CONNECTION_IN_PROGRESS";
  case NETWORK_CHANNEL_STATE_CONNECTION_FAILED:
    return "CONNECTION_FAILED";
  case NETWORK_CHANNEL_STATE_CONNECTED:
    return "CONNECTED";
  case NETWORK_CHANNEL_STATE_LOST_CONNECTION:
    return "LOST_CONNECTION";
  case NETWORK_CHANNEL_STATE_CLOSED:
    return "CLOSED";
  }

  return "UNKNOWN";
}
