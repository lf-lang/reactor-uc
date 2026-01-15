#ifndef REACTOR_UC_TRACE_OTEL_H
#define REACTOR_UC_TRACE_OTEL_H

#include "reactor-uc/trace.h"

typedef struct OTELTrace {
  Tracer super;
  void* tracer;
  void* current_span;
} OTELTrace;

#endif // REACTOR_UC_TRACE_OTEL_H
