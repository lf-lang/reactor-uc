#ifndef REACTOR_UC_PORT_H
#define REACTOR_UC_PORT_H

#include "reactor-uc/connection.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger.h"
#include "reactor-uc/environment.h"

typedef struct InputPort InputPort;
typedef struct OutputPort OutputPort;

struct InputPort {
  // methods
  OutputPort* (*get)(InputPort* self);

  // data
  // points to the data of the specialized InputPort
  void *value_ptr;

  // member variables
  Trigger super;
  Connection *connection;
};

struct OutputPort {
  // methods
  void (*trigger_reactions)(OutputPort *self);
  //void (*set)(OutputPort* self);

  // member variables
  Trigger super;
  Connection *conn;
};

void InputPort_ctor(InputPort *self, Reactor *parent, void *value_ptr, Reaction **sources, size_t source_size);
void OutputPort_ctor(OutputPort *self, Reactor *parent, Reaction **effects, size_t effect_size);

OutputPort* InputPort_get(InputPort* self);

void OutputPort_trigger_reactions(OutputPort* self);


#endif
