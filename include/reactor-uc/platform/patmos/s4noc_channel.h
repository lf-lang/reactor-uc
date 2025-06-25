#ifndef REACTOR_UC_S4NOC_CHANNEL_H
#define REACTOR_UC_S4NOC_CHANNEL_H

#include "reactor-uc/network_channel.h"
#include "reactor-uc/environment.h"
#include <pthread.h>
typedef struct FederatedConnectionBundle FederatedConnectionBundle;
typedef struct S4NOCPollChannel S4NOCPollChannel;
typedef struct S4NOCGlobalState S4NOCGlobalState;

#define S4NOC_CHANNEL_BUFFERSIZE 1024

#ifndef S4NOC_CORE_COUNT
#define S4NOC_CORE_COUNT 4
#endif

struct S4NOCGlobalState {
  S4NOCPollChannel *core_channels[S4NOC_CORE_COUNT][S4NOC_CORE_COUNT];
};

extern S4NOCGlobalState s4noc_global_state;

struct S4NOCPollChannel {
  PolledNetworkChannel super;
  NetworkChannelState state;

  FederateMessage output;
  unsigned char write_buffer[S4NOC_CHANNEL_BUFFERSIZE];
  unsigned char receive_buffer[S4NOC_CHANNEL_BUFFERSIZE];
  unsigned int receive_buffer_index;
  unsigned int destination_core;

  FederatedConnectionBundle *federated_connection;
  void (*receive_callback)(FederatedConnectionBundle *conn, const FederateMessage *message);
  pthread_t worker_thread;
};

void S4NOCPollChannel_ctor(S4NOCPollChannel *self, unsigned int destination_core);

#endif
