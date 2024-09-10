#include "reactor-uc/environment.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/timer.h"
#include "reactor-uc/port.h"
#include <stdio.h>

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define lf_set(port, input) port->value = input; port->super.trigger_reactions(&port->super);
#define lf_get(port) port->value

// Port Definitions
typedef struct {
  // Value
  int value;

  // Generic-Port
  OutputPort super;
} IntOutputPort;

typedef struct {
  // Generic-Port
  InputPort super;
} IntInputPort;

// Reaction Definitions
typedef struct {
  // Output Ports
  IntOutputPort* produce;

  // Generic-Reaction
  Reaction super;
} ProducerReaction;

typedef struct {
  // Input Ports
  IntInputPort* feedback;

  // Generic-Reaction
  Reaction super;
} FeedbackReaction;

typedef struct {
  // Input Ports
  IntInputPort* in;

  // Output Ports
  IntOutputPort* out;

  // Generic-Reaction
  Reaction super;
} ConsumerReaction;

// Trigger Definitions
typedef struct {
  // Downstream Reactions
  Reaction *effects[1];

  // Timer
  Timer super;
} ProducerTimer;


// Reactor Definitions

struct ConsumerReactor {
  // output ports
  IntOutputPort out;
  Reaction *out_down_streams[1];
  // input ports
  IntInputPort in;

  // reactions
  ConsumerReaction consumer_reaction;

  Reactor super;
  Reaction *reactions[1]; // 1 reaction
  Trigger *triggers[1]; // output port
};


struct ProducerReactor {
  // nested reactors
  struct ConsumerReactor consumer_reactor;

  // output ports
  IntOutputPort produce;
  Reaction*producer_down_streams[1];
  // input ports
  IntInputPort feedback;

  // reactions
  ProducerReaction producer_reaction;
  FeedbackReaction feedback_reaction;

  // timers
  ProducerTimer timer;

  // reactor state
  int state;

  Reactor super;
  Reaction *reactions[2]; // important for level assignment algorithm
  Trigger *triggers[2]; // timer and output port
  Reactor* children[1]; // important for level assignment algorithm
};

typedef struct ProducerReactor ProducerReactor;
typedef struct ConsumerReactor ConsumerReactor;

// reactions

// reaction Producer 0

int Producer_0_body(Reaction *untyped_reaction) {
  // reactor
  ProducerReactor *self = (ProducerReactor*) (untyped_reaction->parent->typed);

  // output ports
  IntOutputPort* producer = &self->produce;

  // Begin User Code

  printf("Reaction Producer 0 fires\n");
  lf_set(producer, self->state);

  // End User Code

  return 0;
}

int Producer_0_calculate_level(Reaction* untyped_self) {
  ProducerReaction *self = (ProducerReaction *) (untyped_self->typed);
  struct ProducerReactor *parent_reactor= (struct ProducerReactor*) untyped_self->parent->typed;

  int max_level = 0;

  if (self->super.index >= 1) {
    Reaction* reaction_prev_index = parent_reactor->reactions[self->super.index - 1];
    max_level = max(max_level, reaction_prev_index->calculate_level(reaction_prev_index) + 1);
  }

  self->super.level = max_level;

  return max_level;
}

void ProducerReaction_0_ctor(ProducerReaction *self, Reactor *parent, IntOutputPort* produce) {
  self->produce = produce;
  Reaction_ctor(&self->super, parent, Producer_0_body, 0, Producer_0_calculate_level, self);
}


// reaction Producer 1
int Producer_1_body(Reaction *untyped_reaction) {
  // reactor
  struct ProducerReactor *self = (struct ProducerReactor*) untyped_reaction->parent->typed;

  // input ports
  IntOutputPort* feedback = self->feedback.super.get(&self->feedback.super)->typed;

  // Begin User Code

  printf("Reaction Producer 1 fires\n");
  printf("Feedback send: %i\n", lf_get(feedback));

  self->state = lf_get(feedback) + 1;

  // End User Code

  return 0;
}

int Producer_1_calculate_level(Reaction* untyped_self) {
  // start codegen
  FeedbackReaction* self = (FeedbackReaction*) (untyped_self->typed);
  struct ProducerReactor *parent_reactor= (struct ProducerReactor*) untyped_self->parent->typed;
  // end codegen

  if (self->super.level > 0) {
    return self->super.level;
  }

  int max_level = 0;

  if (self->super.index >= 1) {
    Reaction* reaction_prev_index = parent_reactor->reactions[self->super.index - 1];
    max_level = max(max_level, reaction_prev_index->calculate_level(reaction_prev_index) + 1);
  }

  // start codegen
  ProducerReactor* parent = untyped_self->parent->typed;
  BasePort* base_port = self->feedback->super.get(&self->feedback->super);
  IntOutputPort* typed_port = base_port->typed;
  // end codegen

  Trigger* trigger = &typed_port->super.super;
  for (int i = 0; i < trigger->sources_registered; i++) {
    max_level = max(max_level, trigger->sources[i]->calculate_level(trigger->sources[i]));
  }

  self->super.level = max_level;
  return max_level;
}

