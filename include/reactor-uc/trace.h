#ifndef REACTOR_UC_TRACE_H
#define REACTOR_UC_TRACE_H

#include "reaction.h"

typedef struct Tracer {
  void (*open_span)(struct Tracer*, Reaction*);
  void (*close_span)(struct Tracer*);
} Tracer;

#endif // REACTOR_UC_TRACE_H
