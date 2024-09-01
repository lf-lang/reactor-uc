#include "reactor-uc/environment.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/timer.h"
#include "reactor-uc/port.h"
#include <stdio.h>

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
};

// reaction bodies

int Producer_0_body(Reaction *untyped_reaction) {
  ProducerReaction *reaction = (ProducerReaction *)untyped_reaction;
  struct ProducerReactor *self = (struct ProducerReactor*) untyped_reaction->parent;

  // Begin User Code
  int alpha = 465;
  self->state = (self->state + alpha) % 1000;
  printf("Producing Value: %d\n", self->state);

  // set macro
  self->producer.value = self->state;
  self->producer.super.trigger_reactions(&self->producer.super);
  // end set macro

  // End User Code

  return 0;
}


int Producer_1_body(Reaction *untyped_reaction) {
  FeedbackReaction * reaction = (FeedbackReaction *)untyped_reaction;
  struct ProducerReactor *self = (struct ProducerReactor*) untyped_reaction->parent;

  // Begin User Code

  // start get macro
  int value = *((int*) self->feedback.super.value_ptr);
  // end get macro

  printf("Producing Value: %d\n", value);

  // End User Code

  return 0;
}

void ProducerReaction_0_ctor(ProducerReaction *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, Producer_0_body);
  self->super.level = 0;
}


void ProducerReaction_1_ctor(FeedbackReaction* self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, Producer_1_body);
  self->super.level = 1;
}


void ConsumerReaction_0_ctor(ConsumerReaction* self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, Producer_1_body);
  self->super.level = 0;
}

void ConsumerReactor_ctor(struct ConsumerReactor* self, Environment *env) {
  self->_reactions[0] = &self->consumer_reaction.super;

  self->_triggers[0] = &self->out.super.super;

  Reactor_ctor(&self->super, env, NULL, 0, self->_reactions, 1, self->_triggers, 1);

  ConsumerReaction_0_ctor(&self->consumer_reaction, &self->super);
}

void ProducerReactor_ctor(struct ProducerReactor *self, Environment *env) {
  ConsumerReactor_ctor(&self->consumer_reactor, env);

  self->_reactions[0] = &self->producer_reaction.super;
  self->_reactions[1] = &self->feedback_reaction.super;

  self->_triggers[0] = (Trigger *)&self->timer;
  self->_triggers[1] = &self->producer.super.super;

  Reactor_ctor(&self->super, env, NULL, 0, self->_reactions, 2, self->_triggers, 2);

  
  OutputPort_ctor(&self->producer, self->super, )

  ProducerReaction_0_ctor(&self->producer_reaction, &self->super);
  ProducerReaction_1_ctor(&self->feedback_reaction, &self->super);

  Timer_ctor(&self->timer.super, &self->super, 0, SEC(1), self->timer.effects, 1);

  // effect configurations
  self->timer.super.super.register_effect(&self->timer.super.super, &self->producer_reaction.super);
  Trigger* trigger = &self->producer.super.super;
  trigger->register_effect(trigger, &self->consumer_reactor.consumer_reaction.super);
  self->consumer_reactor.out.super.super.register_effect(&self->consumer_reactor.out.super.super, &self->feedback_reaction.super);
}

int main() {
  struct ProducerReactor producer_reactor;
  Environment env;
  Environment_ctor(&env, (Reactor *)&producer_reactor);
  ProducerReactor_ctor(&producer_reactor, &env);
  env.assemble(&env);
  env.start(&env);
}