void ProducerReaction_1_ctor(FeedbackReaction* self, Reactor *parent, IntInputPort* feedback) {
  self->feedback = feedback;
  Reaction_ctor(&self->super, parent, Producer_1_body, 1, Producer_1_calculate_level, self);
}


// reaction Consumer 0
int Consumer_0_body(Reaction *untyped_reaction) {
  // reactor
  struct ConsumerReactor *self = (struct ConsumerReactor*) untyped_reaction->parent->typed;

  // input ports
  IntOutputPort* in = self->in.super.get(&self->in.super)->typed;

  // output ports
  IntOutputPort* out = &self->out;

  // Begin User Code

  printf("Reaction Consumer 0 fires\n");
  printf("Producer send: %i\n", lf_get(in));

  lf_set(out, lf_get(in));

  // End User Code

  return 0;
}

int Consumer_0_calculate_level(Reaction* untyped_self) {
  ConsumerReaction* self = (ConsumerReaction*) (untyped_self->typed);
  struct ConsumerReactor* parent_reactor= (struct ConsumerReactor*) untyped_self->parent->typed;

  if (self->super.level > 0) {
    return self->super.level;
  }

  int max_level = 0;

  if (self->super.index >= 1) {
    Reaction* reaction_prev_index = parent_reactor->reactions[self->super.index - 1];
    max_level = max(max_level, reaction_prev_index->calculate_level(reaction_prev_index) + 1);
  }

  // dyn part
  BasePort* base_port = self->in->super.get(&self->in->super);
  IntOutputPort* typed_port = base_port->typed;
  Trigger* trigger = &typed_port->super.super;
  // dyn part end

  for (int i = 0; i < trigger->sources_registered; i++) {
    max_level = max(max_level, trigger->sources[i]->calculate_level(trigger->sources[i]));
  }

  self->super.level = max_level;
  return max_level;
}


void ConsumerReaction_0_ctor(ConsumerReaction* self, Reactor *parent, IntInputPort* in, IntOutputPort* out) {
  self->in = in;
  self->out = out;
  Reaction_ctor(&self->super, parent, Consumer_0_body, 0, Consumer_0_calculate_level, self);
}

// reactors

void ConsumerReactor_ctor(struct ConsumerReactor* self, Environment *env) {
  // constructing reactor
  Reactor_ctor(&self->super, env, self, NULL, 0, self->reactions, 1, self->triggers, 1);

  // reaction list
  self->reactions[0] = &self->consumer_reaction.super;

  // trigger list
  self->triggers[0] = &self->out.super.super;

  // constructing output ports
  InputPort_ctor(&self->in.super, &self->super, &self->in, NULL, 0);

  // constructing output ports
  OutputPort_ctor(&self->out.super, &self->super, &self->out, self->out_down_streams, 1);

  // constructing reactions
  ConsumerReaction_0_ctor(&self->consumer_reaction, &self->super, &self->in, &self->out);
}

void ProducerReactor_ctor(ProducerReactor *self, Environment *env) {
  // constructing child reactors
  ConsumerReactor_ctor(&self->consumer_reactor, env);
  self->children[0] = &self->consumer_reactor.super;

  // constructing reactor
  Reactor_ctor(&self->super, env, self, self->children, 1, self->reactions, 2, self->triggers, 2);

  // initializing state
  self->state = 0;

  // reaction list
  self->reactions[0] = &self->producer_reaction.super;
  self->reactions[1] = &self->feedback_reaction.super;

  // trigger list
  self->triggers[0] = &self->timer.super.super;
  self->triggers[1] = &self->produce.super.super;

  // constructing input ports
  InputPort_ctor(&self->feedback.super, &self->super, &self->feedback, NULL, 0);

  // constructing output ports
  OutputPort_ctor(&self->produce.super, &self->super, &self->produce, self->producer_down_streams, 1);

  // constructing reactions
  ProducerReaction_0_ctor(&self->producer_reaction, &self->super, &self->produce);
  ProducerReaction_1_ctor(&self->feedback_reaction, &self->super, &self->feedback);

  // constructing timers
  Timer_ctor(&self->timer.super, &self->super, 0, SEC(1), self->timer.effects, 1);

  // effect configurations
  Trigger* trigger;

  trigger = &self->timer.super.super;
  trigger->register_effect(trigger, &self->producer_reaction.super);

  trigger = &self->produce.super.super;
  trigger->register_effect(trigger, &self->consumer_reactor.consumer_reaction.super);

  trigger = &self->consumer_reactor.out.super.super;
  trigger->register_effect(trigger, &self->feedback_reaction.super);

  // port binding
  InputPort_register_inward_binding(&self->consumer_reactor.in.super, &self->produce.super.base);
  InputPort_register_inward_binding(&self->feedback.super, &self->consumer_reactor.out.super.base);
}

int main() {
  ProducerReactor producer_reactor;
  Environment env;
  Environment_ctor(&env, &producer_reactor.super);
  ProducerReactor_ctor(&producer_reactor, &env);
  env.assemble(&env);
  env.calculate_levels(&env);
  env.start(&env);
}
