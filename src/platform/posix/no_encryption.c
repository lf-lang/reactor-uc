#include "reactor-uc/platform/posix/no_encryption.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/serialization.h"

#define NO_ENCRYPTION_LAYER_ERR(fmt, ...) LF_ERR(NET, "NoEncryptionLayer: " fmt, ##__VA_ARGS__)
#define NO_ENCRYPTION_LAYER_WARN(fmt, ...) LF_WARN(NET, "NoEncryptionLayer: " fmt, ##__VA_ARGS__)
#define NO_ENCRYPTION_LAYER_INFO(fmt, ...) LF_INFO(NET, "NoEncryptionLayer: " fmt, ##__VA_ARGS__)
#define NO_ENCRYPTION_LAYER_DEBUG(fmt, ...) LF_DEBUG(NET, "NoEncryptionLayer: " fmt, ##__VA_ARGS__)

lf_ret_t NoEncryptionLayer_send_message(EncryptionLayer *untyped_self, const FederateMessage *msg) {
  NoEncryptionLayer *self = (NoEncryptionLayer *)untyped_self;

  int size = serialize_to_protobuf(msg, self->buffer + sizeof(MessageFraming), 1024);

  if (size < 0) {
    return LF_ERR;
  }

  size += generate_message_framing(self->buffer, size, UC_NO_ENCRYPTION);

  return self->super.network_channel->send_blocking(self->super.network_channel, (char *)self->buffer, size);
}

void NoEncryptionLayer_register_callback(EncryptionLayer *untyped_self,
                                         void (*receive_callback)(FederatedConnectionBundle *conn,
                                                                  const FederateMessage *message),
                                         FederatedConnectionBundle *bundle) {
  NoEncryptionLayer *self = (NoEncryptionLayer *)untyped_self;
  NO_ENCRYPTION_LAYER_DEBUG("Registering with NoEncryptionLayer %p", bundle);

  self->bundle = bundle;
  self->receive_callback = receive_callback;
}

void _NoEncryptionLayer_receive_callback(EncryptionLayer *untyped_self, const char *buffer, ssize_t buffer_size) {
  (void)buffer_size; // TODO: FIXME
  NoEncryptionLayer *self = (NoEncryptionLayer *)untyped_self;
  MessageFraming message_framing;
  memcpy(&message_framing, buffer, sizeof(MessageFraming));

  NO_ENCRYPTION_LAYER_DEBUG("Received Callback from NetworkChannel Buffer Size: %d", buffer_size);

  if (validate_message_framing((unsigned char *)buffer, UC_NO_ENCRYPTION) != LF_OK) {
    NO_ENCRYPTION_LAYER_ERR("Invalid Message Framing");
  }

  int consumed_bytes = deserialize_from_protobuf(&self->msg, (unsigned char *)buffer + sizeof(MessageFraming),
                                                 message_framing.message_size);

  if (consumed_bytes < 0) {
    NO_ENCRYPTION_LAYER_ERR("Consumed Bytes is zero");
  }

  NO_ENCRYPTION_LAYER_DEBUG("Calling Receive Callback");
  if (self->receive_callback == NULL) {
    NO_ENCRYPTION_LAYER_ERR("NoEncryptionLayer callback is zero!");
  }
  self->receive_callback(self->bundle, &self->msg);
}

void NoEncryptionLayer_ctor(NoEncryptionLayer *self, NetworkChannel *network_channel) {
  self->bundle = NULL;
  self->super.network_channel = network_channel;
  self->super.send_message = NoEncryptionLayer_send_message;
  self->super.register_receive_callback = NoEncryptionLayer_register_callback;
  network_channel->register_receive_callback(network_channel, &self->super, _NoEncryptionLayer_receive_callback);

  NO_ENCRYPTION_LAYER_DEBUG("EncryptionLayer: %p NetworkChannel pointer: %p", self, self->super.network_channel);

}
