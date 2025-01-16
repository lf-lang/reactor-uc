#ifndef REACTOR_UC_I2C_CHANNEL_H
#define REACTOR_UC_I2C_CHANNEL_H
#include "reactor-uc/network_channel.h"
#include "reactor-uc/environment.h"

#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

typedef struct I2CChannel I2CChannel;
typedef struct FederatedConnectionBundle FederatedConnectionBundle;

#define I2C_CHANNEL_BUFFERSIZE 1024
#define I2C_CHANNEL_EXPECTED_CONNECT_DURATION MSEC(10) //TODO:


struct I2CChannel {
  NetworkChannel super;
  NetworkChannelState state;

  FederateMessage output;
  unsigned char write_buffer[TCP_IP_CHANNEL_BUFFERSIZE];
  unsigned char read_buffer[TCP_IP_CHANNEL_BUFFERSIZE];
  unsigned int read_index;

  const struct device *const i2c_dev;
  uint32_t i2c_config;
  uint16_t address;

  FederatedConnectionBundle *federated_connection;
  void (*receive_callback)(FederatedConnectionBundle *conn, const FederateMessage *message);
};

void I2CChannel_ctor(I2CChannel *self, Environment *env);

#endif
