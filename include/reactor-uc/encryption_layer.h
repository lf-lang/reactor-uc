#ifndef REACTOR_UC_ENCRYPTIONLAYER_H
#define REACTOR_UC_ENCRYPTIONLAYER_H

#include "proto/message.pb.h"
#include "reactor-uc/tag.h"
#include "reactor-uc/error.h"
#include "reactor-uc/federated.h"

typedef struct NetworkChannel NetworkChannel;
typedef struct EncryptionLayer EncryptionLayer;
typedef enum EncryptionIdentifier EncryptionIdentifier;

enum EncryptionIdentifier { UC_NO_ENCRYPTION = 0x0 };

struct EncryptionLayer {
  /**
   @brief Specifies which kind of encryption is used for this NetworkChannel
  */
  EncryptionIdentifier encryption_identifier;
  /**
   @brief NetworkChannel underneeth this encryption layer
  */
  NetworkChannel *network_channel;

  /**
    @brief Will serialize the given message and encrypt it and then send it via the configured NetworkChannel
  */
  lf_ret_t (*send_message)(EncryptionLayer *self, const FederateMessage *msg);

  /**
    @brief Registers the callback of the FederatedConnectionBundle with this encryption layer
  */
  void (*register_receive_callback)(EncryptionLayer *self,
                                    void (*receive_callback)(FederatedConnectionBundle *conn,
                                                             const FederateMessage *message),
                                    FederatedConnectionBundle *conn);
};

#endif // REACTOR_UC_ENCRYPTIONLAYER_H