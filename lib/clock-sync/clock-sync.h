#ifndef REACTOR_UC_CLOCK_SYNC_H
#define REACTOR_UC_CLOCK_SYNC_H

#include "reactor-uc/reactor-uc.h"

typedef enum { CLOCK_SYNC, CLOCK_DELAY_REQ, CLOCK_DELAY_RESP } ClockSyncMessageType;
typedef struct {
  ClockSyncMessageType type;
  instant_t time;
} ClockSyncMessage;

#endif