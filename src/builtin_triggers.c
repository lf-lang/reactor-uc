#include "reactor-uc/builtin_triggers.h"

void Startup_start(Trigger *self) { (void)self; }
void Startup_ctor(Startup *self, Reactor *parent, Reaction **effects, size_t effects_size) {
  Trigger_ctor((Trigger *)self, parent, effects, effects_size, NULL, 0, Startup_start);
}
