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
  // ports
  IntInputPort in;
  IntOutputPort out;

  // reactions
  ConsumerReaction consumer_reaction;

  // connections
  Reaction* down_streams_[1]; // <-- needs to be code generated

  Reactor super;
  Reaction *_reactions[1]; // 1 reaction
  Trigger *_triggers[1]; // output port
};


struct ProducerReactor {
  // nested reactors
  struct ConsumerReactor consumer_reactor;

  // ports
  IntOutputPort producer;
  IntInputPort feedback;

  // connections
  Reaction* down_streams_[1]; // <-- needs to be code generated

  // reactions
  ProducerReaction producer_reaction;
  FeedbackReaction feedback_reaction;

  // timers
  ProducerTimer timer;

  // reactor state
  int state;

  Reactor super;
  Reaction *_reactions[2]; // two reactions
  Trigger *_triggers[2]; // timer and output port
  Reactor* children[1];
};

typedef struct ProducerReactor ProducerReactor;
typedef struct ConsumerReactor ConsumerReactor;

// reaction bodies

int Producer_0_body(Reaction *untyped_reaction) {
  ProducerReactor *self = (ProducerReactor*) (untyped_reaction->parent->typed);
  // Begin User Code

  printf("Reaction Producer 0 fires\n");

  // set macro
  self->producer.value = self->state;
  self->producer.super.trigger_reactions(&self->producer.super);
  // end set macro

  // End User Code

  return 0;
}

int Producer_0_calculate_level(Reaction* untyped_self) {
  ProducerReaction *self = (ProducerReaction *) (untyped_self->typed);
  struct ProducerReactor *parent_reactor= (struct ProducerReactor*) untyped_self->parent->typed;

  int max_level = 0;

  if (self->super.index >= 1) {
    Reaction* reaction_prev_index = parent_reactor->_reactions[self->super.index - 1];
    max_level = max(max_level, reaction_prev_index->calculate_level(reaction_prev_index) + 1);
  }

  self->super.level = max_level;

  return max_level;
}


int Producer_1_body(Reaction *untyped_reaction) {
  struct ProducerReactor *self = (struct ProducerReactor*) untyped_reaction->parent->typed;
  // Begin User Code

  // start get macro
  IntOutputPort * source =(IntOutputPort*)self->feedback.super.get(&self->feedback.super)->typed;
  // end get macro

  printf("Reaction Producer 1 fires\n");
  printf("Feedback send: %i\n", source->value);

  self->state = source->value + 1;

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
    Reaction* reaction_prev_index = parent_reactor->_reactions[self->super.index - 1];
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


int Consumer_0_body(Reaction *untyped_reaction) {
  struct ConsumerReactor *self = (struct ConsumerReactor*) untyped_reaction->parent->typed;

  IntOutputPort * source =(IntOutputPort*)self->in.super.get(&self->in.super)->typed;

  printf("Reaction Consumer 0 fires\n");
  printf("Producer send: %i\n", source->value);

  self->out.value = source->value;
  self->out.super.trigger_reactions(&self->out.super);

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
    Reaction* reaction_prev_index = parent_reactor->_reactions[self->super.index - 1];
    max_level = max(max_level, reaction_prev_index->calculate_level(reaction_prev_index) + 1);
  }

  // dyn part
  BasePort* base_port = self->in->super.get(&self->in->super);
  IntOutputPort* typed_port = base_port->typed;
  // dyn part end

  Trigger* trigger = &typed_port->super.super;
  for (int i = 0; i < trigger->sources_registered; i++) {
    max_level = max(max_level, trigger->sources[i]->calculate_level(trigger->sources[i]));
  }

  self->super.level = max_level;
  return max_level;
}


void ProducerReaction_0_ctor(ProducerReaction *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, Producer_0_body, 0, Producer_0_calculate_level, self);
}


void ProducerReaction_1_ctor(FeedbackReaction* self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, Producer_1_body, 1, Producer_1_calculate_level, self);
}


void ConsumerReaction_0_ctor(ConsumerReaction* self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, Consumer_0_body, 0, Consumer_0_calculate_level, self);
}

void ConsumerReactor_ctor(struct ConsumerReactor* self, Environment *env) {
  self->_reactions[0] = &self->consumer_reaction.super;
  self->super.reactions_size = 1;

  self->_triggers[0] = &self->out.super.super;
  self->super.triggers_size = 1;

  self->super.children_size = 0;

  Reactor_ctor(&self->super, env, self, NULL, 0, self->_reactions, 1, self->_triggers, 1);

  InputPort_ctor(&self->in.super, &self->super, NULL, &self->in, NULL, 0);
  OutputPort_ctor(&self->out.super, &self->super, NULL, &self->out, self->down_streams_, 1);
  self->consumer_reaction.out = &self->out;
  self->consumer_reaction.in = &self->in;

  ConsumerReaction_0_ctor(&self->consumer_reaction, &self->super);
}

void ProducerReactor_ctor(ProducerReactor *self, Environment *env) {
  ConsumerReactor_ctor(&self->consumer_reactor, env);
  self->consumer_reactor.in.super.base.inward_binding = &self->producer.super.base;

  self->state = 0;

  self->children[0] = &self->consumer_reactor.super;

  self->_reactions[0] = &self->producer_reaction.super;
  self->_reactions[1] = &self->feedback_reaction.super;
  self->super.reactions_size = 2;

  self->_triggers[0] = &self->timer.super.super;
  self->_triggers[1] = &self->producer.super.super;
  self->super.triggers_size = 2;

  Reactor_ctor(&self->super, env, self, self->children, 1, self->_reactions, 2, self->_triggers, 2);

  OutputPort_ctor(&self->producer.super, &self->super, NULL, &self->producer, self->down_streams_, 1);
  InputPort_ctor(&self->feedback.super, &self->super, &self->consumer_reactor.out.super.base, &self->feedback, NULL, 0);

  ProducerReaction_0_ctor(&self->producer_reaction, &self->super);
  self->producer_reaction.produce = &self->producer;
  ProducerReaction_1_ctor(&self->feedback_reaction, &self->super);
  self->feedback_reaction.feedback = &self->feedback;

  Timer_ctor(&self->timer.super, &self->super, 0, SEC(1), self->timer.effects, 1);

  // effect configurations
  self->timer.super.super.register_effect(&self->timer.super.super, &self->producer_reaction.super);
  Trigger* trigger = &self->producer.super.super;
  trigger->register_effect(trigger, &self->consumer_reactor.consumer_reaction.super);
  self->consumer_reactor.out.super.super.register_effect(&self->consumer_reactor.out.super.super, &self->feedback_reaction.super);


  //self->consumer_reactor.in.super.base.inward_binding = &self->producer.super.base;
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
