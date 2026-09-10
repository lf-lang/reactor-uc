#include "reactor-uc/reaction.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/port.h"
#include "reactor-uc/trigger.h"

size_t Reaction_get_level(Reaction* self) {
  if (self->level == LF_LEVEL_IN_PROGRESS) {
    LF_ERR(ENV, "Dependency cycle through %s reaction %d: levels cannot be assigned", self->parent->name, self->index);
    throw("Dependency cycle in the reaction graph");
  }
  if (self->level < 0) {
    self->level = LF_LEVEL_IN_PROGRESS;
    self->level = (int)self->calculate_level(self);
  }
  return self->level;
}

int calculate_port_level(Port* port) {
  int current = -1;

  for (Port* upstream_port = port; upstream_port != NULL;) {
    for (size_t i = 0; i < upstream_port->sources.size; i++) {
      Reaction* source = upstream_port->sources.reactions[i];
      validate(source);
      int source_level = source->get_level(source);
      if (source_level > current) {
        current = source_level;
      }
    }

    Connection* conn = upstream_port->conn_in;
    upstream_port = NULL;
    if (conn != NULL && (conn->super.type == TRIG_CONN || conn->super.type == TRIG_CONN_FEDERATED_OUTPUT)) {
      upstream_port = conn->upstream;
    }
  }

  LF_DEBUG(ENV, "Input port %p has level %d", port, current);
  return current;
}

size_t Reaction_calculate_trigger_level(Reaction* self, Trigger* trigger) {
  int max_level = 0;
  if (trigger->type == TRIG_INPUT || trigger->type == TRIG_OUTPUT) {
    Port* port = (Port*)trigger;
    for (size_t j = 0; j < port->effects.size; j++) {
      if (port->effects.reactions[j] == self) {
        int level_from_port = calculate_port_level(port) + 1;
        if (level_from_port > max_level) {
          max_level = level_from_port;
        }
      }
    }
    for (size_t j = 0; j < port->observers.size; j++) {
      if (port->observers.reactions[j] == self) {
        int level_from_port = calculate_port_level(port) + 1;
        if (level_from_port > max_level) {
          max_level = level_from_port;
        }
      }
    }
  }
  return max_level;
}

size_t Reaction_calculate_level(Reaction* self) {
  size_t max_level = 0;

  // Possibly inherit level from reactions within same reactor with precedence.
  if (self->index >= 1) {
    Reaction* reaction_prev_index = self->parent->reactions[self->index - 1];
    size_t prev_level = reaction_prev_index->get_level(reaction_prev_index) + 1;
    if (prev_level > max_level) {
      max_level = prev_level;
    }
  }

  // Find all Input ports with the current reaction as an effect
  for (size_t i = 0; i < self->parent->triggers_size; i++) {
    Trigger* trigger = self->parent->triggers[i];
    size_t trigger_from_level = Reaction_calculate_trigger_level(self, trigger);
    if (trigger_from_level > max_level) {
      max_level = trigger_from_level;
    }
  }

  // Find all output ports within contained reactors which has marked our reaction
  // as an effect or observer.
  for (size_t i = 0; i < self->parent->children_size; i++) {
    Reactor* child = self->parent->children[i];
    for (size_t j = 0; j < child->triggers_size; j++) {
      Trigger* trigger = child->triggers[j];
      size_t trigger_from_level = Reaction_calculate_trigger_level(self, trigger);
      if (trigger_from_level > max_level) {
        max_level = trigger_from_level;
      }
    }
  }

  return max_level;
}

void Reaction_ctor(Reaction* self, Reactor* parent, void (*body)(Reaction* self), Trigger** effects,
                   size_t effects_size, size_t index, void (*deadline_violation_handler)(Reaction*),
                   interval_t deadline, void (*stp_violation_handler)(Reaction*)) {
  self->body = body;
  self->parent = parent;
  self->effects = effects;
  self->effects_size = effects_size;
  self->effects_registered = 0;
  self->calculate_level = Reaction_calculate_level;
  self->get_level = Reaction_get_level;
  self->index = index;
  self->level = LF_LEVEL_UNSET;
  self->_queued = false;
  self->_next_in_level = NULL;
  self->deadline_violation_handler = deadline_violation_handler;
  self->deadline = deadline;
  self->stp_violation_handler = stp_violation_handler;
#if defined(LF_RUNTIME_EXTENSIONS)
  LfGate_ctor(&self->gate);
  LfGateCondition_ctor(&self->_primary_gate);
#endif
}
