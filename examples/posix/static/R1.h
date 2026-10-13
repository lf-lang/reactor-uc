#ifndef LFC_GEN_R1_H
#define LFC_GEN_R1_H

#include "reactor-uc/reactor-uc.h"
#include "_lf_preamble.h"

// Include instantiated reactors
// Reaction structs
LF_DEFINE_REACTION_STRUCT(Reactor_R1, reaction0, 0);
LF_DEFINE_REACTION_STRUCT(Reactor_R1, reaction1, 1);
// Timer structs 
LF_DEFINE_TIMER_STRUCT(Reactor_R1, t, 1, 0);
LF_DEFINE_STARTUP_STRUCT(Reactor_R1, 1, 0);
// Port structs
LF_DEFINE_OUTPUT_STRUCT(Reactor_R1, out, 1, int);
// Connection structs

//The reactor self struct
typedef struct {
  Reactor super;
  // Child reactor fields
  
  LF_REACTION_INSTANCE(Reactor_R1, reaction0);
  LF_REACTION_INSTANCE(Reactor_R1, reaction1);
  // Timers
  LF_TIMER_INSTANCE(Reactor_R1, t);
  // Actions and builtin triggers
  
  LF_STARTUP_INSTANCE(Reactor_R1);
  // Connections 
  
  // Ports 
  LF_PORT_INSTANCE(Reactor_R1, out, 1);
  // State variables 
  int cnt;
  // Reactor parameters
  
  LF_REACTOR_BOOKKEEPING_INSTANCES(2, 3, 0);
} Reactor_R1;

LF_REACTOR_CTOR_SIGNATURE_WITH_PARAMETERS(Reactor_R1 , OutputExternalCtorArgs *_out_external  );

#endif // LFC_GEN_R1_H
