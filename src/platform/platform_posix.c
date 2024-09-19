#include "reactor-uc/platform/platform_posix.h"
#include <assert.h>
#include <stdbool.h>
#include <time.h>

static PlatformPosix platform;

static instant_t convert_timespec_to_ns(struct timespec tp) { return ((instant_t)tp.tv_sec) * BILLION + tp.tv_nsec; }

static struct timespec convert_ns_to_timespec(instant_t time) {
  struct timespec tspec;
  tspec.tv_sec = time / BILLION;
  tspec.tv_nsec = (time % BILLION);
  return tspec;
}

instant_t PlatformPosix_get_physical_time(Platform *self) {
  (void)self;
  struct timespec tspec;
  if (clock_gettime(CLOCK_REALTIME, (struct timespec *)&tspec) != 0) {
    assert(false); // TODO: How should this be handled?
  }
  return convert_timespec_to_ns(tspec);
}

void PlatformPosix_wait_until(Platform *self, instant_t wakeup_time) {
  interval_t sleep_duration = wakeup_time - self->get_physical_time(self);

  const struct timespec tspec = convert_ns_to_timespec(sleep_duration);
  struct timespec remaining;
  nanosleep((const struct timespec *)&tspec, (struct timespec *)&remaining);
}

void PlatformPosix_leave_critical_section(Platform *self) { (void)self; }
void PlatformPosix_enter_critical_section(Platform *self) { (void)self; }

void Platform_ctor(Platform *self) {
  self->enter_critical_section = PlatformPosix_enter_critical_section;
  self->leave_critical_section = PlatformPosix_leave_critical_section;
  self->get_physical_time = PlatformPosix_get_physical_time;
  self->wait_until = PlatformPosix_wait_until;
}

Platform *Platform_new() { return (Platform *)&platform; }
