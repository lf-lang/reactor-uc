#define PORT_NUM 8902
#define IP_ADDR "192.168.1.100"

#include "../../common/receiver.h"

int main() {
  setup_led();
  lf_start();
}