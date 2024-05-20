#ifndef REACTOR_UC_EVENT_H
#define REACTOR_UC_EVENT_H

#include "reactor-uc/tag.h"
#include "reactor-uc/trigger.h"

typedef struct {
  Trigger *trigger;
  tag_t tag;
} Event;

#endif
