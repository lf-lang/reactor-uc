\page api-docs Reaction API

This page documents the functions and macros available to use within reaction bodies,
deadline violation handlers and STP violation handlers. A pointer to the \ref
Environment is defined as `env` within the scope of the reactions. This enables us to
access the environment API as follows: 

\code{.c} 
reaction(t) {= 
  instant_t now = env->get_logical_time(env); 
=} 
\endcode

## Reading logical and physical time
\ref Environment.get_elapsed_logical_time \n
\ref Environment.get_logical_time \n
\ref Environment.get_elapsed_physical_time \n
\ref Environment.get_physical_time \n
\ref Environment.get_lag \n

## Requesting shutdown
\ref Environment.request_shutdown \n

## Setting and getting ports
\ref lf_set \n
\ref lf_set_array \n
\ref lf_get \n
\ref lf_is_present \n

## Scheduling actions
\ref lf_schedule \n
\ref lf_schedule_array \n

## Logging
The runtime does logging on a per-module basis, verbosity is controlled by compile
definitions. The different verbosity levels are \ref LF_LOG_LEVEL_OFF, \ref
LF_LOG_LEVEL_ERROR, \ref LF_LOG_LEVEL_WARN, \ref LF_LOG_LEVEL_INFO and \ref
LF_LOG_LEVEL_DEBUG. To configure the log verbosity level to DEBUG for all modules add
the following to your CMake:

\code{.cmake}
target_compile_definition(reactor-uc PUBLIC LF_LOG_LEVEL_ALL=LF_LOG_LEVEL_DEBUG)
\endcode

You can also set the log verbosity level on individual modules, e.g. to set only logging
related to the scheduler to DEBUG:

\code{.cmake}
target_compile_definition(reactor-uc PUBLIC LF_LOG_LEVEL_SCHED=LF_LOG_LEVEL_DEBUG)
\endcode

The different modules are \ref LF_LOG_LEVEL_ENV, \ref LF_LOG_LEVEL_SCHED, \ref
LF_LOG_LEVEL_QUEUE, \ref LF_LOG_LEVEL_FED, \ref LF_LOG_LEVEL_TRIG, \ref
LF_LOG_LEVEL_PLATFORM, \ref LF_LOG_LEVEL_CONN, and \ref LF_LOG_LEVEL_NET.

To disable all logging, \ref LF_LOG_DISABLE can be defined. The logs are by default
colorized, to disable this define \ref LF_COLORIZE_LOGS to 0. Timestamps are also added
to the log by default, to disable define \ref LF_TIMESTAMP_LOGS to 0.