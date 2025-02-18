#include "reactor-uc/physical_clock.h"

void PhysicalClock_set_time(PhysicalClock *self, instant_t time) {
  self->offset = time - self->platform->get_physical_time(self->platform);
}

instant_t PhysicalClock_get_time(PhysicalClock *self) {
  return self->platform->get_physical_time(self->platform) + self->offset;
}

void PhysicalClock_ctor(PhysicalClock *self, Platform *platform) {
  self->platform = platform;
  self->offset = 0;
  self->get_time = PhysicalClock_get_time;
  self->set_time = PhysicalClock_set_time;
}