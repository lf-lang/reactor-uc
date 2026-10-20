#ifndef TEST_LEVEL_GRAPH_H
#define TEST_LEVEL_GRAPH_H

#include "reactor-uc/connection.h"
#include "reactor-uc/port.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/util.h"

#define TEST_MAX_REACTIONS 3
#define TEST_MAX_TRIGGERS 4
#define TEST_MAX_CHILDREN 3

typedef struct {
  Port super;
  Reaction* effects[TEST_MAX_REACTIONS];
  Reaction* sources[TEST_MAX_REACTIONS];
  Connection* conns_out[2];
} TestPort;

typedef struct {
  Reactor super;
  Reaction reactions[TEST_MAX_REACTIONS];
  Reaction* reaction_ptrs[TEST_MAX_REACTIONS];
  Trigger* triggers[TEST_MAX_TRIGGERS];
  Reactor* children[TEST_MAX_CHILDREN];
} TestReactor;

typedef struct {
  LogicalConnection super;
  Port* downstreams[1];
} TestConnection;

static inline void TestReactor_ctor(TestReactor* self, const char* name, TestReactor* parent, size_t num_reactions) {
  Reactor_ctor(&self->super, name, NULL, parent ? &parent->super : NULL, self->children, 0, self->reaction_ptrs,
               num_reactions, self->triggers, 0);
  for (size_t i = 0; i < num_reactions; i++) {
    Reaction_ctor(&self->reactions[i], &self->super, NULL, NULL, 0, i, NULL, 0, NULL);
    self->reaction_ptrs[i] = &self->reactions[i];
  }
  if (parent) {
    parent->children[parent->super.children_size++] = &self->super;
  }
}

static inline void TestReactor_add_port(TestReactor* self, TestPort* port, TriggerType type) {
  Port_ctor(&port->super, type, &self->super, NULL, 0, port->effects, 0, port->sources, 0, NULL, 0, port->conns_out, 2);
  self->triggers[self->super.triggers_size++] = &port->super.super;
}

static inline void TestPort_add_effect(TestPort* port, Reaction* reaction) {
  port->effects[port->super.effects.size++] = reaction;
  port->super.effects.num_registered++;
}

static inline void TestPort_add_source(TestPort* port, Reaction* reaction) {
  port->sources[port->super.sources.size++] = reaction;
  port->super.sources.num_registered++;
}

static inline void TestConnection_connect(TestConnection* self, TestReactor* parent, TestPort* from, TestPort* to) {
  LogicalConnection_ctor(&self->super, &parent->super, self->downstreams, 1);
  lf_connect(&self->super.super, &from->super, &to->super);
}

#endif
