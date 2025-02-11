#ifndef REACTOR_UC_NO_ENCRYPTION_H
#define REACTOR_UC_NO_ENCRYPTION_H

#include "proto/message.pb.h"
#include "reactor-uc/federated.h"
#include "reactor-uc/encryption_layer.h"

typedef struct NoEncryptionLayer NoEncryptionLayer;

struct NoEncryptionLayer {
  EncryptionLayer super;
  NetworkChannel *network_channel;

  FederateMessage msg;
  FederatedConnectionBundle *bundle;
  unsigned char buffer[1024];
  void (*receive_callback)(FederatedConnectionBundle *conn, const FederateMessage *message);
};

void NoEncryptionLayer_ctor(NoEncryptionLayer *self, NetworkChannel *network_channel);

#endif // REACTOR_UC_NO_ENCRYPTION_H
