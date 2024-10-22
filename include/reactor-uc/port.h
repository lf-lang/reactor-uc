#ifndef REACTOR_UC_PORT_H
#define REACTOR_UC_PORT_H

#include "reactor-uc/connection.h"
#include "reactor-uc/reaction.h"
#include "reactor-uc/reactor.h"
#include "reactor-uc/trigger.h"

typedef struct Input Input;
typedef struct Output Output;
typedef struct Connection Connection;
typedef struct Port Port;

// Abstract Port type, inherits from Trigger
struct Port {
  Trigger super;
  Connection *conn_in;         // Connection coming into the port.
  Connection **conns_out;      // Connections going out of the port.
  size_t conns_out_size;       // Number of connections going out of the port.
  size_t conns_out_registered; // Number of connections that have been registered for cleanup.
};

// Input port. In the user-defined derived struct there must be a `buffer` field for storing the values.
struct Input {
  Port super;
  TriggerEffects effects; // The reactions triggered by this Input port.
  void *value_ptr;        // Pointer to the `buffer` field in the user Input port struct.
  size_t value_size;      // Size of the data stored in this Input Port.
};

// Output ports do not have any buffers.
struct Output {
  Port super;
  TriggerSources sources; // The reactions that can write to this Output port.
};

/**
 * @brief Create a new Input port.
 *
 * @param self Pointer to an allocated Input struct.
 * @param parent The parent Reactor.
 * @param effects Pointer to an array of Reaction pointers that can be used to store the effects of this port.
 * @param effects_size The size of the effects array.
 * @param value_ptr A pointer to where the data of this input port is stored. This should be a field in the user-defined
 * struct which is derived from Input.
 * @param value_size The size of the data stored in this Input port.
 */
void Input_ctor(Input *self, Reactor *parent, Reaction **effects, size_t effects_size, Connection **conns_out,
                size_t conns_out_size, void *value_ptr, size_t value_size);

/**
 * @brief Create a new Output port.
 *
 * @param self Pointer to an allocated Output struct.
 * @param parent The parent Reactor of this Output port.
 * @param sources Pointer to an array of Reaction pointers that can be used to store the sources of this port.
 * @param sources_size The size of the sources array.
 */
void Output_ctor(Output *self, Reactor *parent, Reaction **sources, size_t sources_size, Connection **conns_out,
                 size_t conns_out_size);

/**
 * @brief Create a new Port object. A Port is an abstract type that can be either an Input or an Output.
 *
 * @param self The Port object to construct.
 * @param type The type of the trigger. Can be either Input or Output.
 * @param parent The parent Reactor of this Port.
 * @param prepare The prepare function of the Port. This function is called at the beginning of a timestep before
 * and reaction triggered by the port is executed.
 * @param cleanup The cleanup function of the Port. This function is called at the end of a timestep after all reactions
 * that either write to or read from the port have been executed.
 * @param get The get function of the Port. This function is called to return the value of an input port. An output
 * port should just pass NULL.
 */
void Port_ctor(Port *self, TriggerType type, Reactor *parent, Connection **conns_out, size_t conns_out_size,
               void (*prepare)(Trigger *), void (*cleanup)(Trigger *), const void *(*get)(Trigger *));

#endif
