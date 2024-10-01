#ifndef REACTOR_UC_TRIGGER_H
#define REACTOR_UC_TRIGGER_H

#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/reactor_element.h"
#include "reactor-uc/trigger_value.h"
#include <stddef.h>

typedef struct Trigger Trigger;
typedef struct SchedulableTrigger SchedulableTrigger;

typedef enum {
  TRIG_TIMER,
  TRIG_LOGICAL_ACTION,
  TRIG_PHYSICAL_ACTION,
  TRIG_INPUT,
  TRIG_OUTPUT,
  TRIG_CONN,
  TRIG_CONN_DELAYED,
  TRIG_CONN_PHYSICAL,
  TRIG_STARTUP,
  TRIG_SHUTDOWN
} TriggerType;

struct SchedulableTrigger {
  Trigger super;
  TriggerValue *trigger_value;
  int (*schedule_at)(SchedulableTrigger *, tag_t, const void *);
  int (*schedule_at_locked)(SchedulableTrigger *, tag_t, const void *);
};

void SchedulableTrigger_ctor(SchedulableTrigger *self, TriggerValue *trigger_value);

struct Trigger {
  TriggerType type;
  Reactor *parent;
  bool is_present;
  Trigger *next; // For chaining together triggers, e.g. shutdown/startup triggers.
  void (*prepare)(Trigger *);
  void (*cleanup)(Trigger *);
} __attribute__((aligned(32)));

void Trigger_ctor(Trigger *self, TriggerType type, Reactor *parent, void (*prepare)(Trigger *),
                  void (*cleanup)(Trigger *));

typedef struct {
  Reaction **effects;
  size_t effects_size;
  size_t effects_registered;
} TriggerEffects;

typedef struct {
  Reaction **sources;
  size_t sources_size;
  size_t sources_registered;
} TriggerSources;
#endif
