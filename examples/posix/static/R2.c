#include "R2.h"
// Private preambles

// Reaction bodies
LF_DEFINE_REACTION_BODY(Reactor_R2, reaction0) {
  // Bring self struct, environment, triggers, effects and sources into scope.
    LF_SCOPE_SELF(Reactor_R2);
    LF_SCOPE_ENV();
    LF_SCOPE_STARTUP(Reactor_R2);
  // Start of user-witten reaction deadline handler
  printf("Hi from other guy at startup\n");
}
LF_DEFINE_REACTION_BODY(Reactor_R2, reaction1) {
  // Bring self struct, environment, triggers, effects and sources into scope.
    LF_SCOPE_SELF(Reactor_R2);
    LF_SCOPE_ENV();
    LF_SCOPE_PORT(Reactor_R2, in);
  // Start of user-witten reaction deadline handler
  printf("Got %d\n", in->value);
  self->cnt++;
}
LF_DEFINE_REACTION_BODY(Reactor_R2, reaction2) {
  // Bring self struct, environment, triggers, effects and sources into scope.
    LF_SCOPE_SELF(Reactor_R2);
    LF_SCOPE_ENV();
    LF_SCOPE_SHUTDOWN(Reactor_R2);
  // Start of user-witten reaction deadline handler
  validate(self->cnt == self->expected);
}
// Reaction deadline handlers

// Reaction constructors
LF_DEFINE_REACTION_CTOR(Reactor_R2, reaction0, 0 );
LF_DEFINE_REACTION_CTOR(Reactor_R2, reaction1, 1 );
LF_DEFINE_REACTION_CTOR(Reactor_R2, reaction2, 2 );
LF_DEFINE_STARTUP_CTOR(Reactor_R2);
LF_DEFINE_SHUTDOWN_CTOR(Reactor_R2);
// Timer constructors 

// Port constructors
LF_DEFINE_INPUT_CTOR(Reactor_R2, in, 1, 0, int, 0);
// Connection constructors

LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Reactor_R2 , InputExternalCtorArgs *_in_external , int expected ) {
   LF_REACTOR_CTOR_PREAMBLE();
   LF_REACTOR_CTOR(Reactor_R2);
   // Initialize Reactor parameters
   
   self->expected = expected;
   // Initialize State variables 
   self->cnt = 0;
        
   // Initialize Timers
   // Initialize actions and builtin triggers
   
   LF_INITIALIZE_STARTUP(Reactor_R2);
   LF_INITIALIZE_SHUTDOWN(Reactor_R2);
   // Initialize ports
   LF_INITIALIZE_INPUT(Reactor_R2, in, 1, _in_external);
   // Initialize connections
   
   // Do connections 
   
   // Initialize Reactions 
   LF_INITIALIZE_REACTION(Reactor_R2, reaction0);
     LF_STARTUP_REGISTER_EFFECT(self->reaction0);
           
           
   LF_INITIALIZE_REACTION(Reactor_R2, reaction1);
     LF_PORT_REGISTER_EFFECT(self->in, self->reaction1, 1);
           
           
   LF_INITIALIZE_REACTION(Reactor_R2, reaction2);
     LF_SHUTDOWN_REGISTER_EFFECT(self->reaction2);
           
}
