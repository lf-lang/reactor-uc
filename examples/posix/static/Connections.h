#ifndef LFC_GEN_CONNECTIONS_H
#define LFC_GEN_CONNECTIONS_H

#include "reactor-uc/reactor-uc.h"
#include "_lf_preamble.h"

// Include instantiated reactors
#include "R1.h" 
#include "R2.h" 
// Reaction structs

// Timer structs 

        
// Port structs

// Connection structs
LF_DEFINE_LOGICAL_CONNECTION_STRUCT(Reactor_Connections,  conn_r1_out_0, 1);
LF_DEFINE_DELAYED_CONNECTION_STRUCT(Reactor_Connections, conn_r3_out_1, 1, int, 1, MSEC(10));
//The reactor self struct
typedef struct {
  Reactor super;
  // Child reactor fields
  
  LF_CHILD_REACTOR_INSTANCE(Reactor_R1, r1, 1);
  
  LF_CHILD_OUTPUT_CONNECTIONS(r1, out, 1, 1, 1);
  LF_CHILD_OUTPUT_EFFECTS(r1, out, 1, 1, 0);
  LF_CHILD_OUTPUT_OBSERVERS(r1, out, 1, 1, 0);
  
  
  LF_CHILD_REACTOR_INSTANCE(Reactor_R2, r2, 1);
  
  
  LF_CHILD_INPUT_SOURCES(r2, in, 1, 1, 0);
  
  LF_CHILD_REACTOR_INSTANCE(Reactor_R1, r3, 1);
  
  LF_CHILD_OUTPUT_CONNECTIONS(r3, out, 1, 1, 1);
  LF_CHILD_OUTPUT_EFFECTS(r3, out, 1, 1, 0);
  LF_CHILD_OUTPUT_OBSERVERS(r3, out, 1, 1, 0);
  
  
  LF_CHILD_REACTOR_INSTANCE(Reactor_R2, r4, 1);
  
  
  LF_CHILD_INPUT_SOURCES(r4, in, 1, 1, 0);
        
  // Timers
  
  // Actions and builtin triggers
  
  // Connections 
  LF_LOGICAL_CONNECTION_INSTANCE(Reactor_Connections, conn_r1_out_0, 1, 1);
  LF_DELAYED_CONNECTION_INSTANCE(Reactor_Connections, conn_r3_out_1, 1, 1);
  // Ports 
  
  // State variables 
  // Reactor parameters
  
  LF_REACTOR_BOOKKEEPING_INSTANCES(0, 0, 4);
} Reactor_Connections;

LF_REACTOR_CTOR_SIGNATURE(Reactor_Connections);

#endif // LFC_GEN_CONNECTIONS_H
