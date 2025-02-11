//
// Created by tanneberger on 2/5/25.
//

#include "reactor-uc/encryption_layers/no_encryption/no_encryption.h"

#include "reactor-uc/serialization.h"

lf_ret_t NoEncryptionLayer_send_message(EncryptionLayer *untyped_self, const FederateMessage *msg) {
  NoEncryptionLayer *self = (NoEncryptionLayer *)untyped_self;

  int size = serialize_to_protobuf(msg, self->buffer + sizeof(MessageFraming), 1024);

  if (size < 0) {
    return LF_ERR;
  }

  size += generate_message_framing(self->buffer, size, UC_NO_ENCRYPTION);

  return self->network_channel->send_blocking(self->network_channel, (char *)self->buffer, size);
}

void NoEncryptionLayer_register_callback(EncryptionLayer *untyped_self,
                                         void (*receive_callback)(FederatedConnectionBundle *conn,
                                                                  const FederateMessage *message),
                                         FederatedConnectionBundle *bundle) {
  NoEncryptionLayer *self = (NoEncryptionLayer *)untyped_self;

  self->bundle = bundle;
  self->receive_callback = receive_callback;
}

int _NoEncryptionLayer_receive_callback(EncryptionLayer *untyped_self, char *buffer, ssize_t buffer_size) {
  (void)buffer_size; // TODO: FIXME
  NoEncryptionLayer *self = (NoEncryptionLayer *)untyped_self;
  const MessageFraming *message_framing = (MessageFraming *)buffer;

  if (validate_message_framing((unsigned char *)buffer, UC_NO_ENCRYPTION) != LF_OK) {
  }

  int consumed_bytes = deserialize_from_protobuf(&self->msg, (unsigned char *)buffer, message_framing->message_size);

  if (consumed_bytes < 0) {
    // ERROR
  }

  self->receive_callback(self->bundle, &self->msg);

  return consumed_bytes + sizeof(MessageFraming);
}

void NoEncryptionLayer_ctor(NoEncryptionLayer *self, NetworkChannel *network_channel) {
  self->network_channel = network_channel;
  self->super.send_message = NoEncryptionLayer_send_message;
  self->super.register_receive_callback = NoEncryptionLayer_register_callback;

  network_channel->register_encryption_layer(network_channel, (EncryptionLayer *)self,
                                             _NoEncryptionLayer_receive_callback);
}