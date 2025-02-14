#include "R1.h"
// Private preambles

// Reaction bodies
LF_DEFINE_REACTION_BODY(Reactor_R1, reaction0) {
  // Bring self struct, environment, triggers, effects and sources into scope.
    LF_SCOPE_SELF(Reactor_R1);
    LF_SCOPE_ENV();
    LF_SCOPE_STARTUP(Reactor_R1);
  // Start of user-witten reaction deadline handler
  printf("Startup!\n");
}
LF_DEFINE_REACTION_BODY(Reactor_R1, reaction1) {
  // Bring self struct, environment, triggers, effects and sources into scope.
    LF_SCOPE_SELF(Reactor_R1);
    LF_SCOPE_ENV();
    LF_SCOPE_TIMER(Reactor_R1, t);
    LF_SCOPE_PORT(Reactor_R1, out);
  // Start of user-witten reaction deadline handler
  printf("Hello from R1 at %ld\n", env->get_elapsed_logical_time(env));
  lf_set(out, self->cnt++);
}
// Reaction deadline handlers

// Reaction constructors
LF_DEFINE_REACTION_CTOR(Reactor_R1, reaction0, 0 );
LF_DEFINE_REACTION_CTOR(Reactor_R1, reaction1, 1 );
LF_DEFINE_STARTUP_CTOR(Reactor_R1);
// Timer constructors 
LF_DEFINE_TIMER_CTOR(Reactor_R1, t, 1, 0);
// Port constructors
LF_DEFINE_OUTPUT_CTOR(Reactor_R1, out, 1);
// Connection constructors

LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Reactor_R1 , OutputExternalCtorArgs *_out_external  ) {
   LF_REACTOR_CTOR_PREAMBLE();
   LF_REACTOR_CTOR(Reactor_R1);
   // Initialize Reactor parameters
   
   // Initialize State variables 
   self->cnt = 0;
        
   // Initialize Timers
   LF_INITIALIZE_TIMER(Reactor_R1, t, 0, MSEC(10));
   // Initialize actions and builtin triggers
   
   LF_INITIALIZE_STARTUP(Reactor_R1);
   // Initialize ports
   LF_INITIALIZE_OUTPUT(Reactor_R1, out, 1, _out_external);
   // Initialize connections
   
   // Do connections 
   
   // Initialize Reactions 
   LF_INITIALIZE_REACTION(Reactor_R1, reaction0);
     LF_STARTUP_REGISTER_EFFECT(self->reaction0);
           
           
   LF_INITIALIZE_REACTION(Reactor_R1, reaction1);
     LF_TIMER_REGISTER_EFFECT(self->t, self->reaction1);
     LF_PORT_REGISTER_SOURCE(self->out, self->reaction1, 1);
}
