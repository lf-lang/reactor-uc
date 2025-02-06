#include "reactor-uc/reactor-uc.h"
#include "clk.h"
#include "board.h"
#include "periph_conf.h"
#include "timex.h"
#include "ztimer.h"

#define MESSAGE_CONTENT "HELLO"
#define MESSAGE_CONNECTION_ID 42

static void delay(void) {
  if (IS_USED(MODULE_ZTIMER)) {
    ztimer_sleep(ZTIMER_USEC, 1 * US_PER_SEC);
  } else {
    uint32_t loops = (coreclk() / 20);
    for (volatile uint32_t i = 0; i < loops; i++) {
    }
  }
}

UartPolledChannel channel_1;
UartPolledChannel channel_2;
Environment env;
Environment *_lf_environment = &env;
FederateMessage msg;

void receive_callback(FederatedConnectionBundle *conn, const FederateMessage *message) {
  LED0_TOGGLE;

  printf("received packet: %s\n", message->message.tagged_message.payload.bytes);

  channel_2.super.super.send_blocking(&channel_2.super.super, &msg);
  channel_1.super.super.send_blocking(&channel_1.super.super, &msg);
}

int main(void) {
  Environment_ctor(&env, NULL);
  _lf_environment = &env;

  UartPolledChannel_ctor(&channel_1, 0, 9600, UC_UART_DATA_BITS_8, UC_UART_PARITY_EVEN, UC_UART_STOP_BITS_2);
  UartPolledChannel_ctor(&channel_2, 1, 9600, UC_UART_DATA_BITS_8, UC_UART_PARITY_EVEN, UC_UART_STOP_BITS_2);
  channel_1.super.super.register_receive_callback(&channel_1.super.super, &receive_callback, (void *)0x0);
  channel_2.super.super.register_receive_callback(&channel_2.super.super, &receive_callback, (void *)0x1);

  msg.type = MessageType_TAGGED_MESSAGE;
  msg.which_message = FederateMessage_tagged_message_tag;

  TaggedMessage *port_message = &msg.message.tagged_message;
  port_message->conn_id = MESSAGE_CONNECTION_ID;
  const char *message = MESSAGE_CONTENT;

  memcpy(port_message->payload.bytes, message, sizeof(MESSAGE_CONTENT)); // NOLINT
  port_message->payload.size = sizeof(MESSAGE_CONTENT);

  while (1) {
    channel_1.super.poll(&channel_1.super.super);
    channel_2.super.poll(&channel_2.super.super);

    channel_2.super.super.send_blocking(&channel_2.super.super, &msg);
    channel_1.super.super.send_blocking(&channel_1.super.super, &msg);
    LED0_TOGGLE;
    delay();
  };
};
