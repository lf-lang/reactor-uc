#include "reactor-uc/platform/zephyr/i2c_channel.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/environment.h"
#include "reactor-uc/serialization.h"

#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#define I2C_CHANNEL_ERR(fmt, ...) LF_ERR(NET, "I2CChannel: " fmt, ##__VA_ARGS__)
#define I2C_CHANNEL_WARN(fmt, ...) LF_WARN(NET, "I2CChannel: " fmt, ##__VA_ARGS__)
#define I2C_CHANNEL_INFO(fmt, ...) LF_INFO(NET, "I2CChannel: " fmt, ##__VA_ARGS__)
#define I2C_CHANNEL_DEBUG(fmt, ...) LF_DEBUG(NET, "I2CChannel: " fmt, ##__VA_ARGS__)

static lf_ret_t I2CChannel_open_connection(NetworkChannel *untyped_self) {
  I2C_CHANNEL_DEBUG("Open connection");
  (void) untyped_self;
  return LF_OK;
}

static void I2CChannel_close_connection(NetworkChannel *untyped_self) {
  I2C_CHANNEL_DEBUG("Close connection");
  (void) untyped_self;
}

static void I2CChannel_free(NetworkChannel *untyped_self) {
  I2C_CHANNEL_DEBUG("Free");
  (void) untyped_self;
}

static bool I2CChannel_is_connected(NetworkChannel *untyped_self) {
  I2CChannel *self = (I2CChannel *)untyped_self;
  return self->state == NETWORK_CHANNEL_STATE_CONNECTED;
}

static lf_ret_t I2CChannel_send_blocking(NetworkChannel *untyped_self, const FederateMessage *message) {
  I2C_CHANNEL_DEBUG("Send blocking");
  I2CChannel *self = (I2CChannel *)untyped_self;


  if (_TcpIpChannel_get_state(self) == NETWORK_CHANNEL_STATE_CONNECTED) {
    // serializing protobuf into buffer
    char write_buffer[1024];
    int message_size = serialize_to_protobuf(message, write_buffer, 1024);

    if (i2c_write(self->i2c_dev, write_buffer, message_size, self->address)) {
      I2C_CHANNEL_ERR("Fail to configure sensor GY271\n");
      return LF_ERR;
    }
  }


  return LF_ERR;
}

static void I2CChannel_register_receive_callback(NetworkChannel *untyped_self,
                                                       void (*receive_callback)(FederatedConnectionBundle *conn,
                                                                                const FederateMessage *msg),
                                                       FederatedConnectionBundle *conn) {
  I2C_CHANNEL_INFO("Register receive callback");
  I2CChannel *self = (I2CChannel *)untyped_self;

  self->receive_callback = receive_callback;
  self->federated_connection = conn;
}

static lf_ret_t I2CChannel_receive(NetworkChannel* untyped_self, FederateMessage *return_message) {
  I2CChannel *self = (I2CChannel *)untyped_self;

  // calculating the maximum amount of bytes we can read
  int bytes_available = I2C_CHANNEL_BUFFERSIZE - self->read_index;
  bool read_more = true;

  if (self->read_index > 0) {
    I2C_CHANNEL_DEBUG("Has %d bytes in read_buffer from last recv. Trying to deserialize", self->read_index);
    bytes_left = deserialize_from_protobuf(&self->output, self->read_buffer, self->read_index);
    if (bytes_left >= 0) {
      I2C_CHANNEL_DEBUG("%d bytes left after deserialize", bytes_left);
      read_more = false;
    }
  }

  while (read_more) {
    if (0 != i2c_read(self->i2c_dev, self->read_buffer + self->read_index, bytes_available, self->address)) {
      I2C_CHANNEL_ERR("Fail to read bytes from i2c bus\n");
      return LF_ERR;
    }

    self->read_index += bytes_read;
    bytes_left = deserialize_from_protobuf(return_message, self->read_buffer, self->read_index);
    I2C_CHANNEL_DEBUG("%d bytes left after deserialize", bytes_left);
    if (bytes_left < 0) {
      read_more = true;
    } else {
      read_more = false;
    }
  }

  memcpy(self->read_buffer, self->read_buffer + (self->read_index - bytes_left), bytes_left);
  self->read_index = bytes_left;

  return LF_OK;

}

int i2c_read_callback(struct i2c_target_config *config, uint8_t *val) {
  // https://docs.zephyrproject.org/latest/doxygen/html/group__i2c__interface.html#ga4cb0ae2cf41fc2105d1baa5e496e5dae
  if (0 != i2c_read(self->i2c_dev, self->read_buffer + self->read_index, 1, self->address)) {
    I2C_CHANNEL_ERR("Fail to read bytes from i2c bus\n");
    return LF_ERR;
  }


  return 0;
}



void I2CChannel_ctor(I2CChannel *self, Environment *env) {
  assert(self != NULL);
  assert(env != NULL);
  uint32_t i2c_cfg = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_CONTROLLER;

  const struct device *const i2c_dev = DEVICE_DT_GET(I2C_DEV_NODE);
  uint32_t i2c_cfg_tmp;

  if (!device_is_ready(i2c_dev)) {
    I2C_CHANNEL_ERR("device is not ready");
    return;
  }

  if (i2c_configure(i2c_dev, i2c_cfg)) {
    I2C_CHANNEL_ERR("configuration failed");
    return;
  }

  if (i2c_get_config(i2c_dev, &i2c_cfg_tmp)) {
    I2C_CHANNEL_ERR("fetching config from device failed");
    return;
  }

  if (i2c_cfg != i2c_cfg_tmp) {
    I2C_CHANNEL_ERR("not matching configurations on that i2c device");
    return;
  }

  // Super fields
  self->super.expected_connect_duration = I2C_CHANNEL_EXPECTED_CONNECT_DURATION;
  self->super.type = NETWORK_CHANNEL_TYPE_I2C;
  self->super.is_connected = I2CChannel_is_connected;
  self->super.open_connection = I2CChannel_open_connection;
  self->super.close_connection = I2CChannel_close_connection;
  self->super.send_blocking = I2CChannel_send_blocking;
  self->super.register_receive_callback = I2CChannel_register_receive_callback;
  self->super.free = I2CChannel_free;

  // Concrete fields
  self->receive_callback = NULL;
  self->federated_connection = NULL;
  self->i2c_dev = i2c_dev;
  self->i2c_config = i2c_cfg;
  self->state = NETWORK_CHANNEL_STATE_CONNECTED;
}
