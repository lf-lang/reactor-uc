#include "reactor-uc/clock_synchronization.h"
#include "reactor-uc/environment.h"

lf_ret_t ClockSynchronization_ask_neighbors_for_priorities(ClockSynchronization *self) {
  (void)self;
  self->env->enter_critical_section(self->env);
  self->env->leave_critical_section(self->env);
  return LF_OK;
}

lf_ret_t ClockSynchronization_update_priorities(ClockSynchronization *self) {
  (void)self;
  self->env->enter_critical_section(self->env);
  self->env->leave_critical_section(self->env);
  return LF_OK;
}

void ClockSynchronization_handle_message_callback(ClockSynchronization *self, const ClockSyncMessage *msg,
                                                  size_t bundle_idx) {
  (void)self;
  (void)msg;
  (void)bundle_idx;
  self->env->enter_critical_section(self->env);
  self->env->leave_critical_section(self->env);
}

void ClockSynchronization_ctor(ClockSynchronization *self, Environment *env, NeighborClock *neighbor_clock,
                               size_t num_neighbors, bool is_grandmaster) {
  self->env = env;
  self->neighbor_clock = neighbor_clock;
  self->num_neighbours = num_neighbors;
  self->is_grandmaster = is_grandmaster;
  self->has_initial_sync =
      is_grandmaster; // Unless we are the grandmaster, we need to sync with the grandmaster to get initial sync.
  self->master_neighbor_index = -1;
  self->handle_message_callback = ClockSynchronization_handle_message_callback;
  self->ask_neighbors_for_priorities = ClockSynchronization_ask_neighbors_for_priorities;
  self->update_priorities = ClockSynchronization_update_priorities;
}