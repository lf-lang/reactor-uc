\page api-docs API
# API docs

The following functions and macros are available to use within the LF
reaction bodies. A pointer to the \ref Environment is defined as `env`
within the scope of the reactions. This enables us to access the environment
API as follows:
\code{.c}
  reaction(t) {=
    instant_t now = env->get_logical_time(env);
  =}
\endcode


A set of macros are also defined which can be invoked directly



## Reading logical and physical time
\ref Environment.get_elapsed_logical_time \n
\ref Environment.get_logical_time \n
\ref Environment.get_elapsed_physical_time \n
\ref Environment.get_physical_time \n

## Requesting shutdown
\ref Environment.request_shutdown \n

## Setting and getting ports
The following macros are defined for interacting with input and output ports.
\ref lf_set \n
\ref lf_set_array \n
\ref lf_get \n
\ref lf_is_present \n

## Scheduling actions
\ref lf_schedule \n

