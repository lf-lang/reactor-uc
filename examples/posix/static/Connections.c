#include "Connections.h"
// Private preambles

// Reaction bodies

// Reaction deadline handlers

// Reaction constructors

        
// Timer constructors 

// Port constructors

// Connection constructors
LF_DEFINE_LOGICAL_CONNECTION_CTOR(Reactor_Connections,  conn_r1_out_0, 1);
LF_DEFINE_DELAYED_CONNECTION_CTOR(Reactor_Connections, conn_r3_out_1, 1, int, 1, MSEC(10), false);
LF_REACTOR_CTOR_SIGNATURE(Reactor_Connections) {
   LF_REACTOR_CTOR_PREAMBLE();
   LF_REACTOR_CTOR(Reactor_Connections);
   // Initialize Reactor parameters
   
   // Initialize State variables 
   LF_DEFINE_CHILD_OUTPUT_ARGS(r1, out, 1, 1);
   
   LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Reactor_R1, r1, 1 , _r1_out_args[i] );
   
   
   LF_DEFINE_CHILD_INPUT_ARGS(r2, in, 1, 1);
   LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Reactor_R2, r2, 1 , _r2_in_args[i] , 11);
   
   LF_DEFINE_CHILD_OUTPUT_ARGS(r3, out, 1, 1);
   
   LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Reactor_R1, r3, 1 , _r3_out_args[i] );
   
   
   LF_DEFINE_CHILD_INPUT_ARGS(r4, in, 1, 1);
   LF_INITIALIZE_CHILD_REACTOR_WITH_PARAMETERS(Reactor_R2, r4, 1 , _r4_in_args[i] , 10);
   // Initialize Timers
   // Initialize actions and builtin triggers
   
   // Initialize ports
   
   // Initialize connections
   LF_INITIALIZE_LOGICAL_CONNECTION(Reactor_Connections, conn_r1_out_0, 1, 1);
   LF_INITIALIZE_DELAYED_CONNECTION(Reactor_Connections, conn_r3_out_1, 1, 1);
   // Do connections 
   lf_connect((Connection *) &self->conn_r1_out_0[0][0], (Port *) &self->r1[0].out[0], (Port *) &self->r2[0].in[0]);
   lf_connect((Connection *) &self->conn_r3_out_1[0][0], (Port *) &self->r3[0].out[0], (Port *) &self->r4[0].in[0]);
   // Initialize Reactions 
}
