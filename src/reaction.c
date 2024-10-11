#include "reactor-uc/reaction.h"
#include "reactor-uc/port.h"
#include "reactor-uc/trigger.h"

#include <assert.h>

static size_t calculate_input_port_level(Input *port);

size_t Reaction_get_level(Reaction *self) {
  if (self->level < 0) {
    self->level = (int)self->calculate_level(self);
  }
  return self->level;
}

static size_t calculate_input_port_level(Input *port) {
  size_t current = 0;
  if (port->super.conn_in) {
    Output *final_upstream_port = port->super.conn_in->get_final_upstream(port->super.conn_in);
    if (final_upstream_port) {
      for (size_t k = 0; k < final_upstream_port->sources.size; k++) {
        Reaction *upstream = final_upstream_port->sources.reactions[k];
        size_t upstream_level = upstream->get_level(upstream) + 1;
        if (upstream_level > current) {
          current = upstream_level;
        }
      }
    }
  }
  return current;
}

// TODO: Do casuality cycle detection here. A causality cycle will lead to infinite recursion and stack overflow.
size_t Reaction_calculate_level(Reaction *self) {
  size_t max_level = 0;

  // Possibly inherit level from reactions within same reactor with precedence.
  if (self->index >= 1) {
    Reaction *reaction_prev_index = self->parent->reactions[self->index - 1];
    size_t prev_level = reaction_prev_index->get_level(reaction_prev_index);
    if (prev_level > max_level) {
      max_level = prev_level;
    }
  }

  // Find all Input ports with the current reaction as an effect
  for (size_t i = 0; i < self->parent->triggers_size; i++) {
    Trigger *trigger = self->parent->triggers[i];
    if (trigger->type == TRIG_INPUT) {
      Input *port = (Input *)trigger;
      for (size_t j = 0; j < port->effects.size; j++) {
        if (port->effects.reactions[j] == self) {
          size_t level_from_input = calculate_input_port_level(port) + 1;
          if (level_from_input > max_level) {
            max_level = level_from_input;
          }
        }
      }
    }
  }

  return max_level;
}

void Reaction_ctor(Reaction *self, Reactor *parent, void (*body)(Reaction *self), Trigger **effects,
                   size_t effects_size, size_t index) {
  self->body = body;
  self->parent = parent;
  self->effects = effects;
  self->effects_size = effects_size;
  self->effects_registered = 0;
  self->calculate_level = Reaction_calculate_level;
  self->get_level = Reaction_get_level;
  self->index = index;
  self->level = -1;
}
