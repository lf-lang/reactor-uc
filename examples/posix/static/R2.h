#ifndef LFC_GEN_R2_H
#define LFC_GEN_R2_H

#include "reactor-uc/reactor-uc.h"
#include "_lf_preamble.h"

// Include instantiated reactors
// Reaction structs
LF_DEFINE_REACTION_STRUCT(Reactor_R2, reaction0, 0);
LF_DEFINE_REACTION_STRUCT(Reactor_R2, reaction1, 0);
LF_DEFINE_REACTION_STRUCT(Reactor_R2, reaction2, 0);
// Timer structs 

LF_DEFINE_STARTUP_STRUCT(Reactor_R2, 1, 0);
LF_DEFINE_SHUTDOWN_STRUCT(Reactor_R2, 1, 0);
// Port structs
LF_DEFINE_INPUT_STRUCT(Reactor_R2, in, 1, 0, int, 0);
// Connection structs

//The reactor self struct
typedef struct {
  Reactor super;
  // Child reactor fields
  
  LF_REACTION_INSTANCE(Reactor_R2, reaction0);
  LF_REACTION_INSTANCE(Reactor_R2, reaction1);
  LF_REACTION_INSTANCE(Reactor_R2, reaction2);
  // Timers
  
  // Actions and builtin triggers
  
  LF_STARTUP_INSTANCE(Reactor_R2);LF_SHUTDOWN_INSTANCE(Reactor_R2);
  // Connections 
  
  // Ports 
  LF_PORT_INSTANCE(Reactor_R2, in, 1);
  // State variables 
  int cnt;
  // Reactor parameters
  int expected;
  LF_REACTOR_BOOKKEEPING_INSTANCES(3, 3, 0);
} Reactor_R2;

LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Reactor_R2 , InputExternalCtorArgs *_in_external , int expected );

#endif // LFC_GEN_R2_H
