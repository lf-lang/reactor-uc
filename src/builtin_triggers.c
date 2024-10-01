#include "reactor-uc/builtin_triggers.h"

void Startup_ctor(Startup *self, Reactor *parent, Reaction **effects, size_t effects_size) {
  Trigger_ctor((Trigger *)self, TRIG_STARTUP, parent, effects, effects_size, NULL, 0, NULL, NULL, NULL, NULL);
}

void Shutdown_ctor(Shutdown *self, Reactor *parent, Reaction **effects, size_t effects_size) {
  Trigger_ctor((Trigger *)self, TRIG_SHUTDOWN, parent, effects, effects_size, NULL, 0, NULL, NULL, NULL, NULL);
}
