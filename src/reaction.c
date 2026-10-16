#include "reactor-uc/reaction.h"
#include "reactor-uc/logging.h"
#include "reactor-uc/port.h"
#include "reactor-uc/trigger.h"

// Marks a reaction whose level is being computed, so that if we encounter it again
// we know there is a cycle in the dependency graph.
#define LEVEL_IN_PROGRESS (-2)

size_t Reaction_get_level(Reaction* self) {
  if (self->level == LEVEL_IN_PROGRESS) {
    LF_ERR(ENV, "Dependency cycle through %s reaction %d: levels cannot be assigned", self->parent->name,
           (int)self->index);
    throw("Dependency cycle in the reaction graph");
  }
  if (self->level < 0) {
    self->level = LEVEL_IN_PROGRESS;
    self->level = (int)self->calculate_level(self);
  }
  return self->level;
}

/** @brief Whether @p writer and @p reader are declared in different exclusion groups. */
static bool are_exclusive(const Reaction* writer, const Reaction* reader) {
  if (writer->parent != reader->parent || writer->exclusion_group == 0 || reader->exclusion_group == 0) {
    return false;
  }
  return writer->exclusion_group != reader->exclusion_group;
}

/**
 * @brief The highest level among the reactions that can write the value @p reader sees on the @p port.
 *
 * Follows the chain of logical connections backwards from @p port and counts the writers of every
 * port on it, including intermediate ones: a parent reaction can write an input that its child
 * forwards inward. We stop at the first delayed or physical connection, because those are the only ones that can break
 * the chain. Writers in different exclusion groups of the reader's own reactor are skipped, since they never execute in
 * the same tag as @p reader.
 *
 * @return The highest writer level, or -1 if nothing writes the port within the tag.
 */
static int calculate_port_level(Port* port, Reaction* reader) {
  int current = -1;
  for (Port* upstream = port; upstream != NULL;) {
    for (size_t i = 0; i < upstream->sources.size; i++) {
      Reaction* source = upstream->sources.reactions[i];
      validate(source);
      if (are_exclusive(source, reader)) {
        continue;
      }
      int source_level = (int)source->get_level(source);
      if (source_level > current) {
        current = source_level;
      }
    }

    Connection* conn = upstream->conn_in;
    upstream = NULL;
    if (conn != NULL && (conn->super.type == TRIG_CONN || conn->super.type == TRIG_CONN_FEDERATED_OUTPUT)) {
      upstream = conn->upstream;
    }
  }

  LF_DEBUG(ENV, "Port %p has level %d", port, current);
  return current;
}

size_t Reaction_calculate_trigger_level(Reaction* self, Trigger* trigger) {
  int max_level = 0;
  if (trigger->type == TRIG_INPUT || trigger->type == TRIG_OUTPUT) {
    Port* port = (Port*)trigger;
    for (size_t j = 0; j < port->effects.size; j++) {
      if (port->effects.reactions[j] == self) {
        int level_from_port = calculate_port_level(port, self) + 1;
        if (level_from_port > max_level) {
          max_level = level_from_port;
        }
      }
    }
    for (size_t j = 0; j < port->observers.size; j++) {
      if (port->observers.reactions[j] == self) {
        int level_from_port = calculate_port_level(port, self) + 1;
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
  self->level = -1;
  self->deadline_violation_handler = deadline_violation_handler;
  self->deadline = deadline;
  self->stp_violation_handler = stp_violation_handler;
  self->is_stp_violated = false;
  self->exclusion_group = 0;
#if LF_RUNTIME_EXTENSIONS
  LfGate_ctor(&self->gate);
#endif
}

void Reaction_set_exclusion_group(Reaction* self, uint8_t group) { self->exclusion_group = group; }
